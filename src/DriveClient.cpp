#include "DriveClient.hpp"

#include "FolderOrganizer.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QUrlQuery>

DriveClient::DriveClient(QObject *parent)
    : QObject(parent)
{}

void DriveClient::setAccessToken(const QString &token)
{
    m_accessToken = token.trimmed();
}

bool DriveClient::isGoogleWorkspaceMimeType(const QString &mimeType) const
{
    return mimeType.trimmed().startsWith(QStringLiteral("application/vnd.google-apps."));
}

bool DriveClient::isExportableWorkspaceMimeType(const QString &mimeType) const
{
    return !exportMimeTypeFor(mimeType).isEmpty();
}

QString DriveClient::exportMimeTypeFor(const QString &sourceMimeType) const
{
    if (sourceMimeType == QStringLiteral("application/vnd.google-apps.document")) {
        return QStringLiteral("application/vnd.openxmlformats-officedocument.wordprocessingml.document");
    }
    if (sourceMimeType == QStringLiteral("application/vnd.google-apps.spreadsheet")) {
        return QStringLiteral("application/vnd.openxmlformats-officedocument.spreadsheetml.sheet");
    }
    if (sourceMimeType == QStringLiteral("application/vnd.google-apps.presentation")) {
        return QStringLiteral("application/vnd.openxmlformats-officedocument.presentationml.presentation");
    }
    if (sourceMimeType == QStringLiteral("application/vnd.google-apps.drawing")) {
        return QStringLiteral("image/png");
    }
    return QString();
}

QString DriveClient::extensionForExportedMimeType(const QString &sourceMimeType) const
{
    if (sourceMimeType == QStringLiteral("application/vnd.google-apps.document")) {
        return QStringLiteral(".docx");
    }
    if (sourceMimeType == QStringLiteral("application/vnd.google-apps.spreadsheet")) {
        return QStringLiteral(".xlsx");
    }
    if (sourceMimeType == QStringLiteral("application/vnd.google-apps.presentation")) {
        return QStringLiteral(".pptx");
    }
    if (sourceMimeType == QStringLiteral("application/vnd.google-apps.drawing")) {
        return QStringLiteral(".png");
    }
    return QStringLiteral(".url");
}

void DriveClient::downloadFile(const QString &fileId, const QString &destinationDir, const QString &fileName)
{
    downloadBlobFile(fileId, destinationDir, fileName);
}

void DriveClient::exportGoogleWorkspaceFile(
    const QString &fileId,
    const QString &mimeType,
    const QString &destinationDir,
    const QString &fileName)
{
    exportWorkspaceFile(fileId, mimeType, destinationDir, fileName);
}

QNetworkRequest DriveClient::authorizedRequest(const QUrl &url) const
{
    QNetworkRequest request(url);
    request.setRawHeader("Authorization", QStringLiteral("Bearer %1").arg(m_accessToken).toUtf8());
    return request;
}

QString DriveClient::sanitizeFileName(const QString &name) const
{
    return FolderOrganizer::sanitizeFileName(name);
}

QString DriveClient::uniquePathIfExists(const QString &desiredPath) const
{
    if (!QFileInfo::exists(desiredPath)) {
        return desiredPath;
    }

    const QFileInfo info(desiredPath);
    const QString parentDir = info.absolutePath();
    const QString baseName = info.completeBaseName();
    const QString suffix = info.completeSuffix();
    const QString extension = suffix.isEmpty() ? QString() : QStringLiteral(".") + suffix;

    int index = 2;
    QString candidatePath;
    do {
        const QString candidateName = sanitizeFileName(QStringLiteral("%1 (%2)%3").arg(baseName).arg(index).arg(extension));
        candidatePath = QDir(parentDir).filePath(candidateName);
        ++index;
    } while (QFileInfo::exists(candidatePath) && index < 10000);

    return candidatePath;
}

