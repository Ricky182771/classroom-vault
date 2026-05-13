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
    return sanitizeFileName(name);
}

QString FolderOrganizer::sanitizeFileName(QString name)
{
    name.replace(QRegularExpression(QStringLiteral("[\\r\\n\\t]+")), QStringLiteral(" "));
    name.replace(QRegularExpression(QStringLiteral("[/\\\\:*?\"<>|]")), QStringLiteral(" "));
    name = name.simplified().trimmed();

    while (!name.isEmpty() && (name.endsWith(QLatin1Char(' ')) || name.endsWith(QLatin1Char('.')))) {
        name.chop(1);
    }

    if (name.isEmpty()) {
        name = QStringLiteral("Sin nombre");
    }

    constexpr int maxLen = 120;
    if (name.size() > maxLen) {
        name = name.left(maxLen).trimmed();
    }

    while (!name.isEmpty() && (name.endsWith(QLatin1Char(' ')) || name.endsWith(QLatin1Char('.')))) {
        name.chop(1);
    }

    return name.isEmpty() ? QStringLiteral("Sin nombre") : name;
}

QString FolderOrganizer::buildAssignmentFolderName(const Assignment &assignment)
{
    return sanitizeFileName(Utils::assignmentFolderLabel(assignment));
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
    const QString semesterName = sanitizeFileName(semester.trimmed().isEmpty() ? QStringLiteral("Sin semestre") : semester);
    const QString semesterPath = QDir(tasksRootPath()).filePath(semesterName);
    ensureDir(semesterPath);
    return semesterPath;
}

QString FolderOrganizer::createCourseFolder(const QString &semester, const QString &courseName) const
{
    const QString semesterPath = createSemesterFolder(semester);
    const QString courseFolderName = sanitizeFileName(courseName.trimmed().isEmpty() ? QStringLiteral("Materia sin nombre") : courseName);
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

QString FolderOrganizer::resolveFolderConflict(const QString &desiredPath, const QString &assignmentId) const
{
    if (!QFileInfo::exists(desiredPath)) {
        return desiredPath;
    }

    if (metadataAssignmentId(desiredPath) == assignmentId) {
        return desiredPath;
    }

    const QFileInfo desiredInfo(desiredPath);
    const QString parentPath = desiredInfo.absolutePath();
    const QString baseName = desiredInfo.fileName();
    const QString suffix = assignmentId.trimmed().isEmpty() ? QStringLiteral("id") : assignmentId.left(6);

    QString candidateName = sanitizeFileName(baseName + QStringLiteral(" [") + suffix + QStringLiteral("]"));
    QString candidatePath = QDir(parentPath).filePath(candidateName);

    if (!QFileInfo::exists(candidatePath) || metadataAssignmentId(candidatePath) == assignmentId) {
        return candidatePath;
    }

    int index = 2;
    while (index < 1000) {
        candidateName = sanitizeFileName(baseName + QStringLiteral(" [") + suffix + QStringLiteral("] (") + QString::number(index) + QStringLiteral(")"));
        candidatePath = QDir(parentPath).filePath(candidateName);
        if (!QFileInfo::exists(candidatePath) || metadataAssignmentId(candidatePath) == assignmentId) {
            return candidatePath;
        }
        ++index;
    }

    return candidatePath;
}

QString FolderOrganizer::createAssignmentFolder(
    const QString &semester,
    const QString &courseName,
    const Assignment &assignment) const
{
    const QString coursePath = createCourseFolder(semester, courseName);
    const QString assignmentFolderName = buildAssignmentFolderName(assignment);
    const QString desiredPath = QDir(coursePath).filePath(assignmentFolderName);
    const QString finalPath = resolveFolderConflict(desiredPath, assignment.id);
    ensureDir(finalPath);
    return finalPath;
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

bool FolderOrganizer::writeMetadataIfChanged(
    const QString &metadataPath,
    const QJsonObject &metadata,
    const QString &newHash,
    const QString &oldHash) const
{
    if (!oldHash.isEmpty() && oldHash == newHash && QFileInfo::exists(metadataPath)) {
        return false;
    }
    return writeMetadata(metadataPath, metadata);
}
