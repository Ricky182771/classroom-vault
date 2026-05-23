#include "GoogleAuth.hpp"

#include <QDesktopServices>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QUrlQuery>
#include <QUuid>

GoogleAuth::GoogleAuth(QObject *parent)
    : QObject(parent)
    , m_loopbackServer(new QTcpServer(this))
    , m_authTimeoutTimer(new QTimer(this))
{
    connect(m_loopbackServer, &QTcpServer::newConnection, this, &GoogleAuth::onLoopbackNewConnection);

    m_authTimeoutTimer->setSingleShot(true);
    m_authTimeoutTimer->setInterval(180000);
    connect(m_authTimeoutTimer, &QTimer::timeout, this, &GoogleAuth::onAuthTimeout);
}

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
    m_scopes.removeAll(QString());
    m_scopes.removeDuplicates();

    if (m_redirectUri.isEmpty()) {
        m_redirectUri = QStringLiteral("http://127.0.0.1");
    }
}

void GoogleAuth::setTokenUri(const QString &tokenUri)
{
    const QString trimmed = tokenUri.trimmed();
    if (trimmed.isEmpty()) {
        m_tokenUri = QStringLiteral("https://oauth2.googleapis.com/token");
        return;
    }
    m_tokenUri = trimmed;
}

bool GoogleAuth::isConfigured() const
{
    return !m_clientId.isEmpty() && !m_scopes.isEmpty();
}

bool GoogleAuth::isAuthenticating() const
{
    return m_authInProgress;
}

void GoogleAuth::emitStatus(const QString &status)
{
    emit authStatusChanged(status);
}

QString GoogleAuth::generateStateToken() const
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

bool GoogleAuth::startLoopbackListener()
{
    if (!m_loopbackServer) {
        return false;
    }

    if (m_loopbackServer->isListening()) {
        m_loopbackServer->close();
    }

    const QHostAddress loopbackAddress(QStringLiteral("127.0.0.1"));
    if (!m_loopbackServer->listen(loopbackAddress, 0)) {
        return false;
    }

    m_activeRedirectUri = QStringLiteral("http://127.0.0.1:%1/callback").arg(m_loopbackServer->serverPort());
    m_authTimeoutTimer->start();
    m_authInProgress = true;
    return true;
}

void GoogleAuth::finishLoopbackSession()
{
    if (m_authTimeoutTimer) {
        m_authTimeoutTimer->stop();
    }

    if (m_loopbackServer && m_loopbackServer->isListening()) {
        m_loopbackServer->close();
    }

    m_pendingState.clear();
    m_activeRedirectUri.clear();
    m_authInProgress = false;
}

void GoogleAuth::failAuthentication(const QString &message, bool closeServer)
{
    if (closeServer) {
        finishLoopbackSession();
    } else {
        m_authInProgress = false;
    }

    emitStatus(QStringLiteral("failed"));
    emit authFailed(message);
    emit errorOccurred(message);
}

void GoogleAuth::cancelAuthorization()
{
    if (!m_authInProgress) {
        return;
    }

    finishLoopbackSession();
    emitStatus(QStringLiteral("cancelled"));
    emit logMessage(QStringLiteral("Autenticacion cancelada."));
}

