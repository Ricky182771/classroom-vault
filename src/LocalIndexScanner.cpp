#include "LocalIndexScanner.hpp"

#include "CourseFolderMarker.hpp"
#include "Utils.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>

namespace {

constexpr const char *kPublicationsDir = "_Publicaciones";
constexpr const char *kAttachmentsDir = "Adjuntos";

QJsonObject readJsonObject(const QString &path)
{
    QFile file(path);
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
        return {};
    }
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    return doc.isObject() ? doc.object() : QJsonObject{};
}

QString isoUtc(const QDateTime &dateTime)
{
    return dateTime.toUTC().toString(Qt::ISODate);
}

// El hash que guarda el indice se calcula sobre la metadata SIN la marca de
// sincronizacion, que es justo lo unico que se le añade al escribirla. Quitandola
// se reproduce exactamente, y el siguiente sync no reescribe ficheros iguales.
QString metadataHashOf(const QJsonObject &metadata)
{
    QJsonObject withoutSyncTime = metadata;
    withoutSyncTime.remove(QStringLiteral("syncedAt"));
    return Utils::sha256Json(withoutSyncTime);
}

QJsonObject scanAttachments(const QString &assignmentFolder, const QJsonObject &metadata, int *count)
{
    const QString attachmentsPath = QDir(assignmentFolder).filePath(QLatin1String(kAttachmentsDir));
    QDir attachmentsDir(attachmentsPath);
    if (!attachmentsDir.exists()) {
        return {};
    }

    // Los materiales dan el driveFileId real; el titulo coincide con el nombre del
    // fichero escrito en disco, asi que sirve para reencontrar la clave original y
    // que el deduplicador no vuelva a descargarlo todo.
    QHash<QString, QString> driveIdByTitle;
    const QJsonArray materials = metadata.value(QStringLiteral("materials")).toArray();
    for (const QJsonValue &value : materials) {
        const QJsonObject material = value.toObject();
        const QString driveFileId = material.value(QStringLiteral("driveFileId")).toString().trimmed();
        const QString title = material.value(QStringLiteral("title")).toString().trimmed();
        if (!driveFileId.isEmpty() && !title.isEmpty()) {
            driveIdByTitle.insert(title, driveFileId);
        }
    }

    QJsonObject attachments;
    const QFileInfoList files = attachmentsDir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
    for (const QFileInfo &file : files) {
        if (file.fileName() == QLatin1String(".checksum")) {
            continue;
        }

        const QString driveFileId = driveIdByTitle.value(file.fileName());

        QJsonObject record;
        record.insert(QStringLiteral("localFileName"), file.fileName());
        record.insert(QStringLiteral("localPath"), file.absoluteFilePath());
        record.insert(QStringLiteral("name"), file.fileName());
        record.insert(QStringLiteral("size"), static_cast<double>(file.size()));
        record.insert(QStringLiteral("lastDownloaded"), isoUtc(file.lastModified()));
        if (!driveFileId.isEmpty()) {
            record.insert(QStringLiteral("driveFileId"), driveFileId);
        }
        // md5Checksum y modifiedTime remotos no se pueden deducir del disco: se
        // omiten a proposito para que el deduplicador caiga en la comparacion por
        // tamaño en vez de creerse un hash inventado.

        attachments.insert(driveFileId.isEmpty() ? file.fileName() : driveFileId, record);
        ++(*count);
    }

    return attachments;
}

