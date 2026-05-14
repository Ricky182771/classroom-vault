#include "AttachmentDownloader.hpp"

#include "DriveClient.hpp"
#include "FolderOrganizer.hpp"
#include "SyncStateManager.hpp"
#include "Utils.hpp"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QStringList>
#include <algorithm>

AttachmentDownloader::AttachmentDownloader(QObject *parent)
    : QObject(parent)
{}

void AttachmentDownloader::setDriveClient(DriveClient *driveClient)
{
    if (m_driveClient == driveClient) {
        return;
    }

    if (m_driveClient) {
        disconnect(m_driveClient, nullptr, this, nullptr);
    }

    m_driveClient = driveClient;

    if (m_driveClient) {
        connect(m_driveClient, &DriveClient::fileMetadataLoaded, this, &AttachmentDownloader::onFileMetadataLoaded);
        connect(m_driveClient, &DriveClient::downloadFinished, this, &AttachmentDownloader::onDownloadFinished);
        connect(m_driveClient, &DriveClient::downloadSkipped, this, &AttachmentDownloader::onDownloadSkipped);
        connect(m_driveClient, &DriveClient::requestFailed, this, &AttachmentDownloader::onRequestFailed);
    }
}

void AttachmentDownloader::setSyncStateManager(SyncStateManager *syncStateManager)
{
    m_syncStateManager = syncStateManager;
}

void AttachmentDownloader::setDriveDownloadsEnabled(bool enabled)
{
    m_driveDownloadsEnabled = enabled;
}

void AttachmentDownloader::downloadAttachmentsForAssignments(const QVector<Assignment> &assignments)
{
    m_queue.clear();
    m_hasCurrent = false;
    m_currentDriveMetadata = QJsonObject();
    m_currentDriveAction.clear();
    m_currentDriveExportMimeType.clear();

    m_currentIndex = 0;
    m_total = 0;
    m_blobDownloaded = 0;
    m_workspaceExported = 0;
    m_linksSaved = 0;
    m_skipped = 0;
    m_errors = 0;

    emit attachmentCountersChanged(0, 0, 0, 0, 0);

    if (!m_syncStateManager) {
        emit attachmentLog(QStringLiteral("ERR   SyncStateManager no configurado para adjuntos."));
        emit attachmentFinished(0, 0, 1);
        return;
    }

    for (const Assignment &assignment : assignments) {
        if (assignment.materials.isEmpty()) {
            continue;
        }

        const QString path = assignmentFolderPath(assignment);
        if (path.isEmpty() || !QFileInfo::exists(path)) {
            continue;
        }

        for (const AssignmentMaterial &material : assignment.materials) {
            WorkItem item;
            item.assignment = assignment;
            item.material = material;
            m_queue.append(item);
        }
    }

    m_total = m_queue.size();
    if (m_total == 0) {
        emit attachmentLog(QStringLiteral("INFO  No hay adjuntos para procesar."));
        emit attachmentFinished(0, 0, 0);
        return;
    }

    emit attachmentLog(QStringLiteral("INFO  Procesando adjuntos..."));
    processNext();
}

QString AttachmentDownloader::assignmentFolderPath(const Assignment &assignment) const
{
    if (!m_syncStateManager) {
        return QString();
    }
    return m_syncStateManager->assignmentFolderPath(assignment.courseId, assignment.id).trimmed();
}

QString AttachmentDownloader::attachmentsDirPath(const Assignment &assignment) const
{
    const QString assignmentPath = assignmentFolderPath(assignment);
    if (assignmentPath.isEmpty()) {
        return QString();
    }
    return QDir(assignmentPath).filePath(QStringLiteral("Adjuntos"));
}

QString AttachmentDownloader::ensureAttachmentsDir(const Assignment &assignment, bool *ok) const
{
    if (ok) {
        *ok = false;
    }

    const QString path = attachmentsDirPath(assignment);
    if (path.isEmpty()) {
        return QString();
    }

    QDir dir;
    if (dir.exists(path) || dir.mkpath(path)) {
        if (ok) {
            *ok = true;
        }
        return path;
    }

    return QString();
}