void GoogleAuth::startAuthorization()
{
    if (!isConfigured()) {
        failAuthentication(QStringLiteral("OAuth no configurado: revisa clientId/clientSecret en config.json"), false);
        return;
    }

    if (m_authInProgress) {
        cancelAuthorization();
    }

    emit authenticationStarted();
    emitStatus(QStringLiteral("starting"));

    const bool loopbackStarted = startLoopbackListener();
    if (!loopbackStarted) {
        emit logMessage(QStringLiteral("No se pudo iniciar servidor local de autenticacion. Se intentara fallback manual."));
        emitStatus(QStringLiteral("manual_code_required"));

        QUrl manualUrl(QStringLiteral("https://accounts.google.com/o/oauth2/v2/auth"));
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("client_id"), m_clientId);
        query.addQueryItem(QStringLiteral("redirect_uri"), m_redirectUri);
        query.addQueryItem(QStringLiteral("response_type"), QStringLiteral("code"));
        query.addQueryItem(QStringLiteral("scope"), m_scopes.join(QLatin1Char(' ')));
        query.addQueryItem(QStringLiteral("access_type"), QStringLiteral("offline"));
        query.addQueryItem(QStringLiteral("prompt"), QStringLiteral("consent"));
        query.addQueryItem(QStringLiteral("include_granted_scopes"), QStringLiteral("true"));
        manualUrl.setQuery(query);

        QDesktopServices::openUrl(manualUrl);
        emit authorizationUrlReady(manualUrl);
        emit browserOpened(manualUrl);
        return;
    }

    m_pendingState = generateStateToken();

    QUrl authUrl(QStringLiteral("https://accounts.google.com/o/oauth2/v2/auth"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("client_id"), m_clientId);
    query.addQueryItem(QStringLiteral("redirect_uri"), m_activeRedirectUri);
    query.addQueryItem(QStringLiteral("response_type"), QStringLiteral("code"));
    query.addQueryItem(QStringLiteral("scope"), m_scopes.join(QLatin1Char(' ')));
    query.addQueryItem(QStringLiteral("access_type"), QStringLiteral("offline"));
    query.addQueryItem(QStringLiteral("prompt"), QStringLiteral("consent"));
    query.addQueryItem(QStringLiteral("include_granted_scopes"), QStringLiteral("true"));
    query.addQueryItem(QStringLiteral("state"), m_pendingState);
    authUrl.setQuery(query);

    const bool opened = QDesktopServices::openUrl(authUrl);
    emit authorizationUrlReady(authUrl);
    emit browserOpened(authUrl);

    if (!opened) {
        failAuthentication(QStringLiteral("No se pudo abrir el navegador para autenticacion OAuth."));
        return;
    }

    emitStatus(QStringLiteral("waiting_callback"));
    emit logMessage(QStringLiteral("Abriendo navegador para iniciar sesion con Google..."));
}

QString GoogleAuth::tokenEndpointUrl() const
{
    return m_tokenUri.trimmed().isEmpty() ? QStringLiteral("https://oauth2.googleapis.com/token") : m_tokenUri.trimmed();
}

QNetworkRequest GoogleAuth::buildTokenRequest() const
{
    QNetworkRequest request{QUrl(tokenEndpointUrl())};
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/x-www-form-urlencoded"));
    return request;
}

QString GoogleAuth::currentRedirectUriForExchange() const
{
    if (!m_activeRedirectUri.trimmed().isEmpty()) {
        return m_activeRedirectUri;
    }
    return m_redirectUri.trimmed();
}

void GoogleAuth::exchangeAuthorizationCodeWithRedirect(const QString &authorizationCode, const QString &redirectUri)
{
    if (!isConfigured()) {
        failAuthentication(QStringLiteral("OAuth no configurado: revisa config.json"), false);
        return;
    }

    const QString code = authorizationCode.trimmed();
    if (code.isEmpty()) {
        failAuthentication(QStringLiteral("Codigo de autorizacion vacio."), false);
        return;
    }

    QUrlQuery body;
    body.addQueryItem(QStringLiteral("grant_type"), QStringLiteral("authorization_code"));
    body.addQueryItem(QStringLiteral("code"), code);
    body.addQueryItem(QStringLiteral("client_id"), m_clientId);
    if (!m_clientSecret.isEmpty()) {
        body.addQueryItem(QStringLiteral("client_secret"), m_clientSecret);
    }
    body.addQueryItem(QStringLiteral("redirect_uri"), redirectUri.trimmed());

    QNetworkReply *reply = m_networkManager.post(buildTokenRequest(), body.toString(QUrl::FullyEncoded).toUtf8());
    reply->setProperty("requestType", QStringLiteral("exchange_code"));
    connect(reply, &QNetworkReply::finished, this, &GoogleAuth::onTokenReplyFinished);
}

