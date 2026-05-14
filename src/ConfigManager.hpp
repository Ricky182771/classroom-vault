#pragma once

#include <QHash>
#include <QJsonObject>
#include <QString>
#include <QStringList>

class ConfigManager {
public:
    ConfigManager();

    bool load();
    bool save() const;

    QString configDir() const;
    QString configPath() const;
    QString syncStatePath() const;
    QString tokenPath() const;

    QString basePath() const;
    void setBasePath(const QString &path);

    QString semesterForCourse(const QString &courseId) const;
    void setSemesterForCourse(const QString &courseId, const QString &semester);
    QHash<QString, QString> semesterMapping() const;
    void setSemesterMapping(const QHash<QString, QString> &mapping);
    QString globalSemesterFilter() const;
    void setGlobalSemesterFilter(const QString &semester);
    QString defaultSemester() const;
    void setDefaultSemester(const QString &semester);
    QString legacySemesterForCourseName(const QString &courseName) const;
    void clearLegacySemesterForCourseName(const QString &courseName);

    QString oauthClientId() const;
    QString oauthClientSecret() const;
    QString oauthRedirectUri() const;
    QString oauthTokenUri() const;
    QStringList oauthScopes() const;
    QJsonObject oauthObject() const;

    void setOauthObject(const QJsonObject &oauth);

    QJsonObject loadSyncState() const;
    bool saveSyncState(const QJsonObject &state) const;

    QJsonObject loadToken() const;
    bool saveToken(const QJsonObject &token) const;

private:
    bool ensureConfigDir() const;
    void loadDefaults();

    QString m_configDir;
    QString m_basePath;
    QHash<QString, QString> m_courseSemesters;
    QHash<QString, QString> m_legacyCourseSemestersByName;
    QString m_globalSemesterFilter;
    QString m_defaultSemester;
    QJsonObject m_oauth;
};
