#include "GoogleAuth.hpp"

#include <QDesktopServices>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrlQuery>

GoogleAuth::GoogleAuth(QObject *parent)
    : QObject(parent)
{}

void GoogleAuth::setClientConfig(
    const QString &clientId,
    const QString &clientSecret,
    const QString &redirectUri,
    const QStringList &scopes)
{
    m_clientId = clientId.trimmed();
    m_clientSecret = clientSecret.trimmed();
    m_redirectUri = redirectUri.trimmed();
    m_scopes = scopes;

    if (m_redirectUri.isEmpty()) {
        m_redirectUri = QStringLiteral("urn:ietf:wg:oauth:2.0:oob");
    }
}

bool GoogleAuth::isConfigured() const
{
    return !m_clientId.isEmpty() && !m_clientSecret.isEmpty() && !m_scopes.isEmpty();
}

void GoogleAuth::startAuthorization()
{
    if (!isConfigured()) {
        emit errorOccurred(QStringLiteral("OAuth no configurado: revisa clientId/clientSecret en config.json"));
        return;
    }

    QUrl authUrl(QStringLiteral("https://accounts.google.com/o/oauth2/v2/auth"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("client_id"), m_clientId);
    query.addQueryItem(QStringLiteral("redirect_uri"), m_redirectUri);
    query.addQueryItem(QStringLiteral("response_type"), QStringLiteral("code"));
    query.addQueryItem(QStringLiteral("scope"), m_scopes.join(QLatin1Char(' ')));
    query.addQueryItem(QStringLiteral("access_type"), QStringLiteral("offline"));
    query.addQueryItem(QStringLiteral("prompt"), QStringLiteral("consent"));
    authUrl.setQuery(query);

    QDesktopServices::openUrl(authUrl);
    emit logMessage(QStringLiteral("Se abrio el navegador para OAuth."));
    emit authorizationUrlReady(authUrl);
}

QNetworkRequest GoogleAuth::buildTokenRequest() const
{
    QNetworkRequest request(QUrl(QStringLiteral("https://oauth2.googleapis.com/token")));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/x-www-form-urlencoded"));
    return request;
}

void GoogleAuth::exchangeAuthorizationCode(const QString &authorizationCode)
{
    if (!isConfigured()) {
        emit errorOccurred(QStringLiteral("OAuth no configurado: revisa config.json"));
        return;
    }

    const QString code = authorizationCode.trimmed();
    if (code.isEmpty()) {
        emit errorOccurred(QStringLiteral("Codigo de autorizacion vacio."));
        return;
    }

    QUrlQuery body;
    body.addQueryItem(QStringLiteral("grant_type"), QStringLiteral("authorization_code"));
    body.addQueryItem(QStringLiteral("code"), code);
    body.addQueryItem(QStringLiteral("client_id"), m_clientId);
    body.addQueryItem(QStringLiteral("client_secret"), m_clientSecret);
    body.addQueryItem(QStringLiteral("redirect_uri"), m_redirectUri);

    QNetworkReply *reply = m_networkManager.post(buildTokenRequest(), body.toString(QUrl::FullyEncoded).toUtf8());
    reply->setProperty("requestType", QStringLiteral("exchange_code"));
    connect(reply, &QNetworkReply::finished, this, &GoogleAuth::onTokenReplyFinished);
}

void GoogleAuth::refreshAccessToken()
{
    if (!isConfigured()) {
        emit errorOccurred(QStringLiteral("OAuth no configurado: revisa config.json"));
        return;
    }

    if (m_refreshToken.trimmed().isEmpty()) {
        emit errorOccurred(QStringLiteral("No hay refresh_token. Inicia sesion de nuevo."));
        return;
    }

    QUrlQuery body;
    body.addQueryItem(QStringLiteral("grant_type"), QStringLiteral("refresh_token"));
    body.addQueryItem(QStringLiteral("refresh_token"), m_refreshToken);
    body.addQueryItem(QStringLiteral("client_id"), m_clientId);
    body.addQueryItem(QStringLiteral("client_secret"), m_clientSecret);

    QNetworkReply *reply = m_networkManager.post(buildTokenRequest(), body.toString(QUrl::FullyEncoded).toUtf8());
    reply->setProperty("requestType", QStringLiteral("refresh_token"));
    connect(reply, &QNetworkReply::finished, this, &GoogleAuth::onTokenReplyFinished);
}

void GoogleAuth::onTokenReplyFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    if (!reply) {
        return;
    }

    const QString requestType = reply->property("requestType").toString();
    const QByteArray payload = reply->readAll();

    if (reply->error() != QNetworkReply::NoError) {
        const QString errorText = QString::fromUtf8(payload);
        emit errorOccurred(
            QStringLiteral("Error OAuth (%1): %2")
                .arg(requestType, errorText.isEmpty() ? reply->errorString() : errorText));
        reply->deleteLater();
        return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(payload);
    if (!doc.isObject()) {
        emit errorOccurred(QStringLiteral("Respuesta OAuth invalida."));
        reply->deleteLater();
        return;
    }

    applyTokenResponse(doc.object());
    emit tokenUpdated();
    emit authSucceeded();

    if (requestType == QStringLiteral("refresh_token")) {
        emit logMessage(QStringLiteral("Access token refrescado correctamente."));
    } else {
        emit logMessage(QStringLiteral("Sesion OAuth completada correctamente."));
    }

    reply->deleteLater();
}

void GoogleAuth::applyTokenResponse(const QJsonObject &json)
{
    const QString newAccessToken = json.value(QStringLiteral("access_token")).toString();
    const QString newRefreshToken = json.value(QStringLiteral("refresh_token")).toString();

    if (!newAccessToken.isEmpty()) {
        m_accessToken = newAccessToken;
    }
    if (!newRefreshToken.isEmpty()) {
        m_refreshToken = newRefreshToken;
    }

    m_tokenType = json.value(QStringLiteral("token_type")).toString();
    m_scope = json.value(QStringLiteral("scope")).toString();

    const int expiresIn = json.value(QStringLiteral("expires_in")).toInt(0);
    if (expiresIn > 0) {
        m_expiryUtc = QDateTime::currentDateTimeUtc().addSecs(expiresIn - 30);
    }
}

void GoogleAuth::clearTokens()
{
    m_accessToken.clear();
    m_refreshToken.clear();
    m_tokenType.clear();
    m_scope.clear();
    m_expiryUtc = QDateTime();
}

void GoogleAuth::loadFromJson(const QJsonObject &tokenObject)
{
    m_accessToken = tokenObject.value(QStringLiteral("accessToken")).toString();
    m_refreshToken = tokenObject.value(QStringLiteral("refreshToken")).toString();
    m_tokenType = tokenObject.value(QStringLiteral("tokenType")).toString();
    m_scope = tokenObject.value(QStringLiteral("scope")).toString();

    const QString expiresAt = tokenObject.value(QStringLiteral("expiryUtc")).toString();
    m_expiryUtc = QDateTime::fromString(expiresAt, Qt::ISODate);
    if (m_expiryUtc.isValid()) {
        m_expiryUtc = m_expiryUtc.toUTC();
    }
}

QJsonObject GoogleAuth::toJson() const
{
    QJsonObject json;
    json.insert(QStringLiteral("accessToken"), m_accessToken);
    json.insert(QStringLiteral("refreshToken"), m_refreshToken);
    json.insert(QStringLiteral("tokenType"), m_tokenType);
    json.insert(QStringLiteral("scope"), m_scope);
    json.insert(QStringLiteral("expiryUtc"), m_expiryUtc.isValid() ? m_expiryUtc.toUTC().toString(Qt::ISODate) : QString());
    return json;
}

QString GoogleAuth::accessToken() const
{
    return m_accessToken;
}

QString GoogleAuth::refreshToken() const
{
    return m_refreshToken;
}

bool GoogleAuth::hasUsableAccessToken() const
{
    if (m_accessToken.isEmpty()) {
        return false;
    }

    if (!m_expiryUtc.isValid()) {
        return false;
    }

    return m_expiryUtc > QDateTime::currentDateTimeUtc();
}

bool GoogleAuth::hasScope(const QString &scope) const
{
    const QString wanted = scope.trimmed();
    if (wanted.isEmpty()) {
        return false;
    }

    const QStringList granted = m_scope.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    return granted.contains(wanted);
}

QString GoogleAuth::grantedScopes() const
{
    return m_scope;
}

QDateTime GoogleAuth::tokenExpiryUtc() const
{
    return m_expiryUtc;
}