void GoogleAuth::exchangeAuthorizationCode(const QString &authorizationCode)
{
    exchangeAuthorizationCodeWithRedirect(authorizationCode, currentRedirectUriForExchange());
}

void GoogleAuth::refreshAccessToken()
{
    if (!isConfigured()) {
        failAuthentication(QStringLiteral("OAuth no configurado: revisa config.json"), false);
        return;
    }

    if (m_refreshToken.trimmed().isEmpty()) {
        failAuthentication(QStringLiteral("La sesion expiro. Inicia sesion nuevamente."), false);
        return;
    }

    QUrlQuery body;
    body.addQueryItem(QStringLiteral("grant_type"), QStringLiteral("refresh_token"));
    body.addQueryItem(QStringLiteral("refresh_token"), m_refreshToken);
    body.addQueryItem(QStringLiteral("client_id"), m_clientId);
    if (!m_clientSecret.isEmpty()) {
        body.addQueryItem(QStringLiteral("client_secret"), m_clientSecret);
    }

    QNetworkReply *reply = m_networkManager.post(buildTokenRequest(), body.toString(QUrl::FullyEncoded).toUtf8());
    reply->setProperty("requestType", QStringLiteral("refresh_token"));
    connect(reply, &QNetworkReply::finished, this, &GoogleAuth::onTokenReplyFinished);
}

void GoogleAuth::respondHtml(QTcpSocket *socket, int statusCode, const QString &title, const QString &bodyMessage) const
{
    if (!socket) {
        return;
    }

    const QString reason = (statusCode >= 200 && statusCode < 300) ? QStringLiteral("OK") : QStringLiteral("Bad Request");
    const QString html = QStringLiteral(
                             "<!doctype html><html><head><meta charset=\"utf-8\"><title>%1</title></head>"
                             "<body><h2>%2</h2><p>%3</p></body></html>")
                             .arg(title.toHtmlEscaped(), title.toHtmlEscaped(), bodyMessage.toHtmlEscaped());

    const QByteArray payload = html.toUtf8();
    const QByteArray response =
        QStringLiteral("HTTP/1.1 %1 %2\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: %3\r\nConnection: close\r\n\r\n")
            .arg(statusCode)
            .arg(reason)
            .arg(payload.size())
            .toUtf8()
        + payload;

    socket->write(response);
    socket->flush();
    socket->waitForBytesWritten(1000);
    socket->disconnectFromHost();
}

void GoogleAuth::handleLoopbackSocket(QTcpSocket *socket)
{
    if (!socket) {
        return;
    }

    connect(socket, &QTcpSocket::disconnected, socket, &QTcpSocket::deleteLater);

    if (!socket->waitForReadyRead(5000)) {
        respondHtml(
            socket,
            400,
            QStringLiteral("Classroom Vault"),
            QStringLiteral("No se recibio solicitud valida de autorizacion."));
        failAuthentication(QStringLiteral("No se recibio callback de OAuth valido."));
        return;
    }

    const QByteArray request = socket->readAll();
    const int firstBreak = request.indexOf('\n');
    const QByteArray firstLine = (firstBreak >= 0 ? request.left(firstBreak) : request).trimmed();
    const QList<QByteArray> parts = firstLine.split(' ');

    if (parts.size() < 2 || parts.at(0) != "GET") {
        respondHtml(
            socket,
            400,
            QStringLiteral("Classroom Vault"),
            QStringLiteral("Solicitud HTTP invalida."));
        failAuthentication(QStringLiteral("Solicitud HTTP invalida durante OAuth callback."));
        return;
    }

    const QUrl callbackUrl = QUrl::fromEncoded(parts.at(1));
    const QString path = callbackUrl.path().trimmed();

    if (path != QStringLiteral("/callback")) {
        respondHtml(
            socket,
            404,
            QStringLiteral("Classroom Vault"),
            QStringLiteral("Ruta no encontrada."));
        return;
    }

    const QUrlQuery query(callbackUrl);
    const QString error = query.queryItemValue(QStringLiteral("error")).trimmed();
    const QString code = query.queryItemValue(QStringLiteral("code")).trimmed();
    const QString state = query.queryItemValue(QStringLiteral("state")).trimmed();

    if (!error.isEmpty()) {
        respondHtml(
            socket,
            400,
            QStringLiteral("Classroom Vault"),
            QStringLiteral("Autorizacion rechazada o cancelada: %1").arg(error));
        failAuthentication(QStringLiteral("Autorizacion cancelada: %1").arg(error));
        return;
    }

    if (m_pendingState.isEmpty() || state.isEmpty() || state != m_pendingState) {
        respondHtml(
            socket,
            400,
            QStringLiteral("Classroom Vault"),
            QStringLiteral("Estado OAuth invalido."));
        failAuthentication(QStringLiteral("OAuth state invalido."));
        return;
    }

    if (code.isEmpty()) {
        respondHtml(
            socket,
            400,
            QStringLiteral("Classroom Vault"),
            QStringLiteral("No se recibio codigo de autorizacion."));
        failAuthentication(QStringLiteral("No se recibio authorization code."));
        return;
    }

    respondHtml(
        socket,
        200,
        QStringLiteral("Classroom Vault"),
        QStringLiteral("Autorizacion completada. Ya puedes volver a Classroom Vault."));

    const QString redirectUsed = m_activeRedirectUri;
    finishLoopbackSession();

    emit authorizationCodeReceived();
    emitStatus(QStringLiteral("authorization_code_received"));
    emit logMessage(QStringLiteral("Codigo de autorizacion recibido correctamente."));
    exchangeAuthorizationCodeWithRedirect(code, redirectUsed);
}