QString AttachmentDownloader::materialDisplayName(const AssignmentMaterial &material) const
{
    if (!material.title.trimmed().isEmpty()) {
        return FolderOrganizer::sanitizeFileName(material.title);
    }
    if (!material.driveFileId.trimmed().isEmpty()) {
        return FolderOrganizer::sanitizeFileName(material.driveFileId);
    }

    if (material.type == QStringLiteral("youtubeVideo")) {
        return QStringLiteral("YouTube");
    }
    if (material.type == QStringLiteral("form")) {
        return QStringLiteral("Formulario");
    }
    if (material.type == QStringLiteral("link")) {
        return QStringLiteral("Enlace");
    }
    return QStringLiteral("Archivo de Drive");
}

QString AttachmentDownloader::preferredUrlForMaterial(const AssignmentMaterial &material, const QJsonObject &driveMetadata) const
{
    QString url = material.url.trimmed();
    if (url.isEmpty()) {
        url = material.alternateLink.trimmed();
    }
    if (url.isEmpty()) {
        url = driveMetadata.value(QStringLiteral("webViewLink")).toString().trimmed();
    }
    return url;
}

QString AttachmentDownloader::workspaceLogTag(const QString &sourceMimeType) const
{
    if (sourceMimeType == QStringLiteral("application/vnd.google-apps.document")) {
        return QStringLiteral("GDOC");
    }
    if (sourceMimeType == QStringLiteral("application/vnd.google-apps.spreadsheet")) {
        return QStringLiteral("GSHT");
    }
    if (sourceMimeType == QStringLiteral("application/vnd.google-apps.presentation")) {
        return QStringLiteral("GSLD");
    }
    if (sourceMimeType == QStringLiteral("application/vnd.google-apps.drawing")) {
        return QStringLiteral("GDRW");
    }
    return QStringLiteral("GAPP");
}

QString AttachmentDownloader::linkKey(const QString &url) const
{
    if (url.trimmed().isEmpty()) {
        return QStringLiteral("link-empty");
    }
    const QByteArray digest = QCryptographicHash::hash(url.trimmed().toUtf8(), QCryptographicHash::Sha256).toHex();
    return QStringLiteral("link-") + QString::fromUtf8(digest.left(16));
}

QString AttachmentDownloader::localFileMd5(const QString &path) const
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return QString();
    }

    QCryptographicHash hash(QCryptographicHash::Md5);
    if (!hash.addData(&file)) {
        file.close();
        return QString();
    }

    file.close();
    return QString::fromUtf8(hash.result().toHex());
}

bool AttachmentDownloader::createUrlFile(
    const QString &destinationDir,
    const QString &title,
    const QString &url,
    QString *savedPath,
    bool *unchanged)
{
    if (savedPath) {
        savedPath->clear();
    }
    if (unchanged) {
        *unchanged = false;
    }

    if (destinationDir.trimmed().isEmpty() || url.trimmed().isEmpty()) {
        return false;
    }

    QDir dir;
    if (!dir.exists(destinationDir) && !dir.mkpath(destinationDir)) {
        return false;
    }

    const QString safeTitle = FolderOrganizer::sanitizeFileName(title.trimmed().isEmpty() ? QStringLiteral("Enlace") : title);
    const QString fileName = safeTitle + QStringLiteral(".url");
    const QString localPath = QDir(destinationDir).filePath(fileName);
    const QString content = QStringLiteral("[InternetShortcut]\nURL=%1\n").arg(url.trimmed());

    QFile file(localPath);
    if (file.exists()) {
        if (!file.open(QIODevice::ReadOnly)) {
            return false;
        }
        const QString currentContent = QString::fromUtf8(file.readAll());
        file.close();
        if (currentContent == content) {
            if (savedPath) {
                *savedPath = localPath;
            }
            if (unchanged) {
                *unchanged = true;
            }
            return true;
        }
    }

    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }

    if (file.write(content.toUtf8()) < 0) {
        file.close();
        return false;
    }
    file.close();

    if (savedPath) {
        *savedPath = localPath;
    }
    return true;
}

