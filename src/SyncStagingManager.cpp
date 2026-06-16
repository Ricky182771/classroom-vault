#include "SyncStagingManager.hpp"
#include "Paths.hpp"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>

SyncStagingManager::SyncStagingManager(QObject *parent)
    : QObject(parent)
{}

bool SyncStagingManager::beginSession(const QString &mode)
{
    m_preserveOnCleanup = false;
    m_sessionPath.clear();

    const QString rootPath = baseStagingPath();
    QDir dir;
    if (!dir.exists(rootPath) && !dir.mkpath(rootPath)) {
        return false;
    }

    const QString stamp = QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMdd_HHmmss_zzz"));
    const QString safeMode = mode.trimmed().isEmpty() ? QStringLiteral("default") : mode.trimmed();
    m_sessionPath = QDir(rootPath).filePath(QStringLiteral("sync_session_%1_%2").arg(stamp, safeMode));

    return dir.mkpath(m_sessionPath);
}

QString SyncStagingManager::sessionPath() const
{
    return m_sessionPath;
}

bool SyncStagingManager::writeCourse(const Course &course, const QJsonObject &rawCourseJson)
{
    if (m_sessionPath.isEmpty() || course.id.trimmed().isEmpty()) {
        return false;
    }

    const QString dirPath = courseDirPath(course.id);
    QDir dir;
    if (!dir.exists(dirPath) && !dir.mkpath(dirPath)) {
        return false;
    }

    QJsonObject courseObj = rawCourseJson;
    courseObj.insert(QStringLiteral("id"), course.id);
    courseObj.insert(QStringLiteral("name"), course.name);
    courseObj.insert(QStringLiteral("section"), course.section);
    courseObj.insert(QStringLiteral("descriptionHeading"), course.descriptionHeading);
    courseObj.insert(QStringLiteral("alternateLink"), course.alternateLink);
    courseObj.insert(QStringLiteral("stagedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));

    return writeJsonFile(QDir(dirPath).filePath(QStringLiteral("course.json")), courseObj);
}

bool SyncStagingManager::writeAssignment(
    const QString &courseId,
    const QString &assignmentId,
    const QJsonObject &metadata)
{
    if (m_sessionPath.isEmpty() || courseId.trimmed().isEmpty() || assignmentId.trimmed().isEmpty()) {
        return false;
    }

    const QString assignmentsDirPath = QDir(courseDirPath(courseId)).filePath(QStringLiteral("assignments"));
    QDir dir;
    if (!dir.exists(assignmentsDirPath) && !dir.mkpath(assignmentsDirPath)) {
        return false;
    }

    return writeJsonFile(
        QDir(assignmentsDirPath).filePath(assignmentId + QStringLiteral(".json")),
        metadata);
}

bool SyncStagingManager::writeCourseManifest(
    const QString &courseId,
    const QString &courseName,
    const QStringList &assignmentIds,
    bool fetchComplete,
    const QString &errorMessage)
{
    if (m_sessionPath.isEmpty() || courseId.trimmed().isEmpty()) {
        return false;
    }

    QJsonArray idsArray;
    for (const QString &assignmentId : assignmentIds) {
        idsArray.append(assignmentId);
    }

    QJsonObject manifest;
    manifest.insert(QStringLiteral("courseId"), courseId);
    manifest.insert(QStringLiteral("courseName"), courseName);
    manifest.insert(QStringLiteral("fetchedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    manifest.insert(QStringLiteral("fetchStatus"), fetchComplete ? QStringLiteral("complete") : QStringLiteral("incomplete"));
    manifest.insert(QStringLiteral("assignmentCount"), assignmentIds.size());
    manifest.insert(QStringLiteral("assignments"), idsArray);
    if (!errorMessage.trimmed().isEmpty()) {
        manifest.insert(QStringLiteral("error"), errorMessage.trimmed());
    }

    return writeJsonFile(
        QDir(courseDirPath(courseId)).filePath(QStringLiteral("manifest.json")),
        manifest);
}

bool SyncStagingManager::writeGlobalManifest(
    const QStringList &courseIds,
    bool fetchComplete,
    const QString &errorMessage)
{
    if (m_sessionPath.isEmpty()) {
        return false;
    }

    QJsonArray courseArray;
    for (const QString &courseId : courseIds) {
        courseArray.append(courseId);
    }

    QJsonObject manifest;
    manifest.insert(QStringLiteral("fetchedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    manifest.insert(QStringLiteral("fetchStatus"), fetchComplete ? QStringLiteral("complete") : QStringLiteral("incomplete"));
    manifest.insert(QStringLiteral("courseCount"), courseIds.size());
    manifest.insert(QStringLiteral("courses"), courseArray);
    if (!errorMessage.trimmed().isEmpty()) {
        manifest.insert(QStringLiteral("error"), errorMessage.trimmed());
    }

    return writeJsonFile(QDir(m_sessionPath).filePath(QStringLiteral("sync_manifest.json")), manifest);
}

bool SyncStagingManager::writePublication(
    const QString &courseId,
    const QString &publicationId,
    const QJsonObject &metadata)
{
    if (m_sessionPath.isEmpty() || courseId.trimmed().isEmpty() || publicationId.trimmed().isEmpty()) {
        return false;
    }

    const QString pubDirPath = QDir(courseDirPath(courseId)).filePath(QStringLiteral("publications"));
    QDir dir;
    if (!dir.exists(pubDirPath) && !dir.mkpath(pubDirPath)) {
        return false;
    }

    return writeJsonFile(
        QDir(pubDirPath).filePath(publicationId + QStringLiteral(".json")),
        metadata);
}

bool SyncStagingManager::writeCoursePublicationsManifest(
    const QString &courseId,
    const QStringList &publicationIds,
    bool fetchComplete)
{
    if (m_sessionPath.isEmpty() || courseId.trimmed().isEmpty()) {
        return false;
    }

    QJsonArray idsArray;
    for (const QString &id : publicationIds) {
        idsArray.append(id);
    }

    QJsonObject manifest;
    manifest.insert(QStringLiteral("fetchStatus"), fetchComplete ? QStringLiteral("complete") : QStringLiteral("incomplete"));
    manifest.insert(QStringLiteral("publicationCount"), publicationIds.size());
    manifest.insert(QStringLiteral("publications"), idsArray);

    return writeJsonFile(
        QDir(courseDirPath(courseId)).filePath(QStringLiteral("publications_manifest.json")),
        manifest);
}

QJsonObject SyncStagingManager::readAssignmentMetadata(
    const QString &courseId,
    const QString &assignmentId) const
{
    if (m_sessionPath.isEmpty() || courseId.trimmed().isEmpty() || assignmentId.trimmed().isEmpty()) {
        return QJsonObject();
    }

    return readJsonFile(
        QDir(courseDirPath(courseId))
            .filePath(QStringLiteral("assignments/%1.json").arg(assignmentId)));
}

QJsonObject SyncStagingManager::readPublicationMetadata(
    const QString &courseId,
    const QString &publicationId) const
{
    if (m_sessionPath.isEmpty() || courseId.trimmed().isEmpty() || publicationId.trimmed().isEmpty()) {
        return QJsonObject();
    }

    return readJsonFile(
        QDir(courseDirPath(courseId))
            .filePath(QStringLiteral("publications/%1.json").arg(publicationId)));
}

QStringList SyncStagingManager::stagedAssignmentIds(const QString &courseId) const
{
    if (m_sessionPath.isEmpty() || courseId.trimmed().isEmpty()) {
        return {};
    }

    const QJsonObject manifest = readJsonFile(QDir(courseDirPath(courseId)).filePath(QStringLiteral("manifest.json")));
    const QJsonArray idsArray = manifest.value(QStringLiteral("assignments")).toArray();
    QStringList ids;
    ids.reserve(idsArray.size());
    for (const QJsonValue &value : idsArray) {
        const QString id = value.toString().trimmed();
        if (!id.isEmpty()) {
            ids.append(id);
        }
    }
    return ids;
}

QStringList SyncStagingManager::stagedPublicationIds(const QString &courseId) const
{
    if (m_sessionPath.isEmpty() || courseId.trimmed().isEmpty()) {
        return {};
    }

    const QJsonObject manifest = readJsonFile(QDir(courseDirPath(courseId)).filePath(QStringLiteral("publications_manifest.json")));
    const QJsonArray idsArray = manifest.value(QStringLiteral("publications")).toArray();
    QStringList ids;
    ids.reserve(idsArray.size());
    for (const QJsonValue &value : idsArray) {
        const QString id = value.toString().trimmed();
        if (!id.isEmpty()) {
            ids.append(id);
        }
    }
    return ids;
}

bool SyncStagingManager::courseFetchComplete(const QString &courseId) const
{
    if (m_sessionPath.isEmpty() || courseId.trimmed().isEmpty()) {
        return false;
    }

    const QJsonObject manifest = readJsonFile(QDir(courseDirPath(courseId)).filePath(QStringLiteral("manifest.json")));
    return manifest.value(QStringLiteral("fetchStatus")).toString() == QStringLiteral("complete");
}

bool SyncStagingManager::coursePublicationsFetchComplete(const QString &courseId) const
{
    if (m_sessionPath.isEmpty() || courseId.trimmed().isEmpty()) {
        return false;
    }

    const QJsonObject manifest = readJsonFile(QDir(courseDirPath(courseId)).filePath(QStringLiteral("publications_manifest.json")));
    return manifest.value(QStringLiteral("fetchStatus")).toString() == QStringLiteral("complete");
}

void SyncStagingManager::cleanup()
{
    if (m_preserveOnCleanup || m_sessionPath.trimmed().isEmpty()) {
        return;
    }

    QDir dir(m_sessionPath);
    dir.removeRecursively();
    m_sessionPath.clear();
}

void SyncStagingManager::preserveForDebug()
{
    m_preserveOnCleanup = true;
}

bool SyncStagingManager::writeJsonFile(const QString &path, const QJsonObject &json) const
{
    const QFileInfo info(path);
    QDir dir;
    if (!dir.exists(info.absolutePath()) && !dir.mkpath(info.absolutePath())) {
        return false;
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }

    file.write(QJsonDocument(json).toJson(QJsonDocument::Indented));
    file.close();
    return true;
}

QJsonObject SyncStagingManager::readJsonFile(const QString &path) const
{
    QFile file(path);
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
        return QJsonObject();
    }

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    if (!doc.isObject()) {
        return QJsonObject();
    }
    return doc.object();
}

QString SyncStagingManager::baseStagingPath() const
{
    return QDir(Paths::cacheDir()).filePath(QStringLiteral("sync_staging"));
}

QString SyncStagingManager::courseDirPath(const QString &courseId) const
{
    return QDir(QDir(m_sessionPath).filePath(QStringLiteral("courses")))
        .filePath(courseId.trimmed());
}
