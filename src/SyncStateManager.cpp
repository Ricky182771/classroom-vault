#include "SyncStateManager.hpp"

#include "Utils.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>

SyncStateManager::SyncStateManager(const QString &statePath)
    : m_statePath(statePath)
{
    ensureDefaultState();
}

void SyncStateManager::setStatePath(const QString &statePath)
{
    m_statePath = statePath;
}

QString SyncStateManager::statePath() const
{
    return m_statePath;
}

void SyncStateManager::ensureDefaultState()
{
    if (!m_root.contains(QStringLiteral("lastSync"))) {
        m_root.insert(QStringLiteral("lastSync"), QString());
    }
    if (!m_root.contains(QStringLiteral("courses")) || !m_root.value(QStringLiteral("courses")).isObject()) {
        m_root.insert(QStringLiteral("courses"), QJsonObject());
    }
}

void SyncStateManager::migrateLegacyFlatAssignments()
{
    if (!m_root.contains(QStringLiteral("assignments")) || m_root.value(QStringLiteral("assignments")).isUndefined()) {
        return;
    }

    const QJsonObject flatAssignments = m_root.value(QStringLiteral("assignments")).toObject();
    if (flatAssignments.isEmpty()) {
        return;
    }

    QJsonObject courses = m_root.value(QStringLiteral("courses")).toObject();

    for (auto it = flatAssignments.begin(); it != flatAssignments.end(); ++it) {
        const QStringList keys = it.key().split(QLatin1Char(':'));
        if (keys.size() != 2) {
            continue;
        }

        const QString courseId = keys.at(0);
        const QString assignmentId = keys.at(1);
        const QJsonObject legacyEntry = it.value().toObject();

        QJsonObject courseEntry = courses.value(courseId).toObject();
        if (!courseEntry.contains(QStringLiteral("assignments")) || !courseEntry.value(QStringLiteral("assignments")).isObject()) {
            courseEntry.insert(QStringLiteral("assignments"), QJsonObject());
        }

        QJsonObject assignments = courseEntry.value(QStringLiteral("assignments")).toObject();

        QJsonObject assignmentEntry;
        assignmentEntry.insert(QStringLiteral("title"), legacyEntry.value(QStringLiteral("title")).toString());
        assignmentEntry.insert(QStringLiteral("folderName"), legacyEntry.value(QStringLiteral("folderName")).toString());
        assignmentEntry.insert(QStringLiteral("folderPath"), legacyEntry.value(QStringLiteral("folderPath")).toString());
        assignmentEntry.insert(QStringLiteral("metadataHash"), legacyEntry.value(QStringLiteral("metadataHash")).toString());

        const QString lastSynced = legacyEntry.value(QStringLiteral("lastSynced")).toString();
        assignmentEntry.insert(QStringLiteral("lastUpdated"), lastSynced);
        assignmentEntry.insert(QStringLiteral("lastSeen"), lastSynced);

        assignments.insert(assignmentId, assignmentEntry);
        courseEntry.insert(QStringLiteral("assignments"), assignments);

        courses.insert(courseId, courseEntry);
    }

    m_root.remove(QStringLiteral("assignments"));
    m_root.insert(QStringLiteral("courses"), courses);
}

