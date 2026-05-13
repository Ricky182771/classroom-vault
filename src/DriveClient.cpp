#include "DriveClient.hpp"

#include <QJsonDocument>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

DriveClient::DriveClient(QObject *parent)
    : QObject(parent)
{}

void DriveClient::setAccessToken(const QString &token)
{
    m_accessToken = token.trimmed();
}

void DriveClient::fetchFileMetadata(const QString &fileId)
{
    if (m_accessToken.isEmpty()) {
        emit requestFailed(QStringLiteral("No hay access token para Drive API."));
        return;
    }

    if (fileId.trimmed().isEmpty()) {
        emit requestFailed(QStringLiteral("fileId vacio."));
        return;
    }

    // Nota futura: para descarga de contenido se requerira scope drive.readonly
    const QUrl url(
        QStringLiteral("https://www.googleapis.com/drive/v3/files/%1?fields=id,name,mimeType,webViewLink")
            .arg(QString::fromUtf8(QUrl::toPercentEncoding(fileId.trimmed()))));

    QNetworkRequest request(url);
    request.setRawHeader("Authorization", QStringLiteral("Bearer %1").arg(m_accessToken).toUtf8());

    QNetworkReply *reply = m_network.get(request);
    connect(reply, &QNetworkReply::finished, this, &DriveClient::onReplyFinished);
}

void DriveClient::onReplyFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    if (!reply) {
        return;
    }

    const QByteArray payload = reply->readAll();

    if (reply->error() != QNetworkReply::NoError) {
        const QString detail = QString::fromUtf8(payload);
        emit requestFailed(detail.isEmpty() ? reply->errorString() : detail);
        reply->deleteLater();
        return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(payload);
    if (!doc.isObject()) {
        emit requestFailed(QStringLiteral("Respuesta invalida de Drive API."));
        reply->deleteLater();
        return;
    }

    emit fileMetadataLoaded(doc.object());
    reply->deleteLater();
}
