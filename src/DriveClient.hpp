#pragma once

#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>
#include <QString>

class QNetworkReply;

class DriveClient : public QObject {
    Q_OBJECT

public:
    explicit DriveClient(QObject *parent = nullptr);

    void setAccessToken(const QString &token);

public slots:
    void fetchFileMetadata(const QString &fileId);

signals:
    void fileMetadataLoaded(const QJsonObject &metadata);
    void requestFailed(const QString &error);

private slots:
    void onReplyFinished();

private:
    QString m_accessToken;
    QNetworkAccessManager m_network;
};
