#pragma once

#include "AttachmentDownloader.hpp"
#include "AttachmentChecksumManager.hpp"
#include "ClassroomClient.hpp"
#include "ConfigManager.hpp"
#include "DriveClient.hpp"
#include "FolderOrganizer.hpp"
#include "GoogleAuth.hpp"
#include "Models.hpp"
#include "SyncDiffEngine.hpp"
#include "SyncStagingManager.hpp"
#include "SyncStateManager.hpp"

#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QSet>
#include <QStringList>

class SyncManager : public QObject {
    Q_OBJECT

public:
    explicit SyncManager(QObject *parent = nullptr);

    ConfigManager &configManager();
    ClassroomClient *classroomClient();
    GoogleAuth *googleAuth();

    QList<Course> courses() const;
    QList<Assignment> allAssignments() const;
    QList<Publication> publicationsForCourse(const QString &courseId) const;

    QString basePath() const;
    void setBasePath(const QString &basePath);
    // Una ruta registrada puede haber quedado fuera de la base activa (por
    // ejemplo un disco externo que ya no se usa). Que no exista ahi no es un
    // fallo de respaldo, asi que la UI necesita poder distinguirlo.
    bool isPathInsideBasePath(const QString &path) const;

    QString semesterForCourse(const QString &courseId) const;
    void setSemesterForCourse(const QString &courseId, const QString &semester);
    void setSemesterMapping(const QHash<QString, QString> &mapping);
    QString globalSemesterFilter() const;
    void setGlobalSemesterFilter(const QString &semester);
    QString defaultSemester() const;
    void setDefaultSemester(const QString &semester);
    bool isSemesterArchived(const QString &semester) const;
    bool isCourseArchived(const QString &courseId) const;
    QStringList archivedSemesters() const;
    bool archiveSemester(const QString &semester);
    bool unarchiveSemester(const QString &semester);

    // Materias que Classroom SIGUE devolviendo pero cuyo mapeo apunta a un
    // semestre archivado. Son materias vivas atrapadas por metadatos obsoletos:
    // el guard de archivado las excluye de cada sync, para siempre y en silencio.
    // Pasa cuando la institucion reutiliza el mismo curso de Classroom y solo lo
    // renombra al empezar un ciclo nuevo.
    QList<Course> coursesTrappedInArchivedSemester() const;
    // Las devuelve al semestre activo indicado. Deja intacto el respaldo que ya
    // tienen bajo el arbol archivado: a partir de aqui escriben en el nuevo.
    int releaseCoursesFromArchivedSemester(const QString &targetSemester);
    QString ensureSemesterFolderExists(const QString &semester);

    QString assignmentFolderPath(const QString &courseId, const QString &assignmentId) const;
    QString courseFolderPath(const QString &courseId) const;
    QString assignmentMetadataPath(const QString &courseId, const QString &assignmentId) const;
    QJsonObject assignmentState(const QString &courseId, const QString &assignmentId) const;
    QJsonObject assignmentAttachmentsState(const QString &courseId, const QString &assignmentId) const;
    QStringList knownAssignmentIds(const QString &courseId) const;
    bool isAssignmentArchivedDeleted(const QString &courseId, const QString &assignmentId) const;
    bool localCourseFolderExists(const QString &courseId) const;
    bool localAssignmentFolderExists(const QString &courseId, const QString &assignmentId) const;
    bool localAssignmentMetadataExists(const QString &courseId, const QString &assignmentId) const;

    QJsonObject publicationState(const QString &courseId, const QString &publicationId) const;
    QStringList knownPublicationIds(const QString &courseId) const;
    bool isPublicationArchivedDeleted(const QString &courseId, const QString &publicationId) const;
    bool localPublicationFolderExists(const QString &courseId, const QString &publicationId) const;

    bool restoreLocalStateSnapshot();
    void publishCurrentState();
    bool rebuildLocalIndex();

    void loadSampleData(const QString &samplePath = QString());
    void fetchFromClassroom();
    void attemptAutoFetchFromClassroom();
    void verifyChecksumsInBackground();
    void setAutoDownloadAttachments(bool enabled);
    bool autoDownloadAttachments() const;

    void syncAll();
    void syncCourse(const QString &courseId);
    void syncFolders();
    void downloadAttachments();

signals:
    void coursesChanged(const QList<Course> &courses);
    void assignmentsChanged(const QList<Assignment> &assignments);
    void publicationsChanged(const QString &courseId, const QList<Publication> &publications);
    void syncProgress(int current, int total);
    void syncFinished(int newCount, int updatedCount, int unchangedCount, int errorCount);
    void countersChanged(
        int courses,
        int assignments,
        int newCount,
        int updatedCount,
        int unchangedCount,
        int errorCount);
    void attachmentProgress(int current, int total);
    void attachmentFinished(int downloaded, int skipped, int errors);
    void attachmentCountersChanged(int blobDownloaded, int exported, int linksSaved, int skipped, int errors);
    void logMessage(const QString &message);
    void errorOccurred(const QString &message);
    void syncStateChanged();
    void semesterArchivedChanged(const QString &semester);

private slots:
    void onCoursesFetched(const QList<Course> &courses);
    void onCourseWorkFetched(const QString &courseId, const QList<Assignment> &courseWork);
    void onCourseWorkSnapshotFetched(const QString &courseId, const QList<Assignment> &courseWork, bool fetchComplete);
    void onPublicationsSnapshotFetched(const QString &courseId, const QList<Publication> &publications, bool fetchComplete);
    void onClientRequestFailed(const QString &context, int httpStatus, const QString &message);
    void onAttachmentProgress(int current, int total);
    void onAttachmentFinished(int downloaded, int skipped, int errors);
    void onAttachmentLog(const QString &message);
    void onAttachmentCountersChanged(int blobDownloaded, int exported, int linksSaved, int skipped, int errors);

