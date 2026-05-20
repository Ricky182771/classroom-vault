#include "FolderOrganizer.hpp"

#include "Utils.hpp"

#include <QDate>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QTextStream>

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

bool FolderOrganizer::writeDescriptionMarkdown(const QString &assignmentFolder, const QJsonObject &metadata) const
{
    const QString mdPath = QDir(assignmentFolder).filePath(QStringLiteral("descripcion.md"));

    const QString title = metadata.value(QStringLiteral("title")).toString().trimmed();
    const QString courseName = metadata.value(QStringLiteral("courseName")).toString().trimmed();
    const QString workType = metadata.value(QStringLiteral("workType")).toString().trimmed();
    const QString description = metadata.value(QStringLiteral("description")).toString().trimmed();
    const QString alternateLink = metadata.value(QStringLiteral("alternateLink")).toString().trimmed();
    const double maxPoints = metadata.contains(QStringLiteral("maxPoints"))
        ? metadata.value(QStringLiteral("maxPoints")).toDouble()
        : -1.0;

    const QJsonObject submissionObj = metadata.value(QStringLiteral("submission")).toObject();
    const double assignedGrade = submissionObj.contains(QStringLiteral("assignedGrade"))
        ? submissionObj.value(QStringLiteral("assignedGrade")).toDouble()
        : -1.0;

    const QJsonObject dueDateObj = metadata.value(QStringLiteral("dueDate")).toObject();
    const int year = dueDateObj.value(QStringLiteral("year")).toInt();
    const int month = dueDateObj.value(QStringLiteral("month")).toInt();
    const int day = dueDateObj.value(QStringLiteral("day")).toInt();
    const QString dueDateStr = (year > 0 && month > 0 && day > 0)
        ? QDate(year, month, day).toString(QStringLiteral("yyyy-MM-dd"))
        : QStringLiteral("Sin fecha");

    auto formatNum = [](double v) -> QString {
        return (v == static_cast<double>(static_cast<long long>(v)))
            ? QString::number(static_cast<long long>(v))
            : QString::number(v, 'f', 2);
    };

    QString md;
    md += QStringLiteral("# ") + (title.isEmpty() ? QStringLiteral("Tarea sin titulo") : title) + QStringLiteral("\n\n");
    if (!courseName.isEmpty()) {
        md += QStringLiteral("**Materia:** ") + courseName + QStringLiteral("  \n");
    }
    md += QStringLiteral("**Entrega:** ") + dueDateStr + QStringLiteral("  \n");
    if (!workType.isEmpty()) {
        md += QStringLiteral("**Tipo:** ") + workType + QStringLiteral("  \n");
    }
    if (maxPoints >= 0.0) {
        md += QStringLiteral("**Ponderacion:** ") + formatNum(maxPoints) + QStringLiteral(" pts  \n");
    }
    if (assignedGrade >= 0.0) {
        if (maxPoints >= 0.0) {
            md += QStringLiteral("**Calificacion:** ") + formatNum(assignedGrade) + QStringLiteral(" / ") + formatNum(maxPoints) + QStringLiteral(" pts  \n");
        } else {
            md += QStringLiteral("**Calificacion:** ") + formatNum(assignedGrade) + QStringLiteral("  \n");
        }
    }
    if (!alternateLink.isEmpty()) {
        md += QStringLiteral("**Classroom:** [Abrir tarea](") + alternateLink + QStringLiteral(")  \n");
    }
    md += QStringLiteral("\n---\n\n");

    if (description.isEmpty()) {
        md += QStringLiteral("*Esta tarea no tiene descripcion.*\n");
    } else {
        md += description + QStringLiteral("\n");
    }

    QFile file(mdPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        return false;
    }
    QTextStream out(&file);
    out << md;
    file.close();
    return true;
}