QJsonObject scanAssignment(const QString &folder, const QString &metadataPath, int *attachmentCount)
{
    const QJsonObject metadata = readJsonObject(metadataPath);
    if (metadata.isEmpty()) {
        return {};
    }

    const QFileInfo metadataInfo(metadataPath);
    const QString syncedAt = metadata.value(QStringLiteral("syncedAt")).toString().trimmed();

    QJsonObject state;
    state.insert(QStringLiteral("title"), metadata.value(QStringLiteral("title")).toString());
    state.insert(QStringLiteral("workType"), metadata.value(QStringLiteral("workType")).toString());
    state.insert(QStringLiteral("state"), metadata.value(QStringLiteral("state")).toString());
    state.insert(QStringLiteral("alternateLink"), metadata.value(QStringLiteral("alternateLink")).toString());
    state.insert(QStringLiteral("folderName"), QFileInfo(folder).fileName());
    state.insert(QStringLiteral("folderPath"), folder);
    state.insert(QStringLiteral("metadataPath"), metadataPath);
    state.insert(QStringLiteral("metadataHash"), metadataHashOf(metadata));
    state.insert(QStringLiteral("lastSeen"), isoUtc(metadataInfo.lastModified()));
    state.insert(QStringLiteral("lastUpdated"), syncedAt.isEmpty() ? isoUtc(metadataInfo.lastModified()) : syncedAt);
    if (metadata.contains(QStringLiteral("submission"))) {
        state.insert(QStringLiteral("submission"), metadata.value(QStringLiteral("submission")));
    }

    const QJsonObject attachments = scanAttachments(folder, metadata, attachmentCount);
    if (!attachments.isEmpty()) {
        state.insert(QStringLiteral("attachments"), attachments);
    }

    // Evidencia protegida: el marcador en disco manda, es justo para lo que existe.
    const QString archivedMarker = QDir(folder).filePath(QStringLiteral(".archived_deleted.json"));
    if (QFileInfo::exists(archivedMarker)) {
        state.insert(QStringLiteral("isArchivedDeleted"), true);
        const QJsonObject archivalStatus = readJsonObject(archivedMarker);
        if (!archivalStatus.isEmpty()) {
            state.insert(QStringLiteral("archivalStatus"), archivalStatus);
        }
    }

    return state;
}

QJsonObject scanPublication(const QString &folder, const QString &metadataPath)
{
    const QJsonObject metadata = readJsonObject(metadataPath);
    if (metadata.isEmpty()) {
        return {};
    }

    const QFileInfo metadataInfo(metadataPath);
    const QString syncedAt = metadata.value(QStringLiteral("syncedAt")).toString().trimmed();

    QJsonObject state;
    state.insert(QStringLiteral("title"), metadata.value(QStringLiteral("title")).toString());
    state.insert(QStringLiteral("kind"), metadata.value(QStringLiteral("kind")).toString());
    state.insert(QStringLiteral("state"), metadata.value(QStringLiteral("state")).toString());
    state.insert(QStringLiteral("alternateLink"), metadata.value(QStringLiteral("alternateLink")).toString());
    state.insert(QStringLiteral("folderPath"), folder);
    state.insert(QStringLiteral("metadataPath"), metadataPath);
    state.insert(QStringLiteral("metadataHash"), metadataHashOf(metadata));
    state.insert(QStringLiteral("lastSeen"), isoUtc(metadataInfo.lastModified()));
    state.insert(QStringLiteral("lastUpdated"), syncedAt.isEmpty() ? isoUtc(metadataInfo.lastModified()) : syncedAt);
    return state;
}

} // namespace

int LocalScanResult::assignments() const
{
    int total = 0;
    for (const ScannedCourse &course : courses) {
        total += course.assignments;
    }
    return total;
}

int LocalScanResult::publications() const
{
    int total = 0;
    for (const ScannedCourse &course : courses) {
        total += course.publications;
    }
    return total;
}

int LocalScanResult::attachments() const
{
    int total = 0;
    for (const ScannedCourse &course : courses) {
        total += course.attachments;
    }
    return total;
}