bool AttachmentDownloader::saveLinkForCurrentMaterial(
    const QString &prefix,
    const QString &status,
    const QString &url,
    const QString &key,
    bool *unchanged,
    const QString &sourceMimeType)
{
    if (unchanged) {
        *unchanged = false;
    }

    if (url.trimmed().isEmpty()) {
        return false;
    }

    bool ok = false;
    const QString attachmentsDir = ensureAttachmentsDir(m_current.assignment, &ok);
    if (!ok) {
        return false;
    }

    const QString name = materialDisplayName(m_current.material);
    const QString title = QStringLiteral("%1 - %2").arg(prefix, name.isEmpty() ? prefix : name);

    QString savedPath;
    bool localUnchanged = false;
    if (!createUrlFile(attachmentsDir, title, url, &savedPath, &localUnchanged)) {
        return false;
    }

    QJsonObject entry;
    entry.insert(QStringLiteral("type"), m_current.material.type);
    entry.insert(QStringLiteral("status"), status);
    entry.insert(QStringLiteral("title"), name);
    entry.insert(QStringLiteral("url"), url);
    entry.insert(QStringLiteral("localPath"), savedPath);
    entry.insert(QStringLiteral("localFileName"), QFileInfo(savedPath).fileName());
    if (!sourceMimeType.trimmed().isEmpty()) {
        entry.insert(QStringLiteral("sourceMimeType"), sourceMimeType.trimmed());
    }
    if (!m_current.material.driveFileId.trimmed().isEmpty()) {
        entry.insert(QStringLiteral("driveFileId"), m_current.material.driveFileId.trimmed());
    }
    entry.insert(QStringLiteral("lastSaved"), Utils::nowIsoStringUtc());

    m_syncStateManager->updateAttachment(
        m_current.assignment.courseId,
        m_current.assignment.id,
        key,
        entry);
    syncAssignmentMetadataAttachments(m_current.assignment);

    if (unchanged) {
        *unchanged = localUnchanged;
    }
    return true;
}

bool AttachmentDownloader::isUnchangedByState(
    const QJsonObject &existingRecord,
    const QString &localPath,
    const QString &remoteMd5,
    qint64 remoteSize,
    const QString &remoteModifiedTime) const
{
    if (localPath.trimmed().isEmpty() || !QFileInfo::exists(localPath)) {
        return false;
    }

    const QString stateLocalPath = existingRecord.value(QStringLiteral("localPath")).toString().trimmed();
    const QString stateModifiedTime = existingRecord.value(QStringLiteral("modifiedTime")).toString().trimmed();

    if (!stateLocalPath.isEmpty()
        && QFileInfo::exists(stateLocalPath)
        && !remoteModifiedTime.isEmpty()
        && stateModifiedTime == remoteModifiedTime
        && (stateLocalPath == localPath)) {
        return true;
    }

    if (!remoteMd5.isEmpty()) {
        const QString localMd5 = localFileMd5(localPath);
        return !localMd5.isEmpty() && (localMd5.compare(remoteMd5, Qt::CaseInsensitive) == 0);
    }

    if (remoteSize > 0) {
        const QFileInfo info(localPath);
        return info.size() == remoteSize;
    }

    if (!remoteModifiedTime.isEmpty() && stateModifiedTime == remoteModifiedTime) {
        return true;
    }

    return false;
}

QJsonObject AttachmentDownloader::buildDriveAttachmentEntry(
    const QString &status,
    const QString &fileId,
    const QString &localPath,
    const QJsonObject &metadata,
    const QString &exportMimeType) const
{
    QJsonObject entry;
    entry.insert(QStringLiteral("type"), QStringLiteral("driveFile"));
    entry.insert(QStringLiteral("status"), status);
    entry.insert(QStringLiteral("driveFileId"), fileId);
    entry.insert(QStringLiteral("name"), metadata.value(QStringLiteral("name")).toString());
    entry.insert(QStringLiteral("sourceMimeType"), metadata.value(QStringLiteral("mimeType")).toString());
    if (!exportMimeType.isEmpty()) {
        entry.insert(QStringLiteral("exportMimeType"), exportMimeType);
    }
    entry.insert(QStringLiteral("webViewLink"), metadata.value(QStringLiteral("webViewLink")).toString());
    entry.insert(QStringLiteral("localPath"), localPath);
    entry.insert(QStringLiteral("localFileName"), QFileInfo(localPath).fileName());
    if (metadata.contains(QStringLiteral("size"))) {
        qint64 size = 0;
        const QJsonValue sizeValue = metadata.value(QStringLiteral("size"));
        if (sizeValue.isString()) {
            size = sizeValue.toString().toLongLong();
        } else if (sizeValue.isDouble()) {
            size = static_cast<qint64>(sizeValue.toDouble());
        }
        if (size > 0) {
            entry.insert(QStringLiteral("size"), size);
        }
    }
    if (metadata.contains(QStringLiteral("md5Checksum"))) {
        const QString md5 = metadata.value(QStringLiteral("md5Checksum")).toString();
        if (!md5.isEmpty()) {
            entry.insert(QStringLiteral("md5Checksum"), md5);
        }
    }
    if (metadata.contains(QStringLiteral("modifiedTime"))) {
        const QString modified = metadata.value(QStringLiteral("modifiedTime")).toString();
        if (!modified.isEmpty()) {
            entry.insert(QStringLiteral("modifiedTime"), modified);
        }
    }
    entry.insert(QStringLiteral("lastDownloaded"), Utils::nowIsoStringUtc());
    return entry;
}

