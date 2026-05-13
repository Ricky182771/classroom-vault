#pragma once

#include <QHash>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QString>

class DriveClient : public QObject {
    Q_OBJECT

public:
    explicit DriveClient(QObject *parent = nullptr);

    void setAccessToken(const QString &token);

    void fetchFileMetadata(const QString &fileId);
    void downloadFile(const QString &fileId, const QString &destinationDir, const QString &fileName = QString());
    void exportGoogleWorkspaceFile(
        const QString &fileId,
        const QString &mimeType,
        const QString &destinationDir,
        const QString &fileName = QString());

signals:
    void fileMetadataLoaded(const QString &fileId, const QJsonObject &metadata);
    void downloadStarted(const QString &fileId, const QString &fileName);
    void downloadProgress(const QString &fileId, qint64 bytesReceived, qint64 bytesTotal);
    void downloadFinished(const QString &fileId, const QString &savedPath);
    void downloadSkipped(const QString &fileId, const QString &reason);
    void requestFailed(const QString &context, int httpStatus, const QString &errorMessage);

private slots:
    void onReplyFinished();

private:
    struct PendingRequest {
        QString kind;
        QString fileId;
        QString destinationDir;
        QString fileName;
        QString mimeType;
        QString context;
    };

    QNetworkRequest authorizedRequest(const QUrl &url) const;
    QString sanitizeFileName(const QString &name) const;
    QString extensionForGoogleMimeType(const QString &mimeType) const;
    QString exportMimeTypeForGoogleMimeType(const QString &mimeType) const;
    QString extractErrorMessage(const QByteArray &payload) const;

    QString m_accessToken;
    QNetworkAccessManager m_network;
    QHash<QNetworkReply *, PendingRequest> m_pending;
};