void GoogleAuth::onLoopbackNewConnection()
{
    if (!m_loopbackServer) {
        return;
    }

    while (m_loopbackServer->hasPendingConnections()) {
        QTcpSocket *socket = m_loopbackServer->nextPendingConnection();
        handleLoopbackSocket(socket);
    }
}

void GoogleAuth::onAuthTimeout()
{
    if (!m_authInProgress) {
        return;
    }

    failAuthentication(QStringLiteral("Tiempo de autenticacion agotado."));
}

void GoogleAuth::onTokenReplyFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    if (!reply) {
        return;
    }

    const QString requestType = reply->property("requestType").toString();
    const QByteArray payload = reply->readAll();
    const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if (reply->error() != QNetworkReply::NoError || httpStatus >= 400) {
        const QString errorText = QString::fromUtf8(payload).trimmed();

        if (requestType == QStringLiteral("refresh_token")) {
            clearTokens();
            emit tokenUpdated();
            emit logMessage(QStringLiteral("[AUTH] Refresh token invalido. La sesion expiro. Se requiere nuevo inicio de sesion."));
            failAuthentication(QStringLiteral("La sesion expiro. Inicia sesion nuevamente."));
        } else {
            failAuthentication(
                QStringLiteral("Error OAuth (%1): %2")
                    .arg(requestType, errorText.isEmpty() ? reply->errorString() : errorText),
                false);
        }

        reply->deleteLater();
        return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(payload);
    if (!doc.isObject()) {
        failAuthentication(QStringLiteral("Respuesta OAuth invalida."), false);
        reply->deleteLater();
        return;
    }

    applyTokenResponse(doc.object());
    emit tokenUpdated();
    emit authSucceeded();

    if (requestType == QStringLiteral("refresh_token")) {
        emit tokenRefreshed(m_accessToken);
        emitStatus(QStringLiteral("token_refreshed"));
        emit logMessage(QStringLiteral("[AUTH] Access token refrescado correctamente."));
    } else {
        emit authenticated(m_accessToken);
        emitStatus(QStringLiteral("authenticated"));
        emit logMessage(QStringLiteral("[AUTH] Sesion OAuth completada correctamente."));
    }

    reply->deleteLater();
}