QString DriveClient::extractErrorMessage(const QByteArray &payload) const
{
    const QJsonDocument doc = QJsonDocument::fromJson(payload);
    if (!doc.isObject()) {
        return QString::fromUtf8(payload).trimmed();
    }

    const QJsonObject errorObj = doc.object().value(QStringLiteral("error")).toObject();
    const QString message = errorObj.value(QStringLiteral("message")).toString().trimmed();
    if (!message.isEmpty()) {
        return message;
    }

    return QString::fromUtf8(payload).trimmed();
}

void DriveClient::fetchFileMetadata(const QString &fileId)
{
    const QString id = fileId.trimmed();

    if (m_accessToken.isEmpty()) {
        emit requestFailed(QStringLiteral("drive.metadata"), 0, QStringLiteral("No hay access token para Drive API."));
        return;
    }

    if (id.isEmpty()) {
        emit requestFailed(QStringLiteral("drive.metadata"), 0, QStringLiteral("fileId vacio."));
        return;
    }

    const QUrl url(
        QStringLiteral("https://www.googleapis.com/drive/v3/files/%1?fields=id,name,mimeType,size,modifiedTime,webViewLink,md5Checksum")
            .arg(QString::fromUtf8(QUrl::toPercentEncoding(id))));

    QNetworkReply *reply = m_network.get(authorizedRequest(url));

    PendingRequest req;
    req.kind = QStringLiteral("metadata");
    req.fileId = id;
    req.context = QStringLiteral("drive.metadata");
    m_pending.insert(reply, req);

    connect(reply, &QNetworkReply::finished, this, &DriveClient::onReplyFinished);
}

void DriveClient::downloadBlobFile(const QString &fileId, const QString &destinationDir, const QString &fileName)
{
    const QString id = fileId.trimmed();
    const QString dirPath = destinationDir.trimmed();
    const QString safeName = sanitizeFileName(fileName.trimmed().isEmpty() ? id : fileName);

    if (m_accessToken.isEmpty()) {
        emit requestFailed(QStringLiteral("drive.download"), 0, QStringLiteral("No hay access token para Drive API."));
        return;
    }

    if (id.isEmpty() || dirPath.isEmpty()) {
        emit requestFailed(QStringLiteral("drive.download"), 0, QStringLiteral("Parametros invalidos para descarga de archivo."));
        return;
    }

    QDir dir;
    if (!dir.exists(dirPath) && !dir.mkpath(dirPath)) {
        emit requestFailed(QStringLiteral("drive.download"), 0, QStringLiteral("No se pudo crear directorio destino para adjuntos."));
        return;
    }

    emit downloadStarted(id, safeName);

    const QUrl url(QStringLiteral("https://www.googleapis.com/drive/v3/files/%1?alt=media")
                       .arg(QString::fromUtf8(QUrl::toPercentEncoding(id))));

    QNetworkReply *reply = m_network.get(authorizedRequest(url));

    PendingRequest req;
    req.kind = QStringLiteral("download");
    req.fileId = id;
    req.destinationDir = dirPath;
    req.fileName = safeName;
    req.context = QStringLiteral("drive.download");
    m_pending.insert(reply, req);

    connect(reply, &QNetworkReply::downloadProgress, this, [this, reply](qint64 received, qint64 total) {
        const PendingRequest req = m_pending.value(reply);
        emit downloadProgress(req.fileId, received, total);
    });
    connect(reply, &QNetworkReply::finished, this, &DriveClient::onReplyFinished);
}