namespace LocalIndexScanner {

LocalScanResult scan(
    const QString &basePath,
    const QStringList &archivedSemesters,
    bool createMissingMarkers)
{
    LocalScanResult result;

    const QString tasksRoot = QDir(basePath.trimmed()).filePath(QStringLiteral("Tareas"));
    QDir root(tasksRoot);
    if (basePath.trimmed().isEmpty() || !root.exists()) {
        result.warnings.append(QStringLiteral("No existe la carpeta de respaldos: %1").arg(tasksRoot));
        return result;
    }

    const QStringList semesters = root.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QString &semester : semesters) {
        ++result.semesterFolders;
        QDir semesterDir(root.filePath(semester));

        const QStringList courseNames = semesterDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        for (const QString &courseFolderName : courseNames) {
            ++result.courseFolders;

            ScannedCourse course;
            course.semester = semester;
            course.folderPath = semesterDir.filePath(courseFolderName);
            course.name = courseFolderName;

            const QJsonObject marker = CourseFolderMarker::read(course.folderPath);
            course.uid = marker.value(QStringLiteral("uid")).toString().trimmed();
            course.courseId = marker.value(QStringLiteral("courseId")).toString().trimmed();
            const QString markerName = marker.value(QStringLiteral("courseName")).toString().trimmed();
            if (!markerName.isEmpty()) {
                course.name = markerName;
            }

            QJsonObject assignments;
            QJsonObject publications;

            const QStringList children =
                QDir(course.folderPath).entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
            for (const QString &child : children) {
                const QString childPath = QDir(course.folderPath).filePath(child);

                if (child == QLatin1String(kPublicationsDir)) {
                    const QStringList pubs = QDir(childPath).entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
                    for (const QString &pub : pubs) {
                        const QString pubPath = QDir(childPath).filePath(pub);
                        const QString metadataPath = QDir(pubPath).filePath(QStringLiteral("metadata.json"));
                        const QJsonObject metadata = readJsonObject(metadataPath);
                        const QString publicationId = metadata.value(QStringLiteral("resourceId")).toString().trimmed();
                        if (publicationId.isEmpty()) {
                            result.warnings.append(
                                QStringLiteral("Publicacion sin resourceId, se omite: %1").arg(pubPath));
                            continue;
                        }
                        const QJsonObject state = scanPublication(pubPath, metadataPath);
                        if (state.isEmpty()) {
                            continue;
                        }
                        publications.insert(publicationId, state);
                        ++course.publications;
                        if (course.courseId.isEmpty()) {
                            course.courseId = metadata.value(QStringLiteral("courseId")).toString().trimmed();
                        }
                    }
                    continue;
                }

                const QString metadataPath = QDir(childPath).filePath(QStringLiteral("metadata.json"));
                const QJsonObject metadata = readJsonObject(metadataPath);
                if (metadata.isEmpty()) {
                    result.warnings.append(QStringLiteral("Carpeta sin metadata.json, se omite: %1").arg(childPath));
                    continue;
                }

                const QString assignmentId = metadata.value(QStringLiteral("assignmentId")).toString().trimmed();
                if (assignmentId.isEmpty()) {
                    result.warnings.append(
                        QStringLiteral("Tarea sin assignmentId, se omite: %1").arg(childPath));
                    continue;
                }

                const QJsonObject state = scanAssignment(childPath, metadataPath, &course.attachments);
                if (state.isEmpty()) {
                    continue;
                }
                assignments.insert(assignmentId, state);
                ++course.assignments;

                if (course.courseId.isEmpty()) {
                    course.courseId = metadata.value(QStringLiteral("courseId")).toString().trimmed();
                }
                const QString metadataCourseName = metadata.value(QStringLiteral("courseName")).toString().trimmed();
                if (markerName.isEmpty() && !metadataCourseName.isEmpty()) {
                    course.name = metadataCourseName;
                }
            }

            if (createMissingMarkers) {
                const QString ensured =
                    CourseFolderMarker::ensure(course.folderPath, course.courseId, course.name, semester);
                if (course.uid.isEmpty() && !ensured.isEmpty()) {
                    course.markerCreated = true;
                }
                course.uid = ensured;
            }

            QJsonObject state;
            state.insert(QStringLiteral("name"), course.name);
            state.insert(QStringLiteral("semester"), semester);
            state.insert(QStringLiteral("folderPath"), course.folderPath);
            state.insert(QStringLiteral("lastSeen"), isoUtc(QFileInfo(course.folderPath).lastModified()));
            state.insert(QStringLiteral("assignments"), assignments);
            state.insert(QStringLiteral("publications"), publications);
            if (!course.uid.isEmpty()) {
                state.insert(QStringLiteral("localUid"), course.uid);
            }
            course.state = state;

            result.courses.append(course);
        }
    }

    // Clave en sync_state. Cuando el mismo courseId tiene respaldo en dos
    // semestres (el congelado del ciclo pasado y el nuevo), la materia activa se
    // queda con el courseId y la archivada pasa a su uid: son dos respaldos
    // distintos y tienen que poder coexistir.
    QSet<QString> usedKeys;
    const QSet<QString> archived(archivedSemesters.constBegin(), archivedSemesters.constEnd());

    for (int pass = 0; pass < 2; ++pass) {
        const bool wantArchived = pass == 1;
        for (ScannedCourse &course : result.courses) {
            if (archived.contains(course.semester) != wantArchived) {
                continue;
            }
            QString key = course.courseId;
            if (key.isEmpty() || usedKeys.contains(key)) {
                key = course.uid.isEmpty()
                    ? QStringLiteral("local:%1/%2").arg(course.semester, QFileInfo(course.folderPath).fileName())
                    : QStringLiteral("local:%1").arg(course.uid);
            }
            usedKeys.insert(key);
            course.stateKey = key;
        }
    }

    return result;
}

} // namespace LocalIndexScanner
