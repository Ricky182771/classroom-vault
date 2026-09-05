#include "SyncStateManager.hpp"

#include "Utils.hpp"

#include <QDir>
#include <QHash>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>

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

void SyncStateManager::reset()
{
    m_root = QJsonObject();
    ensureDefaultState();
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

void SyncStateManager::setCourseStateRaw(const QString &courseId, const QJsonObject &courseState)
{
    const QString cleanCourseId = courseId.trimmed();
    if (cleanCourseId.isEmpty() || courseState.isEmpty()) {
        return;
    }

    QJsonObject courses = m_root.value(QStringLiteral("courses")).toObject();
    courses.insert(cleanCourseId, courseState);
    m_root.insert(QStringLiteral("courses"), courses);
}

bool SyncStateManager::removeCourse(const QString &courseId)
{
    const QString cleanCourseId = courseId.trimmed();
    QJsonObject courses = m_root.value(QStringLiteral("courses")).toObject();
    if (!courses.contains(cleanCourseId)) {
        return false;
    }

    courses.remove(cleanCourseId);
    m_root.insert(QStringLiteral("courses"), courses);
    return true;
}

QString SyncStateManager::courseFolderPath(const QString &courseId) const
{
    return courseObject(courseId).value(QStringLiteral("folderPath")).toString();
}

QString SyncStateManager::courseSemester(const QString &courseId) const
{
    return courseObject(courseId).value(QStringLiteral("semester")).toString().trimmed();
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

int PathRebasePlan::toMigrate() const
{
    int count = 0;
    for (const PathRebaseEntry &entry : entries) {
        if (!entry.alreadyMigrated) {
            ++count;
        }
    }
    return count;
}

int PathRebasePlan::alreadyMigrated() const
{
    return static_cast<int>(entries.size()) - toMigrate();
}

int PathRebasePlan::missingAfterMigration() const
{
    int count = 0;
    for (const PathRebaseEntry &entry : entries) {
        if (!entry.existsAtNewPath) {
            ++count;
        }
    }
    return count;
}

namespace {

QString cleanBase(const QString &base)
{
    QString clean = QDir::cleanPath(base.trimmed());
    while (clean.endsWith(QLatin1Char('/')) && clean.size() > 1) {
        clean.chop(1);
    }
    return clean;
}

bool pathHasBase(const QString &path, const QString &base)
{
    if (path.isEmpty() || base.isEmpty()) {
        return false;
    }
    const QString cleanPath = QDir::cleanPath(path);
    return cleanPath == base || cleanPath.startsWith(base + QLatin1Char('/'));
}

QString rebasedPath(const QString &path, const QString &oldBase, const QString &newBase)
{
    const QString cleanPath = QDir::cleanPath(path);
    if (cleanPath == oldBase) {
        return newBase;
    }
    return newBase + cleanPath.mid(oldBase.size());
}

} // namespace

QString SyncStateManager::detectPreviousBasePath(const QString &currentBase) const
{
    // Toda ruta la construye FolderOrganizer como <base>/Tareas/<semestre>/...,
    // asi que la base anterior es lo que precede a ese "/Tareas/".
    static const QString marker = QStringLiteral("/Tareas/");
    const QString base = cleanBase(currentBase);

    QHash<QString, int> candidates;
    const QJsonObject courses = m_root.value(QStringLiteral("courses")).toObject();
    for (auto it = courses.begin(); it != courses.end(); ++it) {
        const QString folderPath = QDir::cleanPath(
            it.value().toObject().value(QStringLiteral("folderPath")).toString().trimmed());
        if (folderPath.isEmpty() || pathHasBase(folderPath, base)) {
            continue;
        }

        const int markerIndex = folderPath.indexOf(marker);
        if (markerIndex <= 0) {
            continue;
        }

        ++candidates[folderPath.left(markerIndex)];
    }

    // Puede haber restos de varias bases (pruebas en /tmp, por ejemplo). Se elige
    // la mayoritaria; en empate no se adivina y el usuario debe indicarla.
    QString detected;
    int best = 0;
    bool tied = false;
    for (auto it = candidates.constBegin(); it != candidates.constEnd(); ++it) {
        if (it.value() > best) {
            best = it.value();
            detected = it.key();
            tied = false;
        } else if (it.value() == best) {
            tied = true;
        }
    }

    return tied ? QString() : detected;
}

PathRebasePlan SyncStateManager::rebasePaths(const QString &oldBase, const QString &newBase, bool apply)
{
    PathRebasePlan plan;
    plan.oldBase = cleanBase(oldBase);
    plan.newBase = cleanBase(newBase);
    if (plan.oldBase.isEmpty() || plan.newBase.isEmpty() || plan.oldBase == plan.newBase) {
        return plan;
    }

    QJsonObject courses = m_root.value(QStringLiteral("courses")).toObject();

    const auto rebaseField = [&plan](QJsonObject &object,
                                     const QString &field,
                                     const QString &courseId,
                                     const QString &context) {
        const QString current = object.value(field).toString().trimmed();
        if (current.isEmpty()) {
            return;
        }

        PathRebaseEntry entry;
        entry.courseId = courseId;
        entry.context = context;
        entry.field = field;
        entry.oldPath = current;

        if (pathHasBase(current, plan.newBase)) {
            // Idempotencia: ya migrada, se deja tal cual.
            entry.alreadyMigrated = true;
            entry.newPath = QDir::cleanPath(current);
        } else if (pathHasBase(current, plan.oldBase)) {
            entry.newPath = rebasedPath(current, plan.oldBase, plan.newBase);
        } else {
            // Ruta ajena a ambas bases: no se toca ni se reporta.
            return;
        }

        entry.existsAtNewPath = QFileInfo::exists(entry.newPath);
        plan.entries.append(entry);
        object.insert(field, entry.newPath);
    };

    for (auto courseIt = courses.begin(); courseIt != courses.end(); ++courseIt) {
        const QString courseId = courseIt.key();
        QJsonObject course = courseIt.value().toObject();
        const QString courseName = course.value(QStringLiteral("name")).toString();

        rebaseField(course, QStringLiteral("folderPath"), courseId, courseName);

        QJsonObject assignments = course.value(QStringLiteral("assignments")).toObject();
        for (auto assignmentIt = assignments.begin(); assignmentIt != assignments.end(); ++assignmentIt) {
            QJsonObject assignment = assignmentIt.value().toObject();
            const QString title = assignment.value(QStringLiteral("title")).toString();
            const QString context = QStringLiteral("%1 / %2").arg(courseName, title.isEmpty() ? assignmentIt.key() : title);

            rebaseField(assignment, QStringLiteral("folderPath"), courseId, context);
            rebaseField(assignment, QStringLiteral("metadataPath"), courseId, context);

            QJsonObject attachments = assignment.value(QStringLiteral("attachments")).toObject();
            for (auto attachmentIt = attachments.begin(); attachmentIt != attachments.end(); ++attachmentIt) {
                QJsonObject attachment = attachmentIt.value().toObject();
                const QString fileName = attachment.value(QStringLiteral("localFileName")).toString();
                rebaseField(attachment,
                            QStringLiteral("localPath"),
                            courseId,
                            QStringLiteral("%1 / %2").arg(context, fileName.isEmpty() ? attachmentIt.key() : fileName));
                attachments.insert(attachmentIt.key(), attachment);
            }
            if (!attachments.isEmpty()) {
                assignment.insert(QStringLiteral("attachments"), attachments);
            }

            assignments.insert(assignmentIt.key(), assignment);
        }
        if (!assignments.isEmpty()) {
            course.insert(QStringLiteral("assignments"), assignments);
        }

        QJsonObject publications = course.value(QStringLiteral("publications")).toObject();
        for (auto publicationIt = publications.begin(); publicationIt != publications.end(); ++publicationIt) {
            QJsonObject publication = publicationIt.value().toObject();
            const QString title = publication.value(QStringLiteral("title")).toString();
            const QString context = QStringLiteral("%1 / %2").arg(courseName, title.isEmpty() ? publicationIt.key() : title);

            rebaseField(publication, QStringLiteral("folderPath"), courseId, context);
            rebaseField(publication, QStringLiteral("metadataPath"), courseId, context);

            publications.insert(publicationIt.key(), publication);
        }
        if (!publications.isEmpty()) {
            course.insert(QStringLiteral("publications"), publications);
        }

        courses.insert(courseId, course);
    }

    // Todo lo anterior se calcula sobre copias: sin apply, m_root queda intacto.
    if (apply) {
        m_root.insert(QStringLiteral("courses"), courses);
    }

    return plan;
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

QJsonObject SyncStateManager::publicationObject(const QString &courseId, const QString &publicationId) const
{
    const QJsonObject course = courseObject(courseId);
    const QJsonObject publications = course.value(QStringLiteral("publications")).toObject();
    return publications.value(publicationId).toObject();
}

bool SyncStateManager::hasPublication(const QString &courseId, const QString &publicationId) const
{
    const QJsonObject publications = courseObject(courseId).value(QStringLiteral("publications")).toObject();
    return publications.contains(publicationId);
}

QStringList SyncStateManager::publicationIds(const QString &courseId) const
{
    return courseObject(courseId).value(QStringLiteral("publications")).toObject().keys();
}

QJsonObject SyncStateManager::publicationState(const QString &courseId, const QString &publicationId) const
{
    return publicationObject(courseId, publicationId);
}

QString SyncStateManager::publicationMetadataHash(const QString &courseId, const QString &publicationId) const
{
    return publicationObject(courseId, publicationId).value(QStringLiteral("metadataHash")).toString();
}

QString SyncStateManager::publicationFolderPath(const QString &courseId, const QString &publicationId) const
{
    return publicationObject(courseId, publicationId).value(QStringLiteral("folderPath")).toString();
}

bool SyncStateManager::localPublicationFolderExists(const QString &courseId, const QString &publicationId) const
{
    const QString path = publicationFolderPath(courseId, publicationId).trimmed();
    return !path.isEmpty() && QFileInfo(path).exists() && QFileInfo(path).isDir();
}

bool SyncStateManager::localPublicationMetadataExists(const QString &courseId, const QString &publicationId) const
{
    const QString folderPath = publicationFolderPath(courseId, publicationId).trimmed();
    if (folderPath.isEmpty()) {
        return false;
    }
    const QString metaPath = QDir(folderPath).filePath(QStringLiteral("metadata.json"));
    return QFileInfo(metaPath).exists() && QFileInfo(metaPath).isFile();
}

bool SyncStateManager::isPublicationArchivedDeleted(const QString &courseId, const QString &publicationId) const
{
    const QJsonObject entry = publicationObject(courseId, publicationId);
    if (entry.value(QStringLiteral("isArchivedDeleted")).toBool(false)) {
        return true;
    }
    return entry.value(QStringLiteral("archivalStatus")).toObject()
                .value(QStringLiteral("status")).toString() == QStringLiteral("deleted_archived");
}

void SyncStateManager::updatePublication(
    const QString &courseId,
    const Publication &publication,
    const QString &folderPath,
    const QString &metadataPath,
    const QJsonObject &metadata)
{
    QJsonObject courses = m_root.value(QStringLiteral("courses")).toObject();
    QJsonObject courseEntry = courses.value(courseId).toObject();

    if (!courseEntry.contains(QStringLiteral("publications")) || !courseEntry.value(QStringLiteral("publications")).isObject()) {
        courseEntry.insert(QStringLiteral("publications"), QJsonObject());
    }

    QJsonObject publications = courseEntry.value(QStringLiteral("publications")).toObject();
    QJsonObject entry = publications.value(publication.id).toObject();

    const QString newHash = Utils::sha256Json(metadata);
    const QString oldHash = entry.value(QStringLiteral("metadataHash")).toString();
    const QString now = Utils::nowIsoStringUtc();

    entry.insert(QStringLiteral("title"), publication.title);
    entry.insert(QStringLiteral("kind"), publication.kind == PublicationKind::Announcement
        ? QStringLiteral("announcement")
        : QStringLiteral("material"));
    entry.insert(QStringLiteral("state"), publication.state);
    entry.insert(QStringLiteral("alternateLink"), publication.alternateLink);
    entry.insert(QStringLiteral("folderPath"), folderPath);
    entry.insert(QStringLiteral("metadataPath"), metadataPath);
    entry.insert(QStringLiteral("metadataHash"), newHash);
    if (oldHash != newHash || entry.value(QStringLiteral("lastUpdated")).toString().isEmpty()) {
        entry.insert(QStringLiteral("lastUpdated"), now);
    }
    entry.insert(QStringLiteral("lastSeen"), now);
    entry.remove(QStringLiteral("isArchivedDeleted"));
    entry.remove(QStringLiteral("archivalStatus"));

    publications.insert(publication.id, entry);
    courseEntry.insert(QStringLiteral("publications"), publications);
    courses.insert(courseId, courseEntry);
    m_root.insert(QStringLiteral("courses"), courses);
}

void SyncStateManager::markPublicationArchivedDeleted(
    const QString &courseId,
    const QString &publicationId,
    const QString &reason)
{
    if (courseId.trimmed().isEmpty() || publicationId.trimmed().isEmpty()) {
        return;
    }

    QJsonObject courses = m_root.value(QStringLiteral("courses")).toObject();
    QJsonObject courseEntry = courses.value(courseId).toObject();
    QJsonObject publications = courseEntry.value(QStringLiteral("publications")).toObject();
    QJsonObject entry = publications.value(publicationId).toObject();

    QJsonObject archival;
    archival.insert(QStringLiteral("status"), QStringLiteral("deleted_archived"));
    archival.insert(QStringLiteral("detectedAt"), Utils::nowIsoStringUtc());
    archival.insert(QStringLiteral("reason"), reason);

    entry.insert(QStringLiteral("isArchivedDeleted"), true);
    entry.insert(QStringLiteral("archivalStatus"), archival);
    entry.insert(QStringLiteral("lastSeen"), Utils::nowIsoStringUtc());

    publications.insert(publicationId, entry);
    courseEntry.insert(QStringLiteral("publications"), publications);
    courses.insert(courseId, courseEntry);
    m_root.insert(QStringLiteral("courses"), courses);
}

void SyncStateManager::clearPublicationArchivedDeleted(const QString &courseId, const QString &publicationId)
{
    if (courseId.trimmed().isEmpty() || publicationId.trimmed().isEmpty()) {
        return;
    }

    QJsonObject courses = m_root.value(QStringLiteral("courses")).toObject();
    QJsonObject courseEntry = courses.value(courseId).toObject();
    QJsonObject publications = courseEntry.value(QStringLiteral("publications")).toObject();
    QJsonObject entry = publications.value(publicationId).toObject();
    if (entry.isEmpty()) {
        return;
    }

    entry.remove(QStringLiteral("isArchivedDeleted"));
    entry.remove(QStringLiteral("archivalStatus"));
    publications.insert(publicationId, entry);
    courseEntry.insert(QStringLiteral("publications"), publications);
    courses.insert(courseId, courseEntry);
    m_root.insert(QStringLiteral("courses"), courses);
}