void DriveClient::exportWorkspaceFile(
    const QString &fileId,
    const QString &sourceMimeType,
    const QString &destinationDir,
    const QString &fileName)
{
    const QString id = fileId.trimmed();
    const QString sourceMime = sourceMimeType.trimmed();
    const QString exportMime = exportMimeTypeFor(sourceMime);

    if (m_accessToken.isEmpty()) {
        emit requestFailed(QStringLiteral("drive.export"), 0, QStringLiteral("No hay access token para Drive API."));
        return;
    }

    if (id.isEmpty() || destinationDir.trimmed().isEmpty()) {
        emit requestFailed(QStringLiteral("drive.export"), 0, QStringLiteral("Parametros invalidos para exportacion de archivo."));
        return;
    }

    if (exportMime.isEmpty()) {
        emit requestFailed(QStringLiteral("drive.export"), 400, QStringLiteral("Exportacion no soportada para MIME type: %1").arg(sourceMime));
        return;
    }

    const QString extension = extensionForExportedMimeType(sourceMime);
    QString outName = sanitizeFileName(fileName.trimmed().isEmpty() ? id : fileName);
    if (!extension.isEmpty() && !outName.endsWith(extension, Qt::CaseInsensitive)) {
        outName += extension;
    }

    QDir dir;
    if (!dir.exists(destinationDir) && !dir.mkpath(destinationDir)) {
        emit requestFailed(QStringLiteral("drive.export"), 0, QStringLiteral("No se pudo crear directorio destino para exportacion."));
        return;
    }

    emit exportStarted(id, outName);

    QUrl url(QStringLiteral("https://www.googleapis.com/drive/v3/files/%1/export")
                 .arg(QString::fromUtf8(QUrl::toPercentEncoding(id))));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("mimeType"), exportMime);
    url.setQuery(query);

    QNetworkReply *reply = m_network.get(authorizedRequest(url));

    PendingRequest req;
    req.kind = QStringLiteral("export");
    req.fileId = id;
    req.destinationDir = destinationDir;
    req.fileName = outName;
    req.requestedMimeType = sourceMime;
    req.resolvedExportMimeType = exportMime;
    req.context = QStringLiteral("drive.export");
    m_pending.insert(reply, req);

    connect(reply, &QNetworkReply::downloadProgress, this, [this, reply](qint64 received, qint64 total) {
        const PendingRequest req = m_pending.value(reply);
        emit downloadProgress(req.fileId, received, total);
    });
    connect(reply, &QNetworkReply::finished, this, &DriveClient::onReplyFinished);
}

void DriveClient::onReplyFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    if (!reply) {
        return;
    }

    const PendingRequest req = m_pending.take(reply);
    const QByteArray payload = reply->readAll();
    const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if (reply->error() != QNetworkReply::NoError || httpStatus >= 400) {
        QString errorMessage = extractErrorMessage(payload);
        if (errorMessage.isEmpty()) {
            errorMessage = reply->errorString();
        }

        if (httpStatus == 401) {
            errorMessage = QStringLiteral("Token invalido o expirado. Vuelve a iniciar sesion.");
        } else if (httpStatus == 403) {
            errorMessage = QStringLiteral("No tienes permiso para leer este archivo de Drive o falta el scope drive.readonly.");
        } else if (httpStatus == 404) {
            errorMessage = QStringLiteral("Archivo no encontrado en Drive (404).");
        }

        emit requestFailed(req.context, httpStatus, errorMessage);
        reply->deleteLater();
        return;
    }

    if (req.kind == QStringLiteral("metadata")) {
        const QJsonDocument doc = QJsonDocument::fromJson(payload);
        if (!doc.isObject()) {
            emit requestFailed(req.context, httpStatus, QStringLiteral("Respuesta invalida de Drive API."));
            reply->deleteLater();
            return;
        }

        emit fileMetadataLoaded(req.fileId, doc.object());
        reply->deleteLater();
        return;
    }

    if (req.kind == QStringLiteral("download") || req.kind == QStringLiteral("export")) {
        const QString destinationPath = QDir(req.destinationDir).filePath(req.fileName);

        QFile file(destinationPath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            emit requestFailed(req.context, httpStatus, QStringLiteral("No se pudo escribir archivo en disco: %1").arg(destinationPath));
            reply->deleteLater();
            return;
        }

        const qint64 written = file.write(payload);
        file.close();
        if (written < 0) {
            emit requestFailed(req.context, httpStatus, QStringLiteral("Fallo al escribir archivo en disco: %1").arg(destinationPath));
            reply->deleteLater();
            return;
        }

        emit downloadFinished(req.fileId, destinationPath);
        if (req.kind == QStringLiteral("export")) {
            emit exportFinished(req.fileId, destinationPath);
        }

        reply->deleteLater();
        return;
    }

    reply->deleteLater();
}