bool SyncStateManager::load()
{
    ensureDefaultState();

    if (m_statePath.trimmed().isEmpty()) {
        return false;
    }

    QFile file(m_statePath);
    if (!file.exists()) {
        return true;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    if (!doc.isObject()) {
        return false;
    }

    m_root = doc.object();
    migrateLegacyFlatAssignments();
    ensureDefaultState();
    return true;
}

bool SyncStateManager::save() const
{
    if (m_statePath.trimmed().isEmpty()) {
        return false;
    }

    const QFileInfo info(m_statePath);
    QDir dir;
    if (!dir.exists(info.absolutePath()) && !dir.mkpath(info.absolutePath())) {
        return false;
    }

    QFile file(m_statePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }

    file.write(QJsonDocument(m_root).toJson(QJsonDocument::Indented));
    file.close();
    return true;
}

QJsonObject SyncStateManager::courseObject(const QString &courseId) const
{
    const QJsonObject courses = m_root.value(QStringLiteral("courses")).toObject();
    return courses.value(courseId).toObject();
}

QJsonObject SyncStateManager::assignmentObject(const QString &courseId, const QString &assignmentId) const
{
    const QJsonObject course = courseObject(courseId);
    const QJsonObject assignments = course.value(QStringLiteral("assignments")).toObject();
    return assignments.value(assignmentId).toObject();
}

bool SyncStateManager::hasCourse(const QString &courseId) const
{
    const QJsonObject courses = m_root.value(QStringLiteral("courses")).toObject();
    return courses.contains(courseId);
}

bool SyncStateManager::hasAssignment(const QString &courseId, const QString &assignmentId) const
{
    const QJsonObject assignments = courseObject(courseId).value(QStringLiteral("assignments")).toObject();
    return assignments.contains(assignmentId);
}

QStringList SyncStateManager::courseIds() const
{
    return m_root.value(QStringLiteral("courses")).toObject().keys();
}

QStringList SyncStateManager::assignmentIds(const QString &courseId) const
{
    return courseObject(courseId).value(QStringLiteral("assignments")).toObject().keys();
}

QJsonObject SyncStateManager::courseState(const QString &courseId) const
{
    return courseObject(courseId);
}

QString SyncStateManager::courseFolderPath(const QString &courseId) const
{
    return courseObject(courseId).value(QStringLiteral("folderPath")).toString();
}

QString SyncStateManager::assignmentFolderPath(const QString &courseId, const QString &assignmentId) const
{
    return assignmentObject(courseId, assignmentId).value(QStringLiteral("folderPath")).toString();
}

QString SyncStateManager::assignmentMetadataPath(const QString &courseId, const QString &assignmentId) const
{
    const QString storedPath = assignmentObject(courseId, assignmentId).value(QStringLiteral("metadataPath")).toString().trimmed();
    if (!storedPath.isEmpty()) {
        return storedPath;
    }

    const QString folderPath = assignmentFolderPath(courseId, assignmentId).trimmed();
    if (folderPath.isEmpty()) {
        return QString();
    }
    return QDir(folderPath).filePath(QStringLiteral("metadata.json"));
}

QString SyncStateManager::assignmentMetadataHash(const QString &courseId, const QString &assignmentId) const
{
    return assignmentObject(courseId, assignmentId).value(QStringLiteral("metadataHash")).toString();
}

QJsonObject SyncStateManager::assignmentState(const QString &courseId, const QString &assignmentId) const
{
    return assignmentObject(courseId, assignmentId);
}

bool SyncStateManager::isAssignmentArchivedDeleted(const QString &courseId, const QString &assignmentId) const
{
    const QJsonObject assignmentEntry = assignmentObject(courseId, assignmentId);
    if (assignmentEntry.value(QStringLiteral("isArchivedDeleted")).toBool(false)) {
        return true;
    }

    const QJsonObject archivalStatus = assignmentEntry.value(QStringLiteral("archivalStatus")).toObject();
    return archivalStatus.value(QStringLiteral("status")).toString() == QStringLiteral("deleted_archived");
}

bool SyncStateManager::isAssignmentMetadataChanged(
    const QString &courseId,
    const QString &assignmentId,
    const QJsonObject &metadata) const
{
    if (!hasAssignment(courseId, assignmentId)) {
        return true;
    }

    const QString oldHash = assignmentMetadataHash(courseId, assignmentId);
    const QString newHash = Utils::sha256Json(metadata);
    return oldHash != newHash;
}

void SyncStateManager::updateCourse(const Course &course, const QString &semester, const QString &folderPath)
{
    QJsonObject courses = m_root.value(QStringLiteral("courses")).toObject();
    QJsonObject courseEntry = courses.value(course.id).toObject();

    const QString now = Utils::nowIsoStringUtc();
    courseEntry.insert(QStringLiteral("name"), course.name);
    courseEntry.insert(QStringLiteral("semester"), semester);
    courseEntry.insert(QStringLiteral("folderPath"), folderPath);
    courseEntry.insert(QStringLiteral("lastSeen"), now);

    if (!courseEntry.contains(QStringLiteral("assignments")) || !courseEntry.value(QStringLiteral("assignments")).isObject()) {
        courseEntry.insert(QStringLiteral("assignments"), QJsonObject());
    }

    courses.insert(course.id, courseEntry);
    m_root.insert(QStringLiteral("courses"), courses);
}

void SyncStateManager::updateAssignment(
    const Course &course,
    const Assignment &assignment,
    const QString &folderName,
    const QString &folderPath,
    const QString &metadataPath,
    const QJsonObject &metadata)
{
    QJsonObject courses = m_root.value(QStringLiteral("courses")).toObject();
    QJsonObject courseEntry = courses.value(course.id).toObject();

    if (!courseEntry.contains(QStringLiteral("assignments")) || !courseEntry.value(QStringLiteral("assignments")).isObject()) {
        courseEntry.insert(QStringLiteral("assignments"), QJsonObject());
    }

    QJsonObject assignments = courseEntry.value(QStringLiteral("assignments")).toObject();
    QJsonObject assignmentEntry = assignments.value(assignment.id).toObject();

    const QString newHash = Utils::sha256Json(metadata);
    const QString oldHash = assignmentEntry.value(QStringLiteral("metadataHash")).toString();
    const QString now = Utils::nowIsoStringUtc();

    assignmentEntry.insert(QStringLiteral("title"), Utils::effectiveAssignmentTitle(assignment));
    assignmentEntry.insert(QStringLiteral("state"), assignment.state);
    assignmentEntry.insert(QStringLiteral("workType"), assignment.workType);
    assignmentEntry.insert(QStringLiteral("alternateLink"), assignment.alternateLink);
    assignmentEntry.insert(QStringLiteral("folderName"), folderName);
    assignmentEntry.insert(QStringLiteral("folderPath"), folderPath);
    assignmentEntry.insert(QStringLiteral("metadataPath"), metadataPath);
    assignmentEntry.insert(QStringLiteral("metadataHash"), newHash);
    const QJsonObject submissionObj = metadata.value(QStringLiteral("submission")).toObject();
    if (!submissionObj.isEmpty()) {
        assignmentEntry.insert(QStringLiteral("submission"), submissionObj);
    } else {
        assignmentEntry.remove(QStringLiteral("submission"));
    }
    if (oldHash != newHash || assignmentEntry.value(QStringLiteral("lastUpdated")).toString().isEmpty()) {
        assignmentEntry.insert(QStringLiteral("lastUpdated"), now);
    }
    assignmentEntry.insert(QStringLiteral("lastSeen"), now);
    if (!assignmentEntry.contains(QStringLiteral("attachments")) || !assignmentEntry.value(QStringLiteral("attachments")).isObject()) {
        assignmentEntry.insert(QStringLiteral("attachments"), QJsonObject());
    }
    assignmentEntry.remove(QStringLiteral("isArchivedDeleted"));
    assignmentEntry.remove(QStringLiteral("archivalStatus"));

    assignments.insert(assignment.id, assignmentEntry);
    courseEntry.insert(QStringLiteral("assignments"), assignments);

    courses.insert(course.id, courseEntry);
    m_root.insert(QStringLiteral("courses"), courses);
}

QJsonObject SyncStateManager::assignmentAttachments(const QString &courseId, const QString &assignmentId) const
{
    return assignmentObject(courseId, assignmentId).value(QStringLiteral("attachments")).toObject();
}

QJsonObject SyncStateManager::assignmentAttachmentsState(const QString &courseId, const QString &assignmentId) const
{
    return assignmentAttachments(courseId, assignmentId);
}

QJsonObject SyncStateManager::attachmentRecord(
    const QString &courseId,
    const QString &assignmentId,
    const QString &attachmentKey) const
{
    return assignmentAttachments(courseId, assignmentId).value(attachmentKey).toObject();
}

void SyncStateManager::updateAttachment(
    const QString &courseId,
    const QString &assignmentId,
    const QString &attachmentKey,
    const QJsonObject &attachmentData)
{
    if (courseId.trimmed().isEmpty() || assignmentId.trimmed().isEmpty() || attachmentKey.trimmed().isEmpty()) {
        return;
    }

    QJsonObject courses = m_root.value(QStringLiteral("courses")).toObject();
    QJsonObject courseEntry = courses.value(courseId).toObject();

    if (!courseEntry.contains(QStringLiteral("assignments")) || !courseEntry.value(QStringLiteral("assignments")).isObject()) {
        courseEntry.insert(QStringLiteral("assignments"), QJsonObject());
    }

    QJsonObject assignments = courseEntry.value(QStringLiteral("assignments")).toObject();
    QJsonObject assignmentEntry = assignments.value(assignmentId).toObject();
    if (!assignmentEntry.contains(QStringLiteral("attachments")) || !assignmentEntry.value(QStringLiteral("attachments")).isObject()) {
        assignmentEntry.insert(QStringLiteral("attachments"), QJsonObject());
    }

    QJsonObject attachments = assignmentEntry.value(QStringLiteral("attachments")).toObject();
    attachments.insert(attachmentKey, attachmentData);
    assignmentEntry.insert(QStringLiteral("attachments"), attachments);

    assignments.insert(assignmentId, assignmentEntry);
    courseEntry.insert(QStringLiteral("assignments"), assignments);
    courses.insert(courseId, courseEntry);
    m_root.insert(QStringLiteral("courses"), courses);
}

void SyncStateManager::markAssignmentArchivedDeleted(
    const QString &courseId,
    const QString &assignmentId,
    const QString &reason)
{
    if (courseId.trimmed().isEmpty() || assignmentId.trimmed().isEmpty()) {
        return;
    }

    QJsonObject courses = m_root.value(QStringLiteral("courses")).toObject();
    QJsonObject courseEntry = courses.value(courseId).toObject();

    if (!courseEntry.contains(QStringLiteral("assignments")) || !courseEntry.value(QStringLiteral("assignments")).isObject()) {
        courseEntry.insert(QStringLiteral("assignments"), QJsonObject());
    }

    QJsonObject assignments = courseEntry.value(QStringLiteral("assignments")).toObject();
    QJsonObject assignmentEntry = assignments.value(assignmentId).toObject();

    QJsonObject archival;
    archival.insert(QStringLiteral("status"), QStringLiteral("deleted_archived"));
    archival.insert(QStringLiteral("detectedAt"), Utils::nowIsoStringUtc());
    archival.insert(QStringLiteral("reason"), reason);

    assignmentEntry.insert(QStringLiteral("isArchivedDeleted"), true);
    assignmentEntry.insert(QStringLiteral("archivalStatus"), archival);
    assignmentEntry.insert(QStringLiteral("lastSeen"), Utils::nowIsoStringUtc());

    assignments.insert(assignmentId, assignmentEntry);
    courseEntry.insert(QStringLiteral("assignments"), assignments);
    courses.insert(courseId, courseEntry);
    m_root.insert(QStringLiteral("courses"), courses);
}

void SyncStateManager::clearAssignmentArchivedDeleted(const QString &courseId, const QString &assignmentId)
{
    if (courseId.trimmed().isEmpty() || assignmentId.trimmed().isEmpty()) {
        return;
    }

    QJsonObject courses = m_root.value(QStringLiteral("courses")).toObject();
    QJsonObject courseEntry = courses.value(courseId).toObject();
    QJsonObject assignments = courseEntry.value(QStringLiteral("assignments")).toObject();
    QJsonObject assignmentEntry = assignments.value(assignmentId).toObject();
    if (assignmentEntry.isEmpty()) {
        return;
    }

    assignmentEntry.remove(QStringLiteral("isArchivedDeleted"));
    assignmentEntry.remove(QStringLiteral("archivalStatus"));
    assignments.insert(assignmentId, assignmentEntry);
    courseEntry.insert(QStringLiteral("assignments"), assignments);
    courses.insert(courseId, courseEntry);
    m_root.insert(QStringLiteral("courses"), courses);
}

void SyncStateManager::updateAssignmentChecksumState(
    const QString &courseId,
    const QString &assignmentId,
    const QJsonObject &checksumState)
{
    if (courseId.trimmed().isEmpty() || assignmentId.trimmed().isEmpty()) {
        return;
    }

    QJsonObject courses = m_root.value(QStringLiteral("courses")).toObject();
    QJsonObject courseEntry = courses.value(courseId).toObject();

    if (!courseEntry.contains(QStringLiteral("assignments")) || !courseEntry.value(QStringLiteral("assignments")).isObject()) {
        courseEntry.insert(QStringLiteral("assignments"), QJsonObject());
    }

    QJsonObject assignments = courseEntry.value(QStringLiteral("assignments")).toObject();
    QJsonObject assignmentEntry = assignments.value(assignmentId).toObject();
    assignmentEntry.insert(QStringLiteral("checksum"), checksumState);
    assignments.insert(assignmentId, assignmentEntry);
    courseEntry.insert(QStringLiteral("assignments"), assignments);
    courses.insert(courseId, courseEntry);
    m_root.insert(QStringLiteral("courses"), courses);
}

QString SyncStateManager::lastSync() const
{
    return m_root.value(QStringLiteral("lastSync")).toString();
}

void SyncStateManager::setLastSync(const QDateTime &dateTime)
{
    m_root.insert(QStringLiteral("lastSync"), dateTime.toString(Qt::ISODate));
}

bool SyncStateManager::localCourseFolderExists(const QString &courseId) const
{
    const QString path = courseFolderPath(courseId).trimmed();
    return !path.isEmpty() && QFileInfo(path).exists() && QFileInfo(path).isDir();
}

bool SyncStateManager::localAssignmentFolderExists(const QString &courseId, const QString &assignmentId) const
{
    const QString path = assignmentFolderPath(courseId, assignmentId).trimmed();
    return !path.isEmpty() && QFileInfo(path).exists() && QFileInfo(path).isDir();
}

bool SyncStateManager::localMetadataExists(const QString &courseId, const QString &assignmentId) const
{
    const QString path = assignmentMetadataPath(courseId, assignmentId).trimmed();
    return !path.isEmpty() && QFileInfo(path).exists() && QFileInfo(path).isFile();
}
