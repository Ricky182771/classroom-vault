#include "FolderOrganizer.hpp"

#include "Utils.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QRegularExpression>

void FolderOrganizer::setBasePath(const QString &basePath)
{
    m_basePath = QDir::cleanPath(basePath.trimmed());
}

QString FolderOrganizer::basePath() const
{
    return m_basePath;
}

QString FolderOrganizer::tasksRootPath() const
{
    return QDir(m_basePath).filePath(QStringLiteral("Tareas"));
}

QString FolderOrganizer::sanitizeName(const QString &name)
{
    QString cleaned = name;
    cleaned.replace(QRegularExpression(QStringLiteral("[/\\\\:*?\"<>|]")), QStringLiteral(" "));
    cleaned = cleaned.simplified().trimmed();

    if (cleaned.isEmpty()) {
        cleaned = QStringLiteral("Sin nombre");
    }

    constexpr int maxLen = 120;
    if (cleaned.size() > maxLen) {
        cleaned = cleaned.left(maxLen).trimmed();
    }

    return cleaned;
}

bool FolderOrganizer::ensureDir(const QString &path) const
{
    QDir dir;
    if (dir.exists(path)) {
        return true;
    }
    return dir.mkpath(path);
}

QString FolderOrganizer::createSemesterFolder(const QString &semester) const
{
    const QString semesterName = sanitizeName(semester.trimmed().isEmpty() ? QStringLiteral("Sin semestre") : semester);
    const QString semesterPath = QDir(tasksRootPath()).filePath(semesterName);
    ensureDir(semesterPath);
    return semesterPath;
}

QString FolderOrganizer::createCourseFolder(const QString &semester, const QString &courseName) const
{
    const QString semesterPath = createSemesterFolder(semester);
    const QString courseFolderName = sanitizeName(courseName.trimmed().isEmpty() ? QStringLiteral("Materia sin nombre") : courseName);
    const QString coursePath = QDir(semesterPath).filePath(courseFolderName);
    ensureDir(coursePath);
    return coursePath;
}

QString FolderOrganizer::metadataAssignmentId(const QString &assignmentDir) const
{
    QFile metaFile(QDir(assignmentDir).filePath(QStringLiteral("metadata.json")));
    if (!metaFile.exists() || !metaFile.open(QIODevice::ReadOnly)) {
        return QString();
    }

    const QJsonDocument doc = QJsonDocument::fromJson(metaFile.readAll());
    metaFile.close();
    if (!doc.isObject()) {
        return QString();
    }

    return doc.object().value(QStringLiteral("assignmentId")).toString();
}

QString FolderOrganizer::makeUniqueAssignmentName(
    const QString &coursePath,
    const QString &preferredName,
    const Assignment &assignment) const
{
    QString candidate = sanitizeName(preferredName);
    QString candidatePath = QDir(coursePath).filePath(candidate);

    if (!QDir(candidatePath).exists()) {
        return candidate;
    }

    if (metadataAssignmentId(candidatePath) == assignment.id) {
        return candidate;
    }

    const QString idSuffix = assignment.id.isEmpty() ? QStringLiteral("unknown") : assignment.id.left(8);
    candidate = sanitizeName(preferredName + QStringLiteral(" [") + idSuffix + QStringLiteral("]"));
    candidatePath = QDir(coursePath).filePath(candidate);

    if (!QDir(candidatePath).exists() || metadataAssignmentId(candidatePath) == assignment.id) {
        return candidate;
    }

    int index = 2;
    while (index < 1000) {
        const QString indexed = sanitizeName(candidate + QStringLiteral(" (") + QString::number(index) + QStringLiteral(")"));
        const QString indexedPath = QDir(coursePath).filePath(indexed);
        if (!QDir(indexedPath).exists() || metadataAssignmentId(indexedPath) == assignment.id) {
            return indexed;
        }
        ++index;
    }

    return candidate;
}

QString FolderOrganizer::createAssignmentFolder(
    const QString &semester,
    const QString &courseName,
    const Assignment &assignment) const
{
    const QString coursePath = createCourseFolder(semester, courseName);
    QString assignmentName = Utils::assignmentFolderLabel(assignment);
    assignmentName = sanitizeName(assignmentName);

    const QString finalName = makeUniqueAssignmentName(coursePath, assignmentName, assignment);
    const QString assignmentPath = QDir(coursePath).filePath(finalName);
    ensureDir(assignmentPath);

    return assignmentPath;
}

bool FolderOrganizer::writeMetadata(const QString &filePath, const QJsonObject &metadata) const
{
    const QFileInfo info(filePath);
    ensureDir(info.absolutePath());

    QFile file(filePath);

    if (file.exists()) {
        if (!file.open(QIODevice::ReadOnly)) {
            return false;
        }
        const QJsonDocument existingDoc = QJsonDocument::fromJson(file.readAll());
        file.close();

        if (existingDoc.isObject() && existingDoc.object() == metadata) {
            return false;
        }
    }

    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }

    file.write(QJsonDocument(metadata).toJson(QJsonDocument::Indented));
    file.close();
    return true;
}