    void onTokenUpdated();
    void onAuthSucceeded();
    void onChecksumFailed(const QString &courseId, const QString &assignmentId, const QStringList &attachmentKeys);
    void onChecksumLog(const QString &message);

private:
    enum class SyncOperationMode {
        Idle,
        FetchOnlyAll,
        SyncAll,
        SyncCourse
    };

    void refreshAuthConfig();
    // Deshace una reconstruccion que ya vacio el indice pero no pudo repoblarlo.
    void abortRebuildAndRestore(const QString &reason);
    void startFetchingCourses();
    bool loadLocalStateIntoMemory(bool logOnFailure = true);
    bool buildCourseFromLocalState(const QString &courseId, Course *course) const;
    QList<Assignment> loadLocalAssignmentsForCourse(const QString &courseId) const;
    // Semestres archivados: respaldo local en solo lectura, desconectado de Classroom.
    void mergeArchivedLocalCourses();
    QSet<QString> archivedCourseIds() const;
    QJsonObject buildMetadata(const Course &course, const Assignment &assignment) const;
    void resetSyncOperationState();
    void startSyncAllInternal();
    void startSyncCourseInternal(const QString &courseId);
    QJsonObject buildPublicationMetadata(const Course &course, const Publication &publication) const;
    void maybeFinalizeStagedSync();
    void finalizeFetchOnly();
    void applyStagedDiffForScope();
    Course resolveCourseForSync(const QString &courseId) const;
    Assignment findStagedAssignment(const QString &courseId, const QString &assignmentId) const;
    QStringList archivedSemesterRoots() const;
    bool pathIsUnderArchivedSemester(const QString &path) const;
    // Resolucion unica de la carpeta de una materia: tareas y publicaciones deben
    // salir de aqui, o el mismo curso acaba repartido entre dos semestres.
    QString resolveCoursePath(const Course &course) const;
    bool ensureCourseAndAssignmentPaths(
        const Course &course,
        const Assignment &assignment,
        QString *coursePath,
        QString *assignmentPath) const;
    void startAttachmentDownloadForAssignments(const QVector<Assignment> &assignments);
    QString syncActionLabel(SyncActionType type) const;

    void logInfo(const QString &message);
    void logNew(const QString &message);
    void logSame(const QString &message);
    void logUpdated(const QString &message);
    void logErr(const QString &message);
    void logSec(const QString &message);
    void logArch(const QString &message);

    bool checkBasePath();

    void emitCounters();

    ConfigManager m_configManager;
    SyncStateManager m_syncStateManager;
    FolderOrganizer m_folderOrganizer;
    ClassroomClient m_classroomClient;
    DriveClient m_driveClient;
    AttachmentDownloader m_attachmentDownloader;
    AttachmentChecksumManager m_attachmentChecksumManager;
    SyncStagingManager m_stagingManager;
    SyncDiffEngine m_diffEngine;
    GoogleAuth m_googleAuth;

    QList<Course> m_courses;
    QHash<QString, QList<Assignment>> m_assignmentsByCourse;
    QHash<QString, QList<Publication>> m_publicationsByCourse;
    QHash<QString, QString> m_semesterByCourse;
    // Ids que vinieron de la ultima respuesta de Classroom, antes de reinyectar
    // las materias archivadas que solo existen en local. Sin esta distincion no
    // se puede saber que materias siguen vivas.
    QSet<QString> m_remoteCourseIds;

    int m_pendingCourseWorkRequests = 0;
    int m_pendingPublicationFetchRequests = 0;
    bool m_waitingForTokenRefresh = false;

    int m_newCount = 0;
    int m_updatedCount = 0;
    int m_unchangedCount = 0;
    int m_errorCount = 0;
    int m_attachmentBlobDownloaded = 0;
    int m_attachmentWorkspaceExported = 0;
    int m_attachmentLinksSaved = 0;
    int m_attachmentSkipped = 0;
    int m_attachmentErrors = 0;
    bool m_autoDownloadAttachments = false;
    bool m_rebuildAfterFetch = false;
    // Backup tomado justo antes de vaciar el indice para reconstruirlo. Si la carga
    // de Classroom falla despues del vaciado hay que volver atras: si no, el indice
    // queda truncado y el usuario ni se entera de que existe el backup.
    QString m_rebuildBackupPath;
    QHash<QString, int> m_checksumRepairAttempts;

    SyncOperationMode m_syncOperationMode = SyncOperationMode::Idle;
    QString m_syncScopeCourseId;
    QStringList m_scopedCourseIds;
    QSet<QString> m_pendingCourseWorkCourses;
    QSet<QString> m_incompleteCourseFetches;
    QHash<QString, Course> m_stagedCourses;
    QHash<QString, QList<Assignment>> m_stagedAssignmentsByCourse;
    QHash<QString, QList<Publication>> m_stagedPublicationsByCourse;
    QSet<QString> m_pendingPublicationCourses;
    QSet<QString> m_incompletePublicationFetches;
    bool m_stagingSessionActive = false;
};