void AttachmentDownloader::syncAssignmentMetadataAttachments(const Assignment &assignment) const
{
    if (!m_syncStateManager) {
        return;
    }

    const QString assignmentPath = assignmentFolderPath(assignment);
    if (assignmentPath.isEmpty()) {
        return;
    }

    const QString metadataPath = QDir(assignmentPath).filePath(QStringLiteral("metadata.json"));
    QFile file(metadataPath);
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
        return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    if (!doc.isObject()) {
        return;
    }

    QJsonObject metadata = doc.object();
    const QJsonObject attachmentsObj = m_syncStateManager->assignmentAttachments(assignment.courseId, assignment.id);
    QStringList keys = attachmentsObj.keys();
    std::sort(keys.begin(), keys.end());

    QJsonArray attachmentsArray;
    for (const QString &key : keys) {
        const QJsonObject value = attachmentsObj.value(key).toObject();
        if (!value.isEmpty()) {
            attachmentsArray.append(value);
        }
    }

    const QJsonArray existingArray = metadata.value(QStringLiteral("attachments")).toArray();
    if (existingArray == attachmentsArray) {
        return;
    }

    metadata.insert(QStringLiteral("attachments"), attachmentsArray);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return;
    }
    file.write(QJsonDocument(metadata).toJson(QJsonDocument::Indented));
    file.close();
}

void AttachmentDownloader::processNext()
{
    if (m_queue.isEmpty()) {
        if (m_syncStateManager) {
            m_syncStateManager->save();
        }

        emit attachmentCountersChanged(m_blobDownloaded, m_workspaceExported, m_linksSaved, m_skipped, m_errors);
        emit attachmentFinished(m_blobDownloaded + m_workspaceExported + m_linksSaved, m_skipped, m_errors);
        return;
    }

    m_current = m_queue.takeFirst();
    m_hasCurrent = true;
    m_currentDriveMetadata = QJsonObject();
    m_currentDriveAction.clear();
    m_currentDriveExportMimeType.clear();

    ++m_currentIndex;
    emit attachmentProgress(m_currentIndex, m_total);

    const AssignmentMaterial &material = m_current.material;

    if (material.type == QStringLiteral("link") || material.type == QStringLiteral("youtubeVideo") || material.type == QStringLiteral("form")) {
        const QString url = preferredUrlForMaterial(material);
        if (url.isEmpty()) {
            emit attachmentLog(QStringLiteral("ERR   Material sin URL utilizable: %1").arg(materialDisplayName(material)));
            finalizeCurrentItem(ResultKind::Error);
            return;
        }

        QString prefix = QStringLiteral("Enlace");
        if (material.type == QStringLiteral("youtubeVideo")) {
            prefix = QStringLiteral("YouTube");
        } else if (material.type == QStringLiteral("form")) {
            prefix = QStringLiteral("Formulario");
        }

        bool unchanged = false;
        const QString key = linkKey(url);
        if (!saveLinkForCurrentMaterial(prefix, QStringLiteral("saved_link"), url, key, &unchanged)) {
            emit attachmentLog(QStringLiteral("ERR   No se pudo guardar enlace: %1").arg(materialDisplayName(material)));
            finalizeCurrentItem(ResultKind::Error);
            return;
        }

        const QString fileName = FolderOrganizer::sanitizeFileName(QStringLiteral("%1 - %2").arg(prefix, materialDisplayName(material)))
            + QStringLiteral(".url");
        if (unchanged) {
            emit attachmentLog(QStringLiteral("SKIP  Ya existe sin cambios: %1").arg(fileName));
            finalizeCurrentItem(ResultKind::Skipped);
        } else {
            const QString logTag = (material.type == QStringLiteral("youtubeVideo")) ? QStringLiteral("YT")
                : (material.type == QStringLiteral("form"))                     ? QStringLiteral("FORM")
                                                                                 : QStringLiteral("LINK");
            emit attachmentLog(QStringLiteral("%1  Guardado enlace: %2").arg(logTag, fileName));
            finalizeCurrentItem(ResultKind::LinkSaved);
        }
        return;
    }

    if (material.type != QStringLiteral("driveFile")) {
        emit attachmentLog(QStringLiteral("SKIP  Material no soportado: %1").arg(material.type));
        finalizeCurrentItem(ResultKind::Skipped);
        return;
    }

    if (!m_driveDownloadsEnabled) {
        emit attachmentLog(QStringLiteral("SKIP  Adjuntos de Drive omitidos por permisos de scope."));
        finalizeCurrentItem(ResultKind::Skipped);
        return;
    }

    if (!m_driveClient) {
        emit attachmentLog(QStringLiteral("ERR   DriveClient no configurado."));
        finalizeCurrentItem(ResultKind::Error);
        return;
    }

    processCurrentDriveMaterial();
}