void GoogleAuth::applyTokenResponse(const QJsonObject &json)
{
    const QString previousRefreshToken = m_refreshToken;

    const QString newAccessToken = json.value(QStringLiteral("access_token")).toString().trimmed();
    const QString newRefreshToken = json.value(QStringLiteral("refresh_token")).toString().trimmed();

    if (!newAccessToken.isEmpty()) {
        m_accessToken = newAccessToken;
    }

    if (!newRefreshToken.isEmpty()) {
        m_refreshToken = newRefreshToken;
    } else {
        m_refreshToken = previousRefreshToken;
    }

    const QString tokenType = json.value(QStringLiteral("token_type")).toString().trimmed();
    if (!tokenType.isEmpty()) {
        m_tokenType = tokenType;
    }

    const QString scope = json.value(QStringLiteral("scope")).toString().trimmed();
    if (!scope.isEmpty()) {
        m_scope = scope;
    }

    int expiresIn = 0;
    const QJsonValue expiresValue = json.value(QStringLiteral("expires_in"));
    if (expiresValue.isDouble()) {
        expiresIn = static_cast<int>(expiresValue.toDouble());
    } else if (expiresValue.isString()) {
        expiresIn = expiresValue.toString().toInt();
    }

    if (expiresIn > 0) {
        m_expiryUtc = QDateTime::currentDateTimeUtc().addSecs(expiresIn - 60);
    } else {
        m_expiryUtc = QDateTime();
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

void GoogleAuth::signOut()
{
    cancelAuthorization();
    clearTokens();
    emit tokenUpdated();
    emitStatus(QStringLiteral("signed_out"));
}

void GoogleAuth::loadFromJson(const QJsonObject &tokenObject)
{
    m_accessToken = tokenObject.value(QStringLiteral("accessToken")).toString();
    if (m_accessToken.isEmpty()) {
        m_accessToken = tokenObject.value(QStringLiteral("access_token")).toString();
    }

    m_refreshToken = tokenObject.value(QStringLiteral("refreshToken")).toString();
    if (m_refreshToken.isEmpty()) {
        m_refreshToken = tokenObject.value(QStringLiteral("refresh_token")).toString();
    }

    m_tokenType = tokenObject.value(QStringLiteral("tokenType")).toString();
    if (m_tokenType.isEmpty()) {
        m_tokenType = tokenObject.value(QStringLiteral("token_type")).toString();
    }

    m_scope = tokenObject.value(QStringLiteral("scope")).toString();

    QString expiresAt = tokenObject.value(QStringLiteral("expiryUtc")).toString();
    if (expiresAt.isEmpty()) {
        expiresAt = tokenObject.value(QStringLiteral("expires_at")).toString();
    }

    m_expiryUtc = QDateTime::fromString(expiresAt, Qt::ISODate);
    if (m_expiryUtc.isValid()) {
        m_expiryUtc = m_expiryUtc.toUTC();
    }
}

QJsonObject GoogleAuth::toJson() const
{
    const QString expiry = m_expiryUtc.isValid() ? m_expiryUtc.toUTC().toString(Qt::ISODate) : QString();

    QJsonObject json;
    json.insert(QStringLiteral("accessToken"), m_accessToken);
    json.insert(QStringLiteral("refreshToken"), m_refreshToken);
    json.insert(QStringLiteral("tokenType"), m_tokenType);
    json.insert(QStringLiteral("scope"), m_scope);
    json.insert(QStringLiteral("expiryUtc"), expiry);

    // Compatibilidad adicional con nombres estandar snake_case.
    json.insert(QStringLiteral("access_token"), m_accessToken);
    json.insert(QStringLiteral("refresh_token"), m_refreshToken);
    json.insert(QStringLiteral("token_type"), m_tokenType);
    json.insert(QStringLiteral("expires_at"), expiry);

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

bool GoogleAuth::isAccessTokenExpired() const
{
    if (m_accessToken.trimmed().isEmpty()) {
        return true;
    }

    if (!m_expiryUtc.isValid()) {
        return true;
    }

    return m_expiryUtc <= QDateTime::currentDateTimeUtc();
}

bool GoogleAuth::hasUsableAccessToken() const
{
    return hasValidAccessToken();
}

bool GoogleAuth::hasValidAccessToken() const
{
    return !isAccessTokenExpired();
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
