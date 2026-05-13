#include "ConfigManager.hpp"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>

ConfigManager::ConfigManager()
    : m_configDir(QDir::homePath() + QStringLiteral("/.config/ClassroomVault"))
{
    loadDefaults();
}

void ConfigManager::loadDefaults()
{
    m_basePath.clear();
    m_courseSemesters.clear();
    m_legacyCourseSemestersByName.clear();

    QJsonArray defaultScopes;
    defaultScopes.append(QStringLiteral("https://www.googleapis.com/auth/classroom.courses.readonly"));
    defaultScopes.append(QStringLiteral("https://www.googleapis.com/auth/classroom.coursework.me.readonly"));

    m_oauth = {
        {QStringLiteral("clientId"), QString()},
        {QStringLiteral("clientSecret"), QString()},
        {QStringLiteral("redirectUri"), QStringLiteral("urn:ietf:wg:oauth:2.0:oob")},
        {QStringLiteral("scopes"), defaultScopes},
    };
}

bool ConfigManager::ensureConfigDir() const
{
    QDir dir;
    if (dir.exists(m_configDir)) {
        return true;
    }
    return dir.mkpath(m_configDir);
}

QString ConfigManager::configDir() const
{
    return m_configDir;
}

QString ConfigManager::configPath() const
{
    return m_configDir + QStringLiteral("/config.json");
}

QString ConfigManager::syncStatePath() const
{
    return m_configDir + QStringLiteral("/sync_state.json");
}

QString ConfigManager::tokenPath() const
{
    return m_configDir + QStringLiteral("/token.json");
}

bool ConfigManager::load()
{
    if (!ensureConfigDir()) {
        return false;
    }

    QFile file(configPath());
    if (!file.exists()) {
        return save();
    }

    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    if (!doc.isObject()) {
        return false;
    }

    const QJsonObject root = doc.object();
    m_basePath = root.value(QStringLiteral("basePath")).toString();

    m_courseSemesters.clear();
    m_legacyCourseSemestersByName.clear();
    const QJsonObject courseSemesters = root.value(QStringLiteral("courseSemesters")).toObject();
    for (auto it = courseSemesters.begin(); it != courseSemesters.end(); ++it) {
        const QString key = it.key().trimmed();
        const QString value = it.value().toString().trimmed();
        if (key.isEmpty() || value.isEmpty()) {
            continue;
        }

        // courseId actual suele ser numerico; claves no numericas se consideran configuracion legacy por nombre.
        if (QRegularExpression(QStringLiteral("^\\d+$")).match(key).hasMatch()) {
            m_courseSemesters.insert(key, value);
        } else {
            m_legacyCourseSemestersByName.insert(key, value);
        }
    }

    const QJsonObject legacyByName = root.value(QStringLiteral("courseSemestersByName")).toObject();
    for (auto it = legacyByName.begin(); it != legacyByName.end(); ++it) {
        const QString key = it.key().trimmed();
        const QString value = it.value().toString().trimmed();
        if (!key.isEmpty() && !value.isEmpty()) {
            m_legacyCourseSemestersByName.insert(key, value);
        }
    }

    if (root.contains(QStringLiteral("oauth")) && root.value(QStringLiteral("oauth")).isObject()) {
        m_oauth = root.value(QStringLiteral("oauth")).toObject();
    }

    return true;
}

bool ConfigManager::save() const
{
    if (!ensureConfigDir()) {
        return false;
    }

    QJsonObject courseSemesters;
    for (auto it = m_courseSemesters.constBegin(); it != m_courseSemesters.constEnd(); ++it) {
        courseSemesters.insert(it.key(), it.value());
    }
    QJsonObject legacyCourseSemestersByName;
    for (auto it = m_legacyCourseSemestersByName.constBegin(); it != m_legacyCourseSemestersByName.constEnd(); ++it) {
        legacyCourseSemestersByName.insert(it.key(), it.value());
    }

    QJsonObject root;
    root.insert(QStringLiteral("basePath"), m_basePath);
    root.insert(QStringLiteral("courseSemesters"), courseSemesters);
    if (!legacyCourseSemestersByName.isEmpty()) {
        root.insert(QStringLiteral("courseSemestersByName"), legacyCourseSemestersByName);
    }
    root.insert(QStringLiteral("oauth"), m_oauth);

    QFile file(configPath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }

    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    file.close();
    return true;
}

