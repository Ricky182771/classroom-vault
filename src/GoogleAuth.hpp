#pragma once

#include <QDateTime>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>
#include <QStringList>
#include <QUrl>

class QNetworkReply;

class GoogleAuth : public QObject {
    Q_OBJECT

public:
    explicit GoogleAuth(QObject *parent = nullptr);

    void setClientConfig(
        const QString &clientId,
        const QString &clientSecret,
        const QString &redirectUri,
        const QStringList &scopes);

    bool isConfigured() const;

    void startAuthorization();
    void exchangeAuthorizationCode(const QString &authorizationCode);
    void refreshAccessToken();

    void clearTokens();

    void loadFromJson(const QJsonObject &tokenObject);
    QJsonObject toJson() const;

    QString accessToken() const;
    QString refreshToken() const;
    bool hasUsableAccessToken() const;
    bool hasScope(const QString &scope) const;
    QString grantedScopes() const;
    QDateTime tokenExpiryUtc() const;

signals:
    void authorizationUrlReady(const QUrl &url);
    void authSucceeded();
    void tokenUpdated();
    void errorOccurred(const QString &message);
    void logMessage(const QString &message);

private slots:
    void onTokenReplyFinished();

private:
    QNetworkRequest buildTokenRequest() const;
    void applyTokenResponse(const QJsonObject &json);

    QNetworkAccessManager m_networkManager;

    QString m_clientId;
    QString m_clientSecret;
    QString m_redirectUri;
    QStringList m_scopes;

    QString m_accessToken;
    QString m_refreshToken;
    QString m_tokenType;
    QString m_scope;
    QDateTime m_expiryUtc;
};
