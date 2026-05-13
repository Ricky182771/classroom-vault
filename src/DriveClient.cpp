#include "DriveClient.hpp"

#include "FolderOrganizer.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrlQuery>

DriveClient::DriveClient(QObject *parent)
    : QObject(parent)
{}

void DriveClient::setAccessToken(const QString &token)
{
    m_accessToken = token.trimmed();
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

QString DriveClient::extensionForGoogleMimeType(const QString &mimeType) const
{
    if (mimeType == QStringLiteral("application/vnd.google-apps.document")) {
        return QStringLiteral(".docx");
    }
    if (mimeType == QStringLiteral("application/vnd.google-apps.spreadsheet")) {
        return QStringLiteral(".xlsx");
    }
    if (mimeType == QStringLiteral("application/vnd.google-apps.presentation")) {
        return QStringLiteral(".pptx");
    }
    if (mimeType == QStringLiteral("application/vnd.google-apps.drawing")) {
        return QStringLiteral(".png");
    }
    return QString();
}

QString DriveClient::exportMimeTypeForGoogleMimeType(const QString &mimeType) const
{
    if (mimeType == QStringLiteral("application/vnd.google-apps.document")) {
        return QStringLiteral("application/vnd.openxmlformats-officedocument.wordprocessingml.document");
    }
    if (mimeType == QStringLiteral("application/vnd.google-apps.spreadsheet")) {
        return QStringLiteral("application/vnd.openxmlformats-officedocument.spreadsheetml.sheet");
    }
    if (mimeType == QStringLiteral("application/vnd.google-apps.presentation")) {
        return QStringLiteral("application/vnd.openxmlformats-officedocument.presentationml.presentation");
    }
    if (mimeType == QStringLiteral("application/vnd.google-apps.drawing")) {
        return QStringLiteral("image/png");
    }
    return QString();
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

void DriveClient::downloadFile(const QString &fileId, const QString &destinationDir, const QString &fileName)
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

void DriveClient::exportGoogleWorkspaceFile(
    const QString &fileId,
    const QString &mimeType,
    const QString &destinationDir,
    const QString &fileName)
{
    const QString id = fileId.trimmed();
    const QString googleMime = mimeType.trimmed();
    const QString exportMime = exportMimeTypeForGoogleMimeType(googleMime);

    if (m_accessToken.isEmpty()) {
        emit requestFailed(QStringLiteral("drive.export"), 0, QStringLiteral("No hay access token para Drive API."));
        return;
    }

    if (id.isEmpty() || destinationDir.trimmed().isEmpty() || exportMime.isEmpty()) {
        emit requestFailed(QStringLiteral("drive.export"), 0, QStringLiteral("Exportacion no soportada para este tipo de archivo."));
        return;
    }

    const QString extension = extensionForGoogleMimeType(googleMime);
    QString outName = sanitizeFileName(fileName.trimmed().isEmpty() ? id : fileName);
    if (!extension.isEmpty() && !outName.endsWith(extension, Qt::CaseInsensitive)) {
        outName += extension;
    }

    QDir dir;
    if (!dir.exists(destinationDir) && !dir.mkpath(destinationDir)) {
        emit requestFailed(QStringLiteral("drive.export"), 0, QStringLiteral("No se pudo crear directorio destino para exportacion."));
        return;
    }

    emit downloadStarted(id, outName);

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
    req.mimeType = googleMime;
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

        file.write(payload);
        file.close();

        emit downloadFinished(req.fileId, destinationPath);
        reply->deleteLater();
        return;
    }

    reply->deleteLater();
}