void AttachmentDownloader::processCurrentDriveMaterial()
{
    const QString fileId = m_current.material.driveFileId.trimmed();
    if (fileId.isEmpty()) {
        emit attachmentLog(QStringLiteral("ERR   Material driveFile sin fileId."));
        finalizeCurrentItem(ResultKind::Error);
        return;
    }

    emit attachmentLog(QStringLiteral("FILE  Consultando metadata: %1").arg(fileId));
    m_driveClient->fetchFileMetadata(fileId);
}

void AttachmentDownloader::onFileMetadataLoaded(const QString &fileId, const QJsonObject &metadata)
{
    if (!m_hasCurrent) {
        return;
    }

    if (m_current.material.driveFileId.trimmed() != fileId.trimmed()) {
        return;
    }

    m_currentDriveMetadata = metadata;
    m_currentDriveAction.clear();
    m_currentDriveExportMimeType.clear();

    const QString sourceMimeType = metadata.value(QStringLiteral("mimeType")).toString().trimmed();
    const QString driveName = metadata.value(QStringLiteral("name")).toString();
    const QString baseName = FolderOrganizer::sanitizeFileName(driveName.isEmpty() ? materialDisplayName(m_current.material) : driveName);

    const QJsonObject existingRecord = m_syncStateManager->attachmentRecord(
        m_current.assignment.courseId,
        m_current.assignment.id,
        fileId);
    const QString recordedLocalPath = existingRecord.value(QStringLiteral("localPath")).toString().trimmed();
    if (!recordedLocalPath.isEmpty() && !QFileInfo::exists(recordedLocalPath)) {
        emit attachmentLog(QStringLiteral("MISS  Adjunto faltante local: %1").arg(QFileInfo(recordedLocalPath).fileName()));
    }

    if (m_driveClient->isGoogleWorkspaceMimeType(sourceMimeType)) {
        if (sourceMimeType == QStringLiteral("application/vnd.google-apps.form")) {
            const QString formUrl = preferredUrlForMaterial(m_current.material, metadata);
            bool unchanged = false;
            if (!saveLinkForCurrentMaterial(
                    QStringLiteral("Formulario"),
                    QStringLiteral("saved_link"),
                    formUrl,
                    fileId,
                    &unchanged,
                    sourceMimeType)) {
                emit attachmentLog(QStringLiteral("ERR   No se pudo guardar formulario como enlace: %1").arg(baseName));
                finalizeCurrentItem(ResultKind::Error);
                return;
            }

            const QString fileName = FolderOrganizer::sanitizeFileName(QStringLiteral("Formulario - %1").arg(materialDisplayName(m_current.material)))
                + QStringLiteral(".url");
            if (unchanged) {
                emit attachmentLog(QStringLiteral("SKIP  Ya existe sin cambios: %1").arg(fileName));
                finalizeCurrentItem(ResultKind::Skipped);
            } else {
                emit attachmentLog(QStringLiteral("FORM  Guardado como enlace: %1").arg(fileName));
                finalizeCurrentItem(ResultKind::LinkSaved);
            }
            return;
        }

        if (!m_driveClient->isExportableWorkspaceMimeType(sourceMimeType)) {
            const QString fallbackUrl = preferredUrlForMaterial(m_current.material, metadata);
            bool unchanged = false;
            if (!saveLinkForCurrentMaterial(
                    QStringLiteral("Enlace"),
                    QStringLiteral("saved_link"),
                    fallbackUrl,
                    fileId,
                    &unchanged,
                    sourceMimeType)) {
                emit attachmentLog(QStringLiteral("ERR   Tipo Google Workspace no exportable y sin enlace utilizable: %1").arg(baseName));
                finalizeCurrentItem(ResultKind::Error);
                return;
            }

            const QString fileName = FolderOrganizer::sanitizeFileName(QStringLiteral("Enlace - %1").arg(materialDisplayName(m_current.material)))
                + QStringLiteral(".url");
            if (unchanged) {
                emit attachmentLog(QStringLiteral("SKIP  Ya existe sin cambios: %1").arg(fileName));
                finalizeCurrentItem(ResultKind::Skipped);
            } else {
                emit attachmentLog(QStringLiteral("LINK  Tipo Workspace no exportable, guardado como enlace: %1").arg(fileName));
                finalizeCurrentItem(ResultKind::LinkSaved);
            }
            return;
        }
    }

    QString outputFileName = baseName;
    QString exportMimeType;

    if (m_driveClient->isGoogleWorkspaceMimeType(sourceMimeType)) {
        exportMimeType = m_driveClient->exportMimeTypeFor(sourceMimeType);
        const QString extension = m_driveClient->extensionForExportedMimeType(sourceMimeType);
        if (!extension.isEmpty() && !outputFileName.endsWith(extension, Qt::CaseInsensitive)) {
            outputFileName += extension;
        }
    }

    QString localPath = existingRecord.value(QStringLiteral("localPath")).toString().trimmed();
    if (localPath.isEmpty()) {
        const QString dirPath = attachmentsDirPath(m_current.assignment);
        if (dirPath.isEmpty()) {
            emit attachmentLog(QStringLiteral("ERR   No se pudo resolver ruta de Adjuntos."));
            finalizeCurrentItem(ResultKind::Error);
            return;
        }
        localPath = QDir(dirPath).filePath(outputFileName);
    }

    const QString remoteMd5 = metadata.value(QStringLiteral("md5Checksum")).toString().trimmed();
    qint64 remoteSize = 0;
    const QJsonValue sizeValue = metadata.value(QStringLiteral("size"));
    if (sizeValue.isString()) {
        remoteSize = sizeValue.toString().toLongLong();
    } else if (sizeValue.isDouble()) {
        remoteSize = static_cast<qint64>(sizeValue.toDouble());
    }
    const QString remoteModifiedTime = metadata.value(QStringLiteral("modifiedTime")).toString().trimmed();

    if (isUnchangedByState(existingRecord, localPath, remoteMd5, remoteSize, remoteModifiedTime)) {
        const QString status = m_driveClient->isGoogleWorkspaceMimeType(sourceMimeType) ? QStringLiteral("exported") : QStringLiteral("downloaded");
        const QJsonObject entry = buildDriveAttachmentEntry(status, fileId, localPath, metadata, exportMimeType);
        m_syncStateManager->updateAttachment(m_current.assignment.courseId, m_current.assignment.id, fileId, entry);
        syncAssignmentMetadataAttachments(m_current.assignment);
        emit attachmentLog(QStringLiteral("SKIP  Ya existe sin cambios: %1").arg(QFileInfo(localPath).fileName()));
        finalizeCurrentItem(ResultKind::Skipped);
        return;
    }

    bool ok = false;
    const QString attachmentsDir = ensureAttachmentsDir(m_current.assignment, &ok);
    if (!ok) {
        emit attachmentLog(QStringLiteral("ERR   No se pudo crear carpeta Adjuntos."));
        finalizeCurrentItem(ResultKind::Error);
        return;
    }

    const QString localFileName = QFileInfo(localPath).fileName();
    if (localFileName.isEmpty()) {
        emit attachmentLog(QStringLiteral("ERR   Nombre de archivo invalido para adjunto de Drive."));
        finalizeCurrentItem(ResultKind::Error);
        return;
    }

    if (!m_driveClient->isGoogleWorkspaceMimeType(sourceMimeType)) {
        m_currentDriveAction = QStringLiteral("download");
        emit attachmentLog(QStringLiteral("FILE  Archivo normal detectado: %1").arg(localFileName));
        m_driveClient->downloadBlobFile(fileId, attachmentsDir, localFileName);
        return;
    }

    m_currentDriveAction = QStringLiteral("export");
    m_currentDriveExportMimeType = exportMimeType;

    emit attachmentLog(QStringLiteral("%1  Exportando: %2").arg(workspaceLogTag(sourceMimeType), baseName));
    m_driveClient->exportWorkspaceFile(fileId, sourceMimeType, attachmentsDir, localFileName);
}

