#pragma once

#include "Models.hpp"

#include <QDateTime>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>

// Una ruta absoluta registrada en sync_state.json que debe reescribirse porque la
// ruta base cambio. El indice guarda rutas absolutas y nadie las migraba, asi que
// al mover la base todo el respaldo pasaba a leerse como "no existe".
struct PathRebaseEntry {
    QString courseId;
    QString context;   // materia / tarea / adjunto al que pertenece la ruta
    QString field;     // folderPath | metadataPath | localPath
    QString oldPath;
    QString newPath;
    bool alreadyMigrated = false;
    bool existsAtNewPath = false;
};

struct PathRebasePlan {
    QString oldBase;
    QString newBase;
    QVector<PathRebaseEntry> entries;

    int toMigrate() const;
    int alreadyMigrated() const;
    int missingAfterMigration() const;
};

class SyncStateManager {
public:
    explicit SyncStateManager(const QString &statePath = QString());

    void setStatePath(const QString &statePath);
    QString statePath() const;

    bool load();
    bool save() const;

    bool hasCourse(const QString &courseId) const;
    bool hasAssignment(const QString &courseId, const QString &assignmentId) const;
    QStringList courseIds() const;
    QStringList assignmentIds(const QString &courseId) const;
    QJsonObject courseState(const QString &courseId) const;
    // Reinyecta un curso completo tal cual. Solo para preservar semestres archivados
    // durante una reconstruccion de indice: la API tipada recalcularia hashes y marcas.
    void setCourseStateRaw(const QString &courseId, const QJsonObject &courseState);

    QString courseFolderPath(const QString &courseId) const;
    QString assignmentFolderPath(const QString &courseId, const QString &assignmentId) const;
    QString assignmentMetadataPath(const QString &courseId, const QString &assignmentId) const;
    QString assignmentMetadataHash(const QString &courseId, const QString &assignmentId) const;
    QJsonObject assignmentState(const QString &courseId, const QString &assignmentId) const;
    bool isAssignmentArchivedDeleted(const QString &courseId, const QString &assignmentId) const;

    bool isAssignmentMetadataChanged(
        const QString &courseId,
        const QString &assignmentId,
        const QJsonObject &metadata) const;

    void updateCourse(const Course &course, const QString &semester, const QString &folderPath);
    void updateAssignment(
        const Course &course,
        const Assignment &assignment,
        const QString &folderName,
        const QString &folderPath,
        const QString &metadataPath,
        const QJsonObject &metadata);
    QJsonObject assignmentAttachments(const QString &courseId, const QString &assignmentId) const;
    QJsonObject assignmentAttachmentsState(const QString &courseId, const QString &assignmentId) const;
    QJsonObject attachmentRecord(const QString &courseId, const QString &assignmentId, const QString &attachmentKey) const;
    void updateAttachment(
        const QString &courseId,
        const QString &assignmentId,
        const QString &attachmentKey,
        const QJsonObject &attachmentData);
    void markAssignmentArchivedDeleted(
        const QString &courseId,
        const QString &assignmentId,
        const QString &reason);
    void clearAssignmentArchivedDeleted(const QString &courseId, const QString &assignmentId);
    void updateAssignmentChecksumState(
        const QString &courseId,
        const QString &assignmentId,
        const QJsonObject &checksumState);

    // Publications
    bool hasPublication(const QString &courseId, const QString &publicationId) const;
    QStringList publicationIds(const QString &courseId) const;
    QJsonObject publicationState(const QString &courseId, const QString &publicationId) const;
    QString publicationMetadataHash(const QString &courseId, const QString &publicationId) const;
    QString publicationFolderPath(const QString &courseId, const QString &publicationId) const;
    bool localPublicationFolderExists(const QString &courseId, const QString &publicationId) const;
    bool localPublicationMetadataExists(const QString &courseId, const QString &publicationId) const;
    bool isPublicationArchivedDeleted(const QString &courseId, const QString &publicationId) const;
    void updatePublication(
        const QString &courseId,
        const Publication &publication,
        const QString &folderPath,
        const QString &metadataPath,
        const QJsonObject &metadata);
    void markPublicationArchivedDeleted(
        const QString &courseId,
        const QString &publicationId,
        const QString &reason);
    void clearPublicationArchivedDeleted(const QString &courseId, const QString &publicationId);

    // Reescribe el prefijo oldBase -> newBase en folderPath, metadataPath y
    // localPath de materias, tareas, adjuntos y publicaciones. Con apply=false no
    // toca nada y solo devuelve el plan. Es idempotente: una ruta que ya cuelga de
    // newBase se marca alreadyMigrated y se deja igual.
    PathRebasePlan rebasePaths(const QString &oldBase, const QString &newBase, bool apply);
    // Deduce la base anterior a partir de las rutas registradas que no cuelgan de
    // currentBase. Devuelve vacio si no hay ninguna o si no coinciden entre si.
    QString detectPreviousBasePath(const QString &currentBase) const;

    QString lastSync() const;
    void setLastSync(const QDateTime &dateTime);

    bool localCourseFolderExists(const QString &courseId) const;
    bool localAssignmentFolderExists(const QString &courseId, const QString &assignmentId) const;
    bool localMetadataExists(const QString &courseId, const QString &assignmentId) const;

private:
    void ensureDefaultState();
    void migrateLegacyFlatAssignments();

    QJsonObject courseObject(const QString &courseId) const;
    QJsonObject assignmentObject(const QString &courseId, const QString &assignmentId) const;
    QJsonObject publicationObject(const QString &courseId, const QString &publicationId) const;

    QString m_statePath;
    QJsonObject m_root;
};