QString ConfigManager::basePath() const
{
    return m_basePath;
}

void ConfigManager::setBasePath(const QString &path)
{
    m_basePath = path;
}

QString ConfigManager::semesterForCourse(const QString &courseId) const
{
    const QString semester = m_courseSemesters.value(courseId).trimmed();
    return semester.isEmpty() ? QStringLiteral("Sin semestre") : semester;
}

void ConfigManager::setSemesterForCourse(const QString &courseId, const QString &semester)
{
    const QString trimmed = semester.trimmed();
    if (trimmed.isEmpty()) {
        m_courseSemesters.remove(courseId);
        return;
    }
    m_courseSemesters.insert(courseId, trimmed);
}

QHash<QString, QString> ConfigManager::semesterMapping() const
{
    return m_courseSemesters;
}

void ConfigManager::setSemesterMapping(const QHash<QString, QString> &mapping)
{
    m_courseSemesters = mapping;
}

QString ConfigManager::legacySemesterForCourseName(const QString &courseName) const
{
    return m_legacyCourseSemestersByName.value(courseName).trimmed();
}

void ConfigManager::clearLegacySemesterForCourseName(const QString &courseName)
{
    m_legacyCourseSemestersByName.remove(courseName);
}

QString ConfigManager::oauthClientId() const
{
    return m_oauth.value(QStringLiteral("clientId")).toString();
}

QString ConfigManager::oauthClientSecret() const
{
    return m_oauth.value(QStringLiteral("clientSecret")).toString();
}

QString ConfigManager::oauthRedirectUri() const
{
    const QString redirectUri = m_oauth.value(QStringLiteral("redirectUri")).toString().trimmed();
    return redirectUri.isEmpty() ? QStringLiteral("urn:ietf:wg:oauth:2.0:oob") : redirectUri;
}

QStringList ConfigManager::oauthScopes() const
{
    QStringList scopes;
    for (const QJsonValue &scope : m_oauth.value(QStringLiteral("scopes")).toArray()) {
        scopes.append(scope.toString());
    }
    return scopes;
}

QJsonObject ConfigManager::oauthObject() const
{
    return m_oauth;
}

void ConfigManager::setOauthObject(const QJsonObject &oauth)
{
    m_oauth = oauth;
}

QJsonObject ConfigManager::loadSyncState() const
{
    if (!ensureConfigDir()) {
        return QJsonObject{{QStringLiteral("assignments"), QJsonObject()}};
    }

    QFile file(syncStatePath());
    if (!file.exists()) {
        return QJsonObject{{QStringLiteral("assignments"), QJsonObject()}};
    }

    if (!file.open(QIODevice::ReadOnly)) {
        return QJsonObject{{QStringLiteral("assignments"), QJsonObject()}};
    }

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    if (!doc.isObject()) {
        return QJsonObject{{QStringLiteral("assignments"), QJsonObject()}};
    }

    QJsonObject state = doc.object();
    if (!state.contains(QStringLiteral("assignments")) || !state.value(QStringLiteral("assignments")).isObject()) {
        state.insert(QStringLiteral("assignments"), QJsonObject());
    }

    return state;
}

bool ConfigManager::saveSyncState(const QJsonObject &state) const
{
    if (!ensureConfigDir()) {
        return false;
    }

    QFile file(syncStatePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }

    file.write(QJsonDocument(state).toJson(QJsonDocument::Indented));
    file.close();
    return true;
}

QJsonObject ConfigManager::loadToken() const
{
    if (!ensureConfigDir()) {
        return {};
    }

    QFile file(tokenPath());
    if (!file.exists()) {
        return {};
    }

    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    if (!doc.isObject()) {
        return {};
    }
    return doc.object();
}

bool ConfigManager::saveToken(const QJsonObject &token) const
{
    if (!ensureConfigDir()) {
        return false;
    }

    QFile file(tokenPath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }

    file.write(QJsonDocument(token).toJson(QJsonDocument::Indented));
    file.close();
    return true;
}
