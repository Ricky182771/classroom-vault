#include "AttachmentDownloader.hpp"

#include "DriveClient.hpp"
#include "FolderOrganizer.hpp"
#include "SyncStateManager.hpp"
#include "Utils.hpp"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonObject>

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

    m_currentIndex = 0;
    m_total = 0;
    m_downloaded = 0;
    m_skipped = 0;
    m_errors = 0;

    if (!m_syncStateManager) {
        emit attachmentLog(QStringLiteral("ERR   SyncStateManager no configurado para adjuntos."));
        emit attachmentFinished(0, 0, 1);
        return;
    }

    for (const Assignment &assignment : assignments) {
        if (assignment.materials.isEmpty()) {
            continue;
        }

        const QString assignmentPath = m_syncStateManager->assignmentFolderPath(assignment.courseId, assignment.id);
        if (assignmentPath.trimmed().isEmpty() || !QFileInfo::exists(assignmentPath)) {
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

    emit attachmentLog(QStringLiteral("INFO  Descargando adjuntos..."));
    processNext();
}

QString AttachmentDownloader::buildAttachmentsDir(const Assignment &assignment, bool *ok) const
{
    if (ok) {
        *ok = false;
    }

    if (!m_syncStateManager) {
        return QString();
    }

    const QString assignmentPath = m_syncStateManager->assignmentFolderPath(assignment.courseId, assignment.id);
    if (assignmentPath.trimmed().isEmpty()) {
        return QString();
    }

    const QString attachmentsDir = QDir(assignmentPath).filePath(QStringLiteral("Adjuntos"));
    QDir dir;
    if (dir.exists(attachmentsDir) || dir.mkpath(attachmentsDir)) {
        if (ok) {
            *ok = true;
        }
        return attachmentsDir;
    }

    return QString();
}

QString AttachmentDownloader::materialDisplayName(const AssignmentMaterial &material) const
{
    const QString title = FolderOrganizer::sanitizeFileName(material.title);
    if (!title.isEmpty()) {
        return title;
    }
    if (!material.driveFileId.isEmpty()) {
        return material.driveFileId;
    }
    return QStringLiteral("material");
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

bool AttachmentDownloader::createUrlFile(const QString &destinationDir, const QString &title, const QString &url, bool *unchanged)
{
    if (unchanged) {
        *unchanged = false;
    }

    if (destinationDir.trimmed().isEmpty() || url.trimmed().isEmpty()) {
        return false;
    }

    const QString safeTitle = FolderOrganizer::sanitizeFileName(title.trimmed().isEmpty() ? QStringLiteral("Enlace") : title);
    const QString fileName = safeTitle + QStringLiteral(".url");
    const QString localPath = QDir(destinationDir).filePath(fileName);

    const QString content = QStringLiteral("[InternetShortcut]\nURL=%1\n").arg(url);

    QFile file(localPath);
    if (file.exists()) {
        if (!file.open(QIODevice::ReadOnly)) {
            return false;
        }
        const QString currentContent = QString::fromUtf8(file.readAll());
        file.close();
        if (currentContent == content) {
            if (unchanged) {
                *unchanged = true;
            }
            return true;
        }
    }

    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }

    file.write(content.toUtf8());
    file.close();
    return true;
}

void AttachmentDownloader::processNext()
{
    if (m_queue.isEmpty()) {
        if (m_syncStateManager) {
            m_syncStateManager->save();
        }
        emit attachmentFinished(m_downloaded, m_skipped, m_errors);
        return;
    }

    m_current = m_queue.takeFirst();
    m_hasCurrent = true;

    ++m_currentIndex;
    emit attachmentProgress(m_currentIndex, m_total);

    const AssignmentMaterial &material = m_current.material;

    if (material.type == QStringLiteral("link") || material.type == QStringLiteral("youtubeVideo") || material.type == QStringLiteral("form")) {
        bool ok = false;
        const QString attachmentsDir = buildAttachmentsDir(m_current.assignment, &ok);
        if (!ok) {
            emit attachmentLog(QStringLiteral("ERR   No se pudo crear carpeta Adjuntos para tarea %1")
                                   .arg(m_current.assignment.id));
            finalizeCurrentItem(false, false, true);
            return;
        }

        QString prefix = QStringLiteral("Enlace");
        if (material.type == QStringLiteral("youtubeVideo")) {
            prefix = QStringLiteral("YouTube");
        } else if (material.type == QStringLiteral("form")) {
            prefix = QStringLiteral("Formulario");
        }

        const QString title = prefix + QStringLiteral(" - ") + materialDisplayName(material);
        const QString url = !material.url.trimmed().isEmpty() ? material.url : material.alternateLink;

        bool unchanged = false;
        if (createUrlFile(attachmentsDir, title, url, &unchanged)) {
            const QString key = linkKey(url);
            QJsonObject entry;
            entry.insert(QStringLiteral("type"), material.type);
            entry.insert(QStringLiteral("title"), material.title);
            entry.insert(QStringLiteral("url"), url);
            entry.insert(QStringLiteral("localPath"), QDir(attachmentsDir).filePath(FolderOrganizer::sanitizeFileName(title) + QStringLiteral(".url")));
            entry.insert(QStringLiteral("lastSaved"), Utils::nowIsoStringUtc());
            m_syncStateManager->updateAttachment(m_current.assignment.courseId, m_current.assignment.id, key, entry);

            if (unchanged) {
                emit attachmentLog(QStringLiteral("SKIP  Ya existe sin cambios: %1").arg(FolderOrganizer::sanitizeFileName(title) + QStringLiteral(".url")));
                finalizeCurrentItem(false, true, false);
            } else {
                emit attachmentLog(QStringLiteral("LINK  Enlace guardado: %1").arg(FolderOrganizer::sanitizeFileName(title) + QStringLiteral(".url")));
                finalizeCurrentItem(true, false, false);
            }
        } else {
            emit attachmentLog(QStringLiteral("ERR   No se pudo guardar enlace: %1").arg(title));
            finalizeCurrentItem(false, false, true);
        }
        return;
    }

    if (material.type != QStringLiteral("driveFile")) {
        emit attachmentLog(QStringLiteral("SKIP  Material no soportado: %1").arg(material.type));
        finalizeCurrentItem(false, true, false);
        return;
    }

    if (!m_driveDownloadsEnabled) {
        emit attachmentLog(QStringLiteral("SKIP  Adjuntos de Drive omitidos por permisos de scope."));
        finalizeCurrentItem(false, true, false);
        return;
    }

    if (!m_driveClient) {
        emit attachmentLog(QStringLiteral("ERR   DriveClient no configurado."));
        finalizeCurrentItem(false, false, true);
        return;
    }

    processCurrentDriveMaterial();
}

void AttachmentDownloader::processCurrentDriveMaterial()
{
    const QString fileId = m_current.material.driveFileId.trimmed();
    if (fileId.isEmpty()) {
        emit attachmentLog(QStringLiteral("ERR   Material driveFile sin fileId."));
        finalizeCurrentItem(false, false, true);
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

    bool ok = false;
    const QString attachmentsDir = buildAttachmentsDir(m_current.assignment, &ok);
    if (!ok) {
        emit attachmentLog(QStringLiteral("ERR   No se pudo abrir carpeta Adjuntos."));
        finalizeCurrentItem(false, false, true);
        return;
    }

    const QString mimeType = metadata.value(QStringLiteral("mimeType")).toString();
    const QString driveName = metadata.value(QStringLiteral("name")).toString();
    const QString fileName = FolderOrganizer::sanitizeFileName(driveName.isEmpty() ? materialDisplayName(m_current.material) : driveName);
    const QString localPath = QDir(attachmentsDir).filePath(fileName);

    const QString md5 = metadata.value(QStringLiteral("md5Checksum")).toString();
    const qint64 remoteSize = metadata.value(QStringLiteral("size")).toString().toLongLong();

    if (QFileInfo::exists(localPath)) {
        const QFileInfo info(localPath);

        bool unchanged = false;
        if (!md5.isEmpty()) {
            const QString localMd5 = localFileMd5(localPath);
            unchanged = !localMd5.isEmpty() && (localMd5.compare(md5, Qt::CaseInsensitive) == 0);
        } else if (remoteSize > 0) {
            unchanged = (info.size() == remoteSize);
        }

        if (unchanged) {
            QJsonObject entry;
            entry.insert(QStringLiteral("type"), QStringLiteral("driveFile"));
            entry.insert(QStringLiteral("name"), fileName);
            entry.insert(QStringLiteral("mimeType"), mimeType);
            entry.insert(QStringLiteral("localPath"), localPath);
            if (!md5.isEmpty()) {
                entry.insert(QStringLiteral("md5Checksum"), md5);
            }
            if (remoteSize > 0) {
                entry.insert(QStringLiteral("size"), QString::number(remoteSize));
            }
            entry.insert(QStringLiteral("modifiedTime"), metadata.value(QStringLiteral("modifiedTime")).toString());
            entry.insert(QStringLiteral("lastDownloaded"), Utils::nowIsoStringUtc());
            m_syncStateManager->updateAttachment(m_current.assignment.courseId, m_current.assignment.id, fileId, entry);

            emit attachmentLog(QStringLiteral("SKIP  Ya existe sin cambios: %1").arg(fileName));
            finalizeCurrentItem(false, true, false);
            return;
        }
    }

    const bool isGoogleWorkspace = mimeType.startsWith(QStringLiteral("application/vnd.google-apps."));

    if (!isGoogleWorkspace) {
        m_driveClient->downloadFile(fileId, attachmentsDir, fileName);
        return;
    }

    if (mimeType == QStringLiteral("application/vnd.google-apps.form")) {
        QString formUrl = metadata.value(QStringLiteral("webViewLink")).toString().trimmed();
        if (formUrl.isEmpty()) {
            formUrl = !m_current.material.url.trimmed().isEmpty() ? m_current.material.url : m_current.material.alternateLink;
        }
        const QString title = QStringLiteral("Formulario - ") + fileName;
        bool unchanged = false;
        if (createUrlFile(attachmentsDir, title, formUrl, &unchanged)) {
            const QString key = linkKey(formUrl);
            QJsonObject entry;
            entry.insert(QStringLiteral("type"), QStringLiteral("form"));
            entry.insert(QStringLiteral("title"), fileName);
            entry.insert(QStringLiteral("url"), formUrl);
            entry.insert(QStringLiteral("localPath"), QDir(attachmentsDir).filePath(FolderOrganizer::sanitizeFileName(title) + QStringLiteral(".url")));
            entry.insert(QStringLiteral("lastSaved"), Utils::nowIsoStringUtc());
            m_syncStateManager->updateAttachment(m_current.assignment.courseId, m_current.assignment.id, key, entry);

            if (unchanged) {
                emit attachmentLog(QStringLiteral("SKIP  Ya existe sin cambios: %1").arg(FolderOrganizer::sanitizeFileName(title) + QStringLiteral(".url")));
                finalizeCurrentItem(false, true, false);
            } else {
                emit attachmentLog(QStringLiteral("LINK  Formulario guardado como enlace: %1").arg(fileName));
                finalizeCurrentItem(true, false, false);
            }
        } else {
            emit attachmentLog(QStringLiteral("ERR   No se pudo guardar enlace de formulario: %1").arg(fileName));
            finalizeCurrentItem(false, false, true);
        }
        return;
    }

    m_driveClient->exportGoogleWorkspaceFile(fileId, mimeType, attachmentsDir, fileName);
}

void AttachmentDownloader::onDownloadFinished(const QString &fileId, const QString &savedPath)
{
    if (!m_hasCurrent) {
        return;
    }

    if (m_current.material.driveFileId.trimmed() != fileId.trimmed()) {
        return;
    }

    QJsonObject entry;
    entry.insert(QStringLiteral("type"), QStringLiteral("driveFile"));
    entry.insert(QStringLiteral("id"), fileId);
    entry.insert(QStringLiteral("name"), QFileInfo(savedPath).fileName());
    entry.insert(QStringLiteral("mimeType"), m_currentDriveMetadata.value(QStringLiteral("mimeType")).toString());
    entry.insert(QStringLiteral("webViewLink"), m_currentDriveMetadata.value(QStringLiteral("webViewLink")).toString());
    entry.insert(QStringLiteral("localPath"), savedPath);
    if (m_currentDriveMetadata.contains(QStringLiteral("size"))) {
        entry.insert(QStringLiteral("size"), m_currentDriveMetadata.value(QStringLiteral("size")).toString());
    }
    if (m_currentDriveMetadata.contains(QStringLiteral("md5Checksum"))) {
        entry.insert(QStringLiteral("md5Checksum"), m_currentDriveMetadata.value(QStringLiteral("md5Checksum")).toString());
    }
    if (m_currentDriveMetadata.contains(QStringLiteral("modifiedTime"))) {
        entry.insert(QStringLiteral("modifiedTime"), m_currentDriveMetadata.value(QStringLiteral("modifiedTime")).toString());
    }
    entry.insert(QStringLiteral("lastDownloaded"), Utils::nowIsoStringUtc());
    m_syncStateManager->updateAttachment(m_current.assignment.courseId, m_current.assignment.id, fileId, entry);

    emit attachmentLog(QStringLiteral("SAVE  Guardado: %1").arg(savedPath));
    finalizeCurrentItem(true, false, false);
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
    finalizeCurrentItem(false, true, false);
}

void AttachmentDownloader::onRequestFailed(const QString &context, int httpStatus, const QString &errorMessage)
{
    Q_UNUSED(context)

    if (!m_hasCurrent) {
        return;
    }

    QString msg = errorMessage;
    if (httpStatus == 401) {
        msg = QStringLiteral("Token invalido o expirado. Vuelve a iniciar sesion.");
    } else if (httpStatus == 403) {
        msg = QStringLiteral("No tienes permiso para leer este archivo de Drive o falta el scope drive.readonly.");
    }

    emit attachmentLog(QStringLiteral("ERR   No se pudo descargar archivo: %1").arg(msg));
    finalizeCurrentItem(false, false, true);
}

void AttachmentDownloader::finalizeCurrentItem(bool downloaded, bool skipped, bool errorOccurred)
{
    if (downloaded) {
        ++m_downloaded;
    }
    if (skipped) {
        ++m_skipped;
    }
    if (errorOccurred) {
        ++m_errors;
    }

    m_hasCurrent = false;
    m_currentDriveMetadata = QJsonObject();
    processNext();
}