void AttachmentDownloader::onDownloadFinished(const QString &fileId, const QString &savedPath)
{
    if (!m_hasCurrent) {
        return;
    }

    if (m_current.material.driveFileId.trimmed() != fileId.trimmed()) {
        return;
    }

    const bool wasExport = (m_currentDriveAction == QStringLiteral("export"));
    const QString status = wasExport ? QStringLiteral("exported") : QStringLiteral("downloaded");

    const QJsonObject entry = buildDriveAttachmentEntry(
        status,
        fileId,
        savedPath,
        m_currentDriveMetadata,
        wasExport ? m_currentDriveExportMimeType : QString());

    m_syncStateManager->updateAttachment(m_current.assignment.courseId, m_current.assignment.id, fileId, entry);
    syncAssignmentMetadataAttachments(m_current.assignment);

    if (wasExport) {
        emit attachmentLog(QStringLiteral("SAVE  Exportado: %1").arg(savedPath));
        finalizeCurrentItem(ResultKind::WorkspaceExported);
    } else {
        emit attachmentLog(QStringLiteral("SAVE  Descargado: %1").arg(savedPath));
        finalizeCurrentItem(ResultKind::BlobDownloaded);
    }
}

void AttachmentDownloader::onDownloadSkipped(const QString &fileId, const QString &reason)
{
    if (!m_hasCurrent) {
        return;
    }

    if (m_current.material.driveFileId.trimmed() != fileId.trimmed()) {
        return;
    }

    emit attachmentLog(QStringLiteral("SKIP  %1").arg(reason));
    finalizeCurrentItem(ResultKind::Skipped);
}

