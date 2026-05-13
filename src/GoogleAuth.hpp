#pragma once

#include <QDateTime>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QObject>
#include <QStringList>
#include <QUrl>

class QNetworkReply;
class QTcpServer;
class QTcpSocket;
class QTimer;

class GoogleAuth : public QObject {
    Q_OBJECT

public:
    explicit GoogleAuth(QObject *parent = nullptr);
    ~GoogleAuth() override = default;

    void setClientConfig(
        const QString &clientId,
        const QString &clientSecret,
        const QString &redirectUri,
        const QStringList &scopes);
    void setTokenUri(const QString &tokenUri);

    bool isConfigured() const;
    bool isAuthenticating() const;

    void startAuthorization();
    void exchangeAuthorizationCode(const QString &authorizationCode);
    void refreshAccessToken();
    void cancelAuthorization();

    void clearTokens();
    void signOut();

    void loadFromJson(const QJsonObject &tokenObject);
    QJsonObject toJson() const;

    QString accessToken() const;
    QString refreshToken() const;
    bool hasUsableAccessToken() const;
    bool hasValidAccessToken() const;
    bool isAccessTokenExpired() const;
    bool hasScope(const QString &scope) const;
    QString grantedScopes() const;
    QDateTime tokenExpiryUtc() const;

signals:
    void authorizationUrlReady(const QUrl &url);
    void authSucceeded();
    void tokenUpdated();
    void errorOccurred(const QString &message);
    void logMessage(const QString &message);

    void authenticationStarted();
    void browserOpened(const QUrl &url);
    void authorizationCodeReceived();
    void authenticated(const QString &accessToken);
    void tokenRefreshed(const QString &accessToken);
    void authFailed(const QString &errorMessage);
    void authStatusChanged(const QString &status);

private slots:
    void onTokenReplyFinished();
    void onLoopbackNewConnection();
    void onAuthTimeout();

private:
    QNetworkRequest buildTokenRequest() const;
    void applyTokenResponse(const QJsonObject &json);
    void exchangeAuthorizationCodeWithRedirect(const QString &authorizationCode, const QString &redirectUri);
    bool startLoopbackListener();
    void handleLoopbackSocket(QTcpSocket *socket);
    void respondHtml(QTcpSocket *socket, int statusCode, const QString &title, const QString &bodyMessage) const;
    void finishLoopbackSession();
    void failAuthentication(const QString &message, bool closeServer = true);
    QString tokenEndpointUrl() const;
    QString currentRedirectUriForExchange() const;
    QString generateStateToken() const;
    void emitStatus(const QString &status);

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

    QString m_tokenUri = QStringLiteral("https://oauth2.googleapis.com/token");

    QTcpServer *m_loopbackServer = nullptr;
    QTimer *m_authTimeoutTimer = nullptr;
    QString m_pendingState;
    QString m_activeRedirectUri;
    bool m_authInProgress = false;
};