void AttachmentDownloader::onRequestFailed(const QString &context, int httpStatus, const QString &errorMessage)
{
    if (!m_hasCurrent) {
        return;
    }

    QString msg = errorMessage.trimmed();
    if (httpStatus == 401) {
        msg = QStringLiteral("Token invalido o expirado. Vuelve a iniciar sesion.");
    } else if (httpStatus == 403) {
        msg = QStringLiteral("No tienes permiso para leer este archivo de Drive o falta el scope drive.readonly.");
    } else if (httpStatus == 404) {
        msg = QStringLiteral("Archivo no encontrado en Drive (404).");
    }

    if (context.startsWith(QStringLiteral("drive.export"))) {
        const QString fallbackUrl = preferredUrlForMaterial(m_current.material, m_currentDriveMetadata);
        bool unchanged = false;
        if (!fallbackUrl.isEmpty()
            && saveLinkForCurrentMaterial(
                QStringLiteral("Enlace"),
                QStringLiteral("saved_link"),
                fallbackUrl,
                m_current.material.driveFileId.trimmed(),
                &unchanged,
                m_currentDriveMetadata.value(QStringLiteral("mimeType")).toString())) {
            emit attachmentLog(QStringLiteral("ERR   Fallo exportacion (%1). Se guardo enlace de respaldo.").arg(msg));
            if (unchanged) {
                finalizeCurrentItem(ResultKind::Skipped);
            } else {
                finalizeCurrentItem(ResultKind::LinkSaved);
            }
            return;
        }
    }

    emit attachmentLog(QStringLiteral("ERR   No se pudo procesar adjunto: %1").arg(msg));
    finalizeCurrentItem(ResultKind::Error);
}

void AttachmentDownloader::finalizeCurrentItem(ResultKind result)
{
    switch (result) {
    case ResultKind::BlobDownloaded:
        ++m_blobDownloaded;
        break;
    case ResultKind::WorkspaceExported:
        ++m_workspaceExported;
        break;
    case ResultKind::LinkSaved:
        ++m_linksSaved;
        break;
    case ResultKind::Skipped:
        ++m_skipped;
        break;
    case ResultKind::Error:
        ++m_errors;
        break;
    }

    emit attachmentCountersChanged(m_blobDownloaded, m_workspaceExported, m_linksSaved, m_skipped, m_errors);

    m_hasCurrent = false;
    m_currentDriveMetadata = QJsonObject();
    m_currentDriveAction.clear();
    m_currentDriveExportMimeType.clear();
    processNext();
}
