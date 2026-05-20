#include "SyncManager.hpp"

#include "Utils.hpp"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>

namespace {

bool pathIsInsideBase(const QString &path, const QString &basePath)
{
    const QString cleanPath = QDir::cleanPath(path.trimmed());
    const QString cleanBase = QDir::cleanPath(basePath.trimmed());
    if (cleanPath.isEmpty() || cleanBase.isEmpty()) {
        return false;
    }

    const QString normalizedBase = cleanBase.endsWith(QLatin1Char('/'))
        ? cleanBase
        : cleanBase + QLatin1Char('/');
    return cleanPath == cleanBase || cleanPath.startsWith(normalizedBase);
}

} // namespace

SyncManager::SyncManager(QObject *parent)
    : QObject(parent)
    , m_syncStateManager()
    , m_classroomClient(this)
    , m_driveClient(this)
    , m_attachmentDownloader(this)
    , m_googleAuth(this)
{
    m_configManager.load();
    m_semesterByCourse = m_configManager.semesterMapping();

    m_folderOrganizer.setBasePath(m_configManager.basePath());

    m_syncStateManager.setStatePath(m_configManager.syncStatePath());
    m_syncStateManager.load();

    refreshAuthConfig();
    m_googleAuth.loadFromJson(m_configManager.loadToken());
    m_driveClient.setAccessToken(m_googleAuth.accessToken());

    m_attachmentDownloader.setDriveClient(&m_driveClient);
    m_attachmentDownloader.setSyncStateManager(&m_syncStateManager);
    m_attachmentChecksumManager.setSyncStateManager(&m_syncStateManager);

    connect(&m_classroomClient, &ClassroomClient::coursesFetched, this, &SyncManager::onCoursesFetched);
    connect(&m_classroomClient, &ClassroomClient::courseWorkSnapshotFetched, this, &SyncManager::onCourseWorkSnapshotFetched);
    connect(&m_classroomClient, &ClassroomClient::requestFailed, this, &SyncManager::onClientRequestFailed);
    connect(&m_classroomClient, &ClassroomClient::logMessage, this, [this](const QString &message) {
        emit logMessage(message);
    });
    connect(&m_attachmentDownloader, &AttachmentDownloader::attachmentProgress, this, &SyncManager::onAttachmentProgress);
    connect(&m_attachmentDownloader, &AttachmentDownloader::attachmentFinished, this, &SyncManager::onAttachmentFinished);
    connect(&m_attachmentDownloader, &AttachmentDownloader::attachmentLog, this, &SyncManager::onAttachmentLog);
    connect(&m_attachmentDownloader, &AttachmentDownloader::attachmentCountersChanged, this, &SyncManager::onAttachmentCountersChanged);
    connect(&m_attachmentChecksumManager, &AttachmentChecksumManager::checksumFailed, this, &SyncManager::onChecksumFailed);
    connect(&m_attachmentChecksumManager, &AttachmentChecksumManager::checksumLog, this, &SyncManager::onChecksumLog);

    connect(&m_googleAuth, &GoogleAuth::tokenUpdated, this, &SyncManager::onTokenUpdated);
    connect(&m_googleAuth, &GoogleAuth::authSucceeded, this, &SyncManager::onAuthSucceeded);
    connect(&m_googleAuth, &GoogleAuth::logMessage, this, [this](const QString &message) {
        logInfo(message);
    });
    connect(&m_googleAuth, &GoogleAuth::errorOccurred, this, [this](const QString &message) {
        m_waitingForTokenRefresh = false;
        ++m_errorCount;
        logErr(message);
        emitCounters();
    });
    connect(&m_googleAuth, &GoogleAuth::authFailed, this, [this](const QString &) {
        m_waitingForTokenRefresh = false;
    });

    emitCounters();
    emit attachmentCountersChanged(
        m_attachmentBlobDownloaded,
        m_attachmentWorkspaceExported,
        m_attachmentLinksSaved,
        m_attachmentSkipped,
        m_attachmentErrors);
}

ConfigManager &SyncManager::configManager()
{
    return m_configManager;
}

ClassroomClient *SyncManager::classroomClient()
{
    return &m_classroomClient;
}

GoogleAuth *SyncManager::googleAuth()
{
    return &m_googleAuth;
}

QList<Course> SyncManager::courses() const
{
    return m_courses;
}

QList<Assignment> SyncManager::allAssignments() const
{
    QList<Assignment> all;
    for (const Course &course : m_courses) {
        all.append(m_assignmentsByCourse.value(course.id));
    }
    return all;
}

QString SyncManager::basePath() const
{
    return m_configManager.basePath();
}

void SyncManager::setBasePath(const QString &basePath)
{
    const QString clean = QDir::cleanPath(basePath.trimmed());
    m_configManager.setBasePath(clean);
    m_folderOrganizer.setBasePath(clean);
    if (!m_configManager.save()) {
        ++m_errorCount;
        logErr(QStringLiteral("No se pudo guardar config.json"));
        emitCounters();
        return;
    }

    logInfo(QStringLiteral("Ruta base cambiada: %1").arg(clean));
    logInfo(QStringLiteral("Actualizando servicios con nueva ruta base..."));
}

QString SyncManager::semesterForCourse(const QString &courseId) const
{
    const QString semester = m_semesterByCourse.value(courseId).trimmed();
    if (!semester.isEmpty() && semester != QStringLiteral("Sin semestre")) {
        return semester;
    }

    const QString defaultSemesterValue = m_configManager.defaultSemester().trimmed();
    if (!defaultSemesterValue.isEmpty() && defaultSemesterValue != QStringLiteral("Todos los semestres")) {
        return defaultSemesterValue;
    }
    return QStringLiteral("Sin semestre");
}

void SyncManager::setSemesterForCourse(const QString &courseId, const QString &semester)
{
    const QString value = semester.trimmed();
    if (value.isEmpty()) {
        m_semesterByCourse.remove(courseId);
        m_configManager.setSemesterForCourse(courseId, QString());
    } else {
        m_semesterByCourse.insert(courseId, value);
        m_configManager.setSemesterForCourse(courseId, value);
    }

    if (!m_configManager.save()) {
        ++m_errorCount;
        logErr(QStringLiteral("No se pudo guardar config.json"));
        emitCounters();
    }
}

void SyncManager::setSemesterMapping(const QHash<QString, QString> &mapping)
{
    m_semesterByCourse = mapping;
    m_configManager.setSemesterMapping(mapping);
    if (!m_configManager.save()) {
        ++m_errorCount;
        logErr(QStringLiteral("No se pudo guardar config.json"));
        emitCounters();
    }
}

QString SyncManager::globalSemesterFilter() const
{
    return m_configManager.globalSemesterFilter();
}

void SyncManager::setGlobalSemesterFilter(const QString &semester)
{
    m_configManager.setGlobalSemesterFilter(semester);
    if (!m_configManager.save()) {
        ++m_errorCount;
        logErr(QStringLiteral("No se pudo guardar config.json"));
        emitCounters();
    }
}

QString SyncManager::defaultSemester() const
{
    return m_configManager.defaultSemester();
}

void SyncManager::setDefaultSemester(const QString &semester)
{
    m_configManager.setDefaultSemester(semester);
    if (!m_configManager.save()) {
        ++m_errorCount;
        logErr(QStringLiteral("No se pudo guardar config.json"));
        emitCounters();
    }
}

QString SyncManager::ensureSemesterFolderExists(const QString &semester)
{
    const QString clean = semester.trimmed();
    if (clean.isEmpty() || clean == QStringLiteral("Todos los semestres")) {
        return QString();
    }

    if (m_configManager.basePath().trimmed().isEmpty()) {
        return QString();
    }

    m_folderOrganizer.setBasePath(m_configManager.basePath());
    return m_folderOrganizer.createSemesterFolder(clean);
}

QString SyncManager::assignmentFolderPath(const QString &courseId, const QString &assignmentId) const
{
    return m_syncStateManager.assignmentFolderPath(courseId, assignmentId);
}

QString SyncManager::courseFolderPath(const QString &courseId) const
{
    return m_syncStateManager.courseFolderPath(courseId);
}

QString SyncManager::assignmentMetadataPath(const QString &courseId, const QString &assignmentId) const
{
    return m_syncStateManager.assignmentMetadataPath(courseId, assignmentId);
}

QJsonObject SyncManager::assignmentState(const QString &courseId, const QString &assignmentId) const
{
    return m_syncStateManager.assignmentState(courseId, assignmentId);
}

QJsonObject SyncManager::assignmentAttachmentsState(const QString &courseId, const QString &assignmentId) const
{
    return m_syncStateManager.assignmentAttachmentsState(courseId, assignmentId);
}

QStringList SyncManager::knownAssignmentIds(const QString &courseId) const
{
    return m_syncStateManager.assignmentIds(courseId);
}

bool SyncManager::isAssignmentArchivedDeleted(const QString &courseId, const QString &assignmentId) const
{
    return m_syncStateManager.isAssignmentArchivedDeleted(courseId, assignmentId);
}

bool SyncManager::localCourseFolderExists(const QString &courseId) const
{
    return m_syncStateManager.localCourseFolderExists(courseId);
}

bool SyncManager::localAssignmentFolderExists(const QString &courseId, const QString &assignmentId) const
{
    return m_syncStateManager.localAssignmentFolderExists(courseId, assignmentId);
}

bool SyncManager::localAssignmentMetadataExists(const QString &courseId, const QString &assignmentId) const
{
    return m_syncStateManager.localMetadataExists(courseId, assignmentId);
}

bool SyncManager::loadLocalStateIntoMemory(bool logOnFailure)
{
    if (!m_syncStateManager.load()) {
        if (logOnFailure) {
            logErr(QStringLiteral("No se pudo leer sync_state.json"));
        }
        return false;
    }

    QList<Course> localCourses;
    QHash<QString, QList<Assignment>> localAssignmentsByCourse;

    const QStringList courseIds = m_syncStateManager.courseIds();
    for (const QString &courseId : courseIds) {
        const QJsonObject courseState = m_syncStateManager.courseState(courseId);
        if (courseState.isEmpty()) {
            continue;
        }

        Course course;
        course.id = courseId;
        course.name = courseState.value(QStringLiteral("name")).toString().trimmed();
        if (course.name.isEmpty()) {
            course.name = courseId;
        }
        course.section = courseState.value(QStringLiteral("section")).toString().trimmed();
        course.descriptionHeading = courseState.value(QStringLiteral("descriptionHeading")).toString().trimmed();
        course.alternateLink = courseState.value(QStringLiteral("alternateLink")).toString().trimmed();
        localCourses.append(course);

        // El semestre manual por curso se mantiene en config.json.
        // No debemos "promover" el semestre guardado en sync_state a mapping manual,
        // porque eso impide que el selector global actue como default para materias sin asignacion manual.

        QList<Assignment> assignments;
        const QStringList assignmentIds = m_syncStateManager.assignmentIds(courseId);
        assignments.reserve(assignmentIds.size());

        for (const QString &assignmentId : assignmentIds) {
            const QJsonObject assignmentState = m_syncStateManager.assignmentState(courseId, assignmentId);
            if (assignmentState.isEmpty()) {
                continue;
            }

            Assignment assignment;
            assignment.id = assignmentId;
            assignment.courseId = courseId;
            assignment.title = assignmentState.value(QStringLiteral("title")).toString().trimmed();
            if (assignment.title.isEmpty()) {
                assignment.title = assignmentState.value(QStringLiteral("folderName")).toString().trimmed();
            }

            const QString metadataPath = m_syncStateManager.assignmentMetadataPath(courseId, assignmentId).trimmed();
            QFile metadataFile(metadataPath);
            if (metadataFile.exists() && metadataFile.open(QIODevice::ReadOnly)) {
                const QJsonDocument metadataDoc = QJsonDocument::fromJson(metadataFile.readAll());
                metadataFile.close();
                if (metadataDoc.isObject()) {
                    const QJsonObject metadata = metadataDoc.object();
                    assignment.description = metadata.value(QStringLiteral("description")).toString();
                    assignment.workType = metadata.value(QStringLiteral("workType")).toString();
                    assignment.state = metadata.value(QStringLiteral("state")).toString();
                    assignment.alternateLink = metadata.value(QStringLiteral("alternateLink")).toString();
                    assignment.maxPoints = metadata.contains(QStringLiteral("maxPoints"))
                        ? metadata.value(QStringLiteral("maxPoints")).toDouble()
                        : -1.0;
                    const QJsonObject submissionObj = metadata.value(QStringLiteral("submission")).toObject();
                    assignment.submissionId = submissionObj.value(QStringLiteral("id")).toString().trimmed();
                    assignment.submissionState = submissionObj.value(QStringLiteral("state")).toString().trimmed();
                    assignment.submissionLate = submissionObj.value(QStringLiteral("late")).toBool(false);
                    assignment.submissionUpdateTime = submissionObj.value(QStringLiteral("updateTime")).toString().trimmed();
                    assignment.submissionAlternateLink = submissionObj.value(QStringLiteral("alternateLink")).toString().trimmed();
                    assignment.submissionStateReliable =
                        submissionObj.value(QStringLiteral("reliable")).toBool(!assignment.submissionState.isEmpty());
                    assignment.submissionAssignedGrade = submissionObj.contains(QStringLiteral("assignedGrade"))
                        ? submissionObj.value(QStringLiteral("assignedGrade")).toDouble()
                        : -1.0;
                    assignment.submissionDraftGrade = submissionObj.contains(QStringLiteral("draftGrade"))
                        ? submissionObj.value(QStringLiteral("draftGrade")).toDouble()
                        : -1.0;

                    const QJsonObject dueDateObj = metadata.value(QStringLiteral("dueDate")).toObject();
                    if (!dueDateObj.isEmpty()) {
                        assignment.dueDate = QDate(
                            dueDateObj.value(QStringLiteral("year")).toInt(),
                            dueDateObj.value(QStringLiteral("month")).toInt(),
                            dueDateObj.value(QStringLiteral("day")).toInt());
                    }

                    const QJsonObject dueTimeObj = metadata.value(QStringLiteral("dueTime")).toObject();
                    if (!dueTimeObj.isEmpty()) {
                        assignment.dueTime = QTime(
                            dueTimeObj.value(QStringLiteral("hours")).toInt(),
                            dueTimeObj.value(QStringLiteral("minutes")).toInt(),
                            dueTimeObj.value(QStringLiteral("seconds")).toInt());
                    }

                    const QJsonArray materialsArray = metadata.value(QStringLiteral("materials")).toArray();
                    assignment.materials.reserve(materialsArray.size());
                    for (const QJsonValue &materialValue : materialsArray) {
                        if (!materialValue.isObject()) {
                            continue;
                        }

                        const QJsonObject materialObj = materialValue.toObject();
                        AssignmentMaterial material;
                        material.type = materialObj.value(QStringLiteral("type")).toString();
                        material.title = materialObj.value(QStringLiteral("title")).toString();
                        material.alternateLink = materialObj.value(QStringLiteral("alternateLink")).toString();
                        material.driveFileId = materialObj.value(QStringLiteral("driveFileId")).toString();
                        material.url = materialObj.value(QStringLiteral("url")).toString();
                        material.rawJson = materialObj;
                        assignment.materials.append(material);
                    }

                    assignment.rawJson = metadata;
                }
            }

            if (assignment.submissionState.trimmed().isEmpty()) {
                const QJsonObject submissionObj = assignmentState.value(QStringLiteral("submission")).toObject();
                assignment.submissionId = submissionObj.value(QStringLiteral("id")).toString().trimmed();
                assignment.submissionState = submissionObj.value(QStringLiteral("state")).toString().trimmed();
                assignment.submissionLate = submissionObj.value(QStringLiteral("late")).toBool(false);
                assignment.submissionUpdateTime = submissionObj.value(QStringLiteral("updateTime")).toString().trimmed();
                assignment.submissionAlternateLink = submissionObj.value(QStringLiteral("alternateLink")).toString().trimmed();
                assignment.submissionStateReliable =
                    submissionObj.value(QStringLiteral("reliable")).toBool(!assignment.submissionState.isEmpty());
                assignment.submissionAssignedGrade = submissionObj.contains(QStringLiteral("assignedGrade"))
                    ? submissionObj.value(QStringLiteral("assignedGrade")).toDouble()
                    : -1.0;
                assignment.submissionDraftGrade = submissionObj.contains(QStringLiteral("draftGrade"))
                    ? submissionObj.value(QStringLiteral("draftGrade")).toDouble()
                    : -1.0;
            }

            assignments.append(assignment);
        }

        localAssignmentsByCourse.insert(courseId, assignments);
    }

    m_courses = localCourses;
    m_assignmentsByCourse = localAssignmentsByCourse;
    return true;
}

bool SyncManager::restoreLocalStateSnapshot()
{
    return loadLocalStateIntoMemory(true);
}

void SyncManager::publishCurrentState()
{
    emit coursesChanged(m_courses);
    emit assignmentsChanged(allAssignments());
    emitCounters();
}

bool SyncManager::rebuildLocalIndex()
{
    refreshAuthConfig();

    if (!m_googleAuth.hasUsableAccessToken() && m_googleAuth.refreshToken().trimmed().isEmpty()) {
        logErr(QStringLiteral("No hay sesion valida para reconstruir indice local."));
        return false;
    }

    const QString syncPath = m_configManager.syncStatePath();
    const QFileInfo syncInfo(syncPath);
    if (syncInfo.exists()) {
        const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"));
        const QString backupPath = syncPath + QStringLiteral(".bak.") + stamp;

        if (!QFile::rename(syncPath, backupPath)) {
            if (!QFile::copy(syncPath, backupPath)) {
                logErr(QStringLiteral("No se pudo crear backup de sync_state.json"));
                return false;
            }
            QFile::remove(syncPath);
        }

        logInfo(QStringLiteral("Backup creado: %1").arg(backupPath));
    }

    m_courses.clear();
    m_assignmentsByCourse.clear();
    emit coursesChanged(m_courses);
    emit assignmentsChanged({});
    emitCounters();
    logInfo(QStringLiteral("Estado en memoria limpiado."));

    m_syncStateManager.setStatePath(syncPath);
    if (!m_syncStateManager.load()) {
        logErr(QStringLiteral("No se pudo inicializar estado limpio para reconstruccion."));
        return false;
    }
    m_syncStateManager.save();

    m_rebuildAfterFetch = true;
    logInfo(QStringLiteral("Cargando Classroom desde cero para reconstruccion..."));
    attemptAutoFetchFromClassroom();
    return true;
}

void SyncManager::refreshAuthConfig()
{
    m_googleAuth.setClientConfig(
        m_configManager.oauthClientId(),
        m_configManager.oauthClientSecret(),
        m_configManager.oauthRedirectUri(),
        m_configManager.oauthScopes());
    m_googleAuth.setTokenUri(m_configManager.oauthTokenUri());

    m_syncStateManager.setStatePath(m_configManager.syncStatePath());
}

void SyncManager::loadSampleData(const QString &samplePath)
{
    refreshAuthConfig();
    resetSyncOperationState();
    m_syncOperationMode = SyncOperationMode::FetchOnlyAll;

    m_classroomClient.setUseSampleData(true);
    if (!samplePath.trimmed().isEmpty()) {
        m_classroomClient.setSampleDataPath(samplePath);
    }

    logInfo(QStringLiteral("Cargando datos de prueba de Classroom..."));
    startFetchingCourses();
}

void SyncManager::fetchFromClassroom()
{
    refreshAuthConfig();
    resetSyncOperationState();
    m_syncOperationMode = SyncOperationMode::FetchOnlyAll;

    m_classroomClient.setUseSampleData(false);

    if (m_googleAuth.hasUsableAccessToken()) {
        m_classroomClient.setAccessToken(m_googleAuth.accessToken());
        logInfo(QStringLiteral("Consultando Classroom API..."));
        startFetchingCourses();
        return;
    }

    if (!m_googleAuth.refreshToken().isEmpty()) {
        logInfo(QStringLiteral("Access token vencido, intentando refresh token..."));
        m_waitingForTokenRefresh = true;
        m_googleAuth.refreshAccessToken();
        return;
    }

    ++m_errorCount;
    logErr(QStringLiteral("No hay token valido. Usa 'Iniciar sesion' primero."));
    emitCounters();
}

void SyncManager::attemptAutoFetchFromClassroom()
{
    refreshAuthConfig();
    resetSyncOperationState();
    m_syncOperationMode = SyncOperationMode::FetchOnlyAll;
    m_classroomClient.setUseSampleData(false);

    if (m_googleAuth.hasUsableAccessToken()) {
        m_classroomClient.setAccessToken(m_googleAuth.accessToken());
        logInfo(QStringLiteral("Token valido. Cargando Classroom..."));
        startFetchingCourses();
        return;
    }

    if (!m_googleAuth.refreshToken().trimmed().isEmpty()) {
        logInfo(QStringLiteral("Token expirado. Intentando refrescar sesion..."));
        m_waitingForTokenRefresh = true;
        m_googleAuth.refreshAccessToken();
        return;
    }

    logInfo(QStringLiteral("No hay sesion guardada. Inicia sesion para conectar Classroom."));
}

void SyncManager::verifyChecksumsInBackground()
{
    m_attachmentChecksumManager.verifyAllKnownAttachments();
}

void SyncManager::setAutoDownloadAttachments(bool enabled)
{
    m_autoDownloadAttachments = enabled;
}

bool SyncManager::autoDownloadAttachments() const
{
    return m_autoDownloadAttachments;
}

void SyncManager::syncAll()
{
    refreshAuthConfig();
    m_classroomClient.setUseSampleData(false);

    if (m_googleAuth.hasUsableAccessToken()) {
        m_classroomClient.setAccessToken(m_googleAuth.accessToken());
        startSyncAllInternal();
        return;
    }

    if (!m_googleAuth.refreshToken().trimmed().isEmpty()) {
        logInfo(QStringLiteral("Access token vencido, intentando refresh token..."));
        m_waitingForTokenRefresh = true;
        m_syncOperationMode = SyncOperationMode::SyncAll;
        m_syncScopeCourseId.clear();
        m_googleAuth.refreshAccessToken();
        return;
    }

    ++m_errorCount;
    logErr(QStringLiteral("No hay token valido. Usa 'Iniciar sesion' primero."));
    emitCounters();
}

void SyncManager::syncCourse(const QString &courseId)
{
    const QString cleanCourseId = courseId.trimmed();
    if (cleanCourseId.isEmpty()) {
        logErr(QStringLiteral("No se recibio un courseId valido para sincronizar materia."));
        return;
    }

    refreshAuthConfig();
    m_classroomClient.setUseSampleData(false);

    if (m_googleAuth.hasUsableAccessToken()) {
        m_classroomClient.setAccessToken(m_googleAuth.accessToken());
        startSyncCourseInternal(cleanCourseId);
        return;
    }

    if (!m_googleAuth.refreshToken().trimmed().isEmpty()) {
        logInfo(QStringLiteral("Access token vencido, intentando refresh token..."));
        m_waitingForTokenRefresh = true;
        m_syncOperationMode = SyncOperationMode::SyncCourse;
        m_syncScopeCourseId = cleanCourseId;
        m_googleAuth.refreshAccessToken();
        return;
    }

    ++m_errorCount;
    logErr(QStringLiteral("No hay token valido. Usa 'Iniciar sesion' primero."));
    emitCounters();
}

void SyncManager::resetSyncOperationState()
{
    m_scopedCourseIds.clear();
    m_pendingCourseWorkCourses.clear();
    m_incompleteCourseFetches.clear();
    m_stagedCourses.clear();
    m_stagedAssignmentsByCourse.clear();
    m_stagingSessionActive = false;
    m_syncScopeCourseId.clear();
    if (m_syncOperationMode != SyncOperationMode::FetchOnlyAll) {
        m_syncOperationMode = SyncOperationMode::Idle;
    }
}

void SyncManager::startSyncAllInternal()
{
    m_checksumRepairAttempts.clear();
    m_newCount = 0;
    m_updatedCount = 0;
    m_unchangedCount = 0;
    m_errorCount = 0;
    emitCounters();

    m_syncOperationMode = SyncOperationMode::SyncAll;
    m_syncScopeCourseId.clear();
    m_scopedCourseIds.clear();
    m_pendingCourseWorkCourses.clear();
    m_incompleteCourseFetches.clear();
    m_stagedCourses.clear();
    m_stagedAssignmentsByCourse.clear();

    if (!m_stagingManager.beginSession(QStringLiteral("global"))) {
        ++m_errorCount;
        logErr(QStringLiteral("No se pudo crear staging global de metadata."));
        emitCounters();
        resetSyncOperationState();
        return;
    }

    m_stagingSessionActive = true;
    emit syncProgress(0, 0);
    emit logMessage(QStringLiteral("SYNC  Sincronizando todo Classroom..."));
    emit logMessage(QStringLiteral("STAGE Creando staging global de metadata..."));
    m_classroomClient.fetchCourses();
}

void SyncManager::startSyncCourseInternal(const QString &courseId)
{
    m_checksumRepairAttempts.clear();
    m_newCount = 0;
    m_updatedCount = 0;
    m_unchangedCount = 0;
    m_errorCount = 0;
    emitCounters();

    m_syncOperationMode = SyncOperationMode::SyncCourse;
    m_syncScopeCourseId = courseId;
    m_scopedCourseIds = {courseId};
    m_pendingCourseWorkCourses = QSet<QString>{courseId};
    m_incompleteCourseFetches.clear();
    m_stagedCourses.clear();
    m_stagedAssignmentsByCourse.clear();

    if (!m_stagingManager.beginSession(QStringLiteral("course"))) {
        ++m_errorCount;
        logErr(QStringLiteral("No se pudo crear staging de metadata para la materia."));
        emitCounters();
        resetSyncOperationState();
        return;
    }
    m_stagingSessionActive = true;

    const Course course = resolveCourseForSync(courseId);
    m_stagedCourses.insert(courseId, course);
    m_stagingManager.writeCourse(course, QJsonObject());

    emit syncProgress(0, 1);
    emit logMessage(QStringLiteral("SYNC  Sincronizando materia: %1").arg(course.name.trimmed().isEmpty() ? courseId : course.name));
    emit logMessage(QStringLiteral("STAGE Creando staging de metadata..."));
    m_classroomClient.fetchCourseWork(courseId);
}

void SyncManager::startFetchingCourses()
{
    m_pendingCourseWorkRequests = 0;

    m_newCount = 0;
    m_updatedCount = 0;
    m_unchangedCount = 0;
    m_errorCount = 0;
    emitCounters();

    m_classroomClient.fetchCourses();
}

void SyncManager::onCoursesFetched(const QList<Course> &courses)
{
    m_courses = courses;

    bool migratedLegacy = false;
    for (const Course &course : m_courses) {
        if (m_semesterByCourse.contains(course.id)) {
            continue;
        }

        const QString legacySemester = m_configManager.legacySemesterForCourseName(course.name);
        if (!legacySemester.isEmpty()) {
            // "Sin semestre" heredado se considera valor por defecto, no asignacion manual.
            if (legacySemester != QStringLiteral("Sin semestre")) {
                m_semesterByCourse.insert(course.id, legacySemester);
                m_configManager.setSemesterForCourse(course.id, legacySemester);
            } else {
                m_semesterByCourse.remove(course.id);
                m_configManager.setSemesterForCourse(course.id, QString());
            }
            m_configManager.clearLegacySemesterForCourseName(course.name);
            migratedLegacy = true;
            logInfo(
                QStringLiteral("Migracion config: '%1' -> %2")
                    .arg(course.name, legacySemester));
        }
    }

    if (migratedLegacy) {
        m_configManager.save();
    }

    emit coursesChanged(m_courses);

    if (m_syncOperationMode == SyncOperationMode::SyncAll) {
        m_scopedCourseIds.clear();
        m_pendingCourseWorkCourses.clear();
        m_incompleteCourseFetches.clear();
        m_stagedCourses.clear();
        m_stagedAssignmentsByCourse.clear();

        for (const Course &course : m_courses) {
            m_scopedCourseIds.append(course.id);
            m_pendingCourseWorkCourses.insert(course.id);
            m_stagedCourses.insert(course.id, course);
            m_stagingManager.writeCourse(course, QJsonObject());
        }

        emit logMessage(QStringLiteral("INFO  Cursos remotos cargados: %1").arg(m_courses.size()));

        if (m_scopedCourseIds.isEmpty()) {
            maybeFinalizeStagedSync();
            return;
        }

        for (const Course &course : m_courses) {
            emit logMessage(QStringLiteral("API   Cargando tareas de %1...").arg(course.name.trimmed().isEmpty() ? course.id : course.name));
            m_classroomClient.fetchCourseWork(course.id);
        }
        return;
    }

    m_pendingCourseWorkRequests = m_courses.size();

    if (m_courses.isEmpty()) {
        logInfo(QStringLiteral("Cursos cargados: 0"));
        emit assignmentsChanged({});
        emitCounters();
        return;
    }

    logInfo(QStringLiteral("Cursos cargados: %1").arg(m_courses.size()));
    emitCounters();

    for (const Course &course : m_courses) {
        m_classroomClient.fetchCourseWork(course.id);
    }
}

void SyncManager::onCourseWorkFetched(const QString &courseId, const QList<Assignment> &courseWork)
{
    onCourseWorkSnapshotFetched(courseId, courseWork, true);
}

void SyncManager::onCourseWorkSnapshotFetched(const QString &courseId, const QList<Assignment> &courseWork, bool fetchComplete)
{
    if (m_syncOperationMode == SyncOperationMode::SyncAll || m_syncOperationMode == SyncOperationMode::SyncCourse) {
        m_stagedAssignmentsByCourse.insert(courseId, courseWork);

        QStringList assignmentIds;
        assignmentIds.reserve(courseWork.size());
        for (const Assignment &assignment : courseWork) {
            assignmentIds.append(assignment.id);

            Course course = m_stagedCourses.value(courseId, resolveCourseForSync(courseId));
            QJsonObject metadata = buildMetadata(course, assignment);
            m_stagingManager.writeAssignment(courseId, assignment.id, metadata);
        }

        const Course course = m_stagedCourses.value(courseId, resolveCourseForSync(courseId));
        if (!m_stagingManager.writeCourseManifest(
                courseId,
                course.name,
                assignmentIds,
                fetchComplete,
                fetchComplete ? QString() : QStringLiteral("Fetch incompleto"))) {
            ++m_errorCount;
            logErr(QStringLiteral("No se pudo escribir manifest de staging para curso %1").arg(courseId));
        }

        if (!fetchComplete) {
            m_incompleteCourseFetches.insert(courseId);
            emit logMessage(QStringLiteral("ERR   Fetch incompleto. No se detectaran eliminadas para esta materia: %1").arg(course.name));
        } else {
            emit logMessage(QStringLiteral("API   Fetch completo: %1 tareas (%2)").arg(courseWork.size()).arg(course.name));
        }

        if (m_pendingCourseWorkCourses.contains(courseId)) {
            m_pendingCourseWorkCourses.remove(courseId);
        }

        const int total = m_scopedCourseIds.size();
        const int done = total - m_pendingCourseWorkCourses.size();
        emit syncProgress(done, total);

        maybeFinalizeStagedSync();
        return;
    }

    if (!fetchComplete) {
        if (m_pendingCourseWorkRequests > 0) {
            --m_pendingCourseWorkRequests;
        }
        if (m_pendingCourseWorkRequests == 0) {
            finalizeFetchOnly();
        }
        return;
    }

    m_assignmentsByCourse.insert(courseId, courseWork);
    if (m_pendingCourseWorkRequests > 0) {
        --m_pendingCourseWorkRequests;
    }
    if (m_pendingCourseWorkRequests == 0) {
        finalizeFetchOnly();
    }
}

void SyncManager::onClientRequestFailed(const QString &context, int httpStatus, const QString &message)
{
    QString errorText = context + QStringLiteral(" -> ");
    if (httpStatus > 0) {
        errorText += QStringLiteral("HTTP %1 - ").arg(httpStatus);
    }
    errorText += message;

    ++m_errorCount;
    logErr(errorText);
    emitCounters();

    if (context == QStringLiteral("courses")
        && (m_syncOperationMode == SyncOperationMode::SyncAll || m_syncOperationMode == SyncOperationMode::SyncCourse)) {
        if (m_stagingSessionActive) {
            m_stagingManager.preserveForDebug();
            emit logMessage(QStringLiteral("WARN  Se conserva staging para diagnostico: %1").arg(m_stagingManager.sessionPath()));
            m_stagingManager.cleanup();
        }
        emit syncFinished(m_newCount, m_updatedCount, m_unchangedCount, m_errorCount);
        resetSyncOperationState();
        return;
    }

    if (context.startsWith(QStringLiteral("courseWork:"))
        && (m_syncOperationMode == SyncOperationMode::SyncAll || m_syncOperationMode == SyncOperationMode::SyncCourse)) {
        // Para sync con staging, la finalizacion del curso se decide en onCourseWorkSnapshotFetched
        // (ahi ya tenemos lista parcial + fetchComplete=false cuando aplica).
        return;
    }

}

void SyncManager::maybeFinalizeStagedSync()
{
    if (m_syncOperationMode != SyncOperationMode::SyncAll && m_syncOperationMode != SyncOperationMode::SyncCourse) {
        return;
    }
    if (!m_pendingCourseWorkCourses.isEmpty()) {
        return;
    }

    const bool allComplete = m_incompleteCourseFetches.isEmpty();
    if (!m_stagingManager.writeGlobalManifest(m_scopedCourseIds, allComplete, allComplete ? QString() : QStringLiteral("Fetch parcial"))) {
        ++m_errorCount;
        logErr(QStringLiteral("No se pudo escribir manifest global de staging."));
    }

    applyStagedDiffForScope();
}

void SyncManager::finalizeFetchOnly()
{
    const QList<Assignment> all = allAssignments();
    emit assignmentsChanged(all);
    logInfo(QStringLiteral("Tareas cargadas: %1").arg(all.size()));
    emitCounters();

    if (m_rebuildAfterFetch) {
        m_rebuildAfterFetch = false;
        logInfo(QStringLiteral("Reindexando estado con metadata fresca..."));
        syncAll();
        logInfo(QStringLiteral("Reconstruccion completada."));
    }
}

void SyncManager::applyStagedDiffForScope()
{
    if (!m_stagingSessionActive) {
        return;
    }

    if (!m_syncStateManager.load()) {
        ++m_errorCount;
        logErr(QStringLiteral("No se pudo leer sync_state.json para aplicar diff de staging."));
    }

    m_folderOrganizer.setBasePath(m_configManager.basePath());

    QVector<Assignment> attachmentQueue;
    int archivedCount = 0;

    for (const QString &courseId : std::as_const(m_scopedCourseIds)) {
        const Course course = m_stagedCourses.value(courseId, resolveCourseForSync(courseId));
        const bool allowArchive = !m_incompleteCourseFetches.contains(courseId) && m_stagingManager.courseFetchComplete(courseId);
        if (!allowArchive) {
            emit logMessage(QStringLiteral("WARN  Curso con fetch incompleto. Se omite deteccion de eliminadas: %1").arg(course.name));
        }

        const QVector<SyncAction> actions = m_diffEngine.diffCourse(
            courseId,
            m_stagingManager,
            m_syncStateManager,
            allowArchive);

        QString coursePath;
        for (const SyncAction &action : actions) {
            if (action.type == SyncActionType::DeletedArchivedAssignment) {
                ++archivedCount;
                m_syncStateManager.markAssignmentArchivedDeleted(
                    courseId,
                    action.assignmentId,
                    QStringLiteral("Assignment no longer returned by Classroom API during successful sync"));

                const QString assignmentFolder = m_syncStateManager.assignmentFolderPath(courseId, action.assignmentId).trimmed();
                if (!assignmentFolder.isEmpty() && QFileInfo::exists(assignmentFolder)) {
                    const QString archiveMarkerPath = QDir(assignmentFolder).filePath(QStringLiteral(".archived_deleted.json"));
                    QFile archiveMarker(archiveMarkerPath);
                    if (archiveMarker.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                        QJsonObject archiveObj;
                        archiveObj.insert(QStringLiteral("status"), QStringLiteral("deleted_archived"));
                        archiveObj.insert(QStringLiteral("detectedAt"), Utils::nowIsoStringUtc());
                        archiveObj.insert(QStringLiteral("courseId"), courseId);
                        archiveObj.insert(QStringLiteral("assignmentId"), action.assignmentId);
                        archiveObj.insert(
                            QStringLiteral("reason"),
                            QStringLiteral("Assignment no longer returned by Classroom API during successful sync"));
                        archiveMarker.write(QJsonDocument(archiveObj).toJson(QJsonDocument::Indented));
                        archiveMarker.close();
                    }
                }
                emit logMessage(
                    QStringLiteral("ARCH  Tarea ya no aparece en Classroom. Archivada localmente: %1")
                        .arg(action.title.trimmed().isEmpty() ? action.assignmentId : action.title));
                continue;
            }

            const Assignment assignment = findStagedAssignment(courseId, action.assignmentId);
            if (assignment.id.trimmed().isEmpty()) {
                ++m_errorCount;
                logErr(QStringLiteral("No se encontro assignment staged para aplicar diff: %1:%2").arg(courseId, action.assignmentId));
                continue;
            }

            QString assignmentPath;
            if (!ensureCourseAndAssignmentPaths(course, assignment, &coursePath, &assignmentPath)) {
                ++m_errorCount;
                logErr(QStringLiteral("No se pudo preparar carpeta de tarea: %1 / %2").arg(course.name, assignment.title));
                continue;
            }

            const QString metadataPath = QDir(assignmentPath).filePath(QStringLiteral("metadata.json"));
            QJsonObject metadataNoSyncTime = action.stagedMetadata;
            if (metadataNoSyncTime.isEmpty()) {
                metadataNoSyncTime = buildMetadata(course, assignment);
            }

            const QString newHash = Utils::sha256Json(metadataNoSyncTime);
            const QString oldHash = m_syncStateManager.assignmentMetadataHash(courseId, assignment.id);

            QJsonObject metadataToWrite = metadataNoSyncTime;
            metadataToWrite.insert(QStringLiteral("syncedAt"), Utils::nowIsoStringUtc());

            const bool metadataWritten = m_folderOrganizer.writeMetadataIfChanged(metadataPath, metadataToWrite, newHash, oldHash);
            const bool metadataExists = QFileInfo::exists(metadataPath);

            const QString mdPath = QDir(assignmentPath).filePath(QStringLiteral("descripcion.md"));
            if (metadataWritten || !QFileInfo::exists(mdPath)) {
                m_folderOrganizer.writeDescriptionMarkdown(assignmentPath, metadataNoSyncTime);
            }

            if (action.type == SyncActionType::NewAssignment) {
                if (metadataWritten || metadataExists) {
                    ++m_newCount;
                    logNew(QStringLiteral("%1 / %2").arg(course.name, Utils::effectiveAssignmentTitle(assignment)));
                }
            } else if (action.type == SyncActionType::UpdatedAssignment || action.type == SyncActionType::RestoredAssignment) {
                if (metadataWritten || metadataExists) {
                    ++m_updatedCount;
                    logUpdated(QStringLiteral("%1 / %2").arg(course.name, Utils::effectiveAssignmentTitle(assignment)));
                }
            } else if (action.type == SyncActionType::UnchangedAssignment) {
                ++m_unchangedCount;
                logSame(QStringLiteral("%1 / %2").arg(course.name, Utils::effectiveAssignmentTitle(assignment)));
            }

            m_syncStateManager.updateCourse(course, semesterForCourse(course.id), coursePath);
            m_syncStateManager.updateAssignment(
                course,
                assignment,
                QFileInfo(assignmentPath).fileName(),
                assignmentPath,
                metadataPath,
                metadataNoSyncTime);

            attachmentQueue.append(assignment);
        }
    }

    m_syncStateManager.setLastSync(QDateTime::currentDateTimeUtc());
    if (!m_syncStateManager.save()) {
        ++m_errorCount;
        logErr(QStringLiteral("No se pudo guardar sync_state.json"));
    }

    if (m_syncOperationMode == SyncOperationMode::SyncAll) {
        m_assignmentsByCourse = m_stagedAssignmentsByCourse;
    } else if (m_syncOperationMode == SyncOperationMode::SyncCourse && !m_syncScopeCourseId.isEmpty()) {
        m_assignmentsByCourse.insert(m_syncScopeCourseId, m_stagedAssignmentsByCourse.value(m_syncScopeCourseId));
        const Course scopedCourse = m_stagedCourses.value(m_syncScopeCourseId, resolveCourseForSync(m_syncScopeCourseId));
        bool found = false;
        for (Course &existing : m_courses) {
            if (existing.id == scopedCourse.id) {
                existing = scopedCourse;
                found = true;
                break;
            }
        }
        if (!found && !scopedCourse.id.trimmed().isEmpty()) {
            m_courses.append(scopedCourse);
        }
    }

    emit assignmentsChanged(allAssignments());
    emit coursesChanged(m_courses);
    emit syncStateChanged();
    emitCounters();

    emit logMessage(
        QStringLiteral("DIFF  Nuevas: %1, Actualizadas: %2, Sin cambios: %3, Archivadas: %4")
            .arg(m_newCount)
            .arg(m_updatedCount)
            .arg(m_unchangedCount)
            .arg(archivedCount));

    emit syncFinished(m_newCount, m_updatedCount, m_unchangedCount, m_errorCount);

    if (m_autoDownloadAttachments && !attachmentQueue.isEmpty()) {
        startAttachmentDownloadForAssignments(attachmentQueue);
    }

    if (m_errorCount > 0 || !m_incompleteCourseFetches.isEmpty()) {
        m_stagingManager.preserveForDebug();
        emit logMessage(QStringLiteral("WARN  Se conserva staging para diagnostico: %1").arg(m_stagingManager.sessionPath()));
    }

    m_stagingManager.cleanup();
    resetSyncOperationState();
}

Course SyncManager::resolveCourseForSync(const QString &courseId) const
{
    for (const Course &course : m_courses) {
        if (course.id == courseId) {
            return course;
        }
    }

    Course fallback;
    fallback.id = courseId;
    const QJsonObject state = m_syncStateManager.courseState(courseId);
    fallback.name = state.value(QStringLiteral("name")).toString().trimmed();
    fallback.section = state.value(QStringLiteral("section")).toString().trimmed();
    fallback.descriptionHeading = state.value(QStringLiteral("descriptionHeading")).toString().trimmed();
    fallback.alternateLink = state.value(QStringLiteral("alternateLink")).toString().trimmed();
    if (fallback.name.isEmpty()) {
        fallback.name = courseId;
    }
    return fallback;
}

Assignment SyncManager::findStagedAssignment(const QString &courseId, const QString &assignmentId) const
{
    const QList<Assignment> assignments = m_stagedAssignmentsByCourse.value(courseId);
    for (const Assignment &assignment : assignments) {
        if (assignment.id == assignmentId) {
            return assignment;
        }
    }
    return Assignment();
}

bool SyncManager::ensureCourseAndAssignmentPaths(
    const Course &course,
    const Assignment &assignment,
    QString *coursePath,
    QString *assignmentPath) const
{
    if (!coursePath || !assignmentPath) {
        return false;
    }

    const QString semester = semesterForCourse(course.id);
    const QString configuredBasePath = m_configManager.basePath().trimmed();
    const QString suggestedCoursePath = m_folderOrganizer.createCourseFolder(semester, course.name);
    const QString previousCoursePath = m_syncStateManager.courseFolderPath(course.id).trimmed();

    *coursePath = suggestedCoursePath;
    if (!previousCoursePath.isEmpty()
        && QFileInfo::exists(previousCoursePath)
        && QDir::cleanPath(previousCoursePath) != QDir::cleanPath(suggestedCoursePath)
        && pathIsInsideBase(previousCoursePath, configuredBasePath)
        && !m_syncStateManager.assignmentIds(course.id).isEmpty()) {
        *coursePath = previousCoursePath;
    }

    QString localAssignmentPath = m_syncStateManager.assignmentFolderPath(course.id, assignment.id).trimmed();
    if (!localAssignmentPath.isEmpty() && !pathIsInsideBase(localAssignmentPath, configuredBasePath)) {
        localAssignmentPath.clear();
    }
    if (localAssignmentPath.isEmpty() || !QFileInfo::exists(localAssignmentPath)) {
        localAssignmentPath = m_folderOrganizer.createAssignmentFolder(semester, course.name, assignment);
    }

    if (localAssignmentPath.isEmpty() || !QFileInfo::exists(localAssignmentPath)) {
        return false;
    }

    *assignmentPath = localAssignmentPath;
    return true;
}

void SyncManager::startAttachmentDownloadForAssignments(const QVector<Assignment> &assignments)
{
    const QString driveScope = QStringLiteral("https://www.googleapis.com/auth/drive.readonly");
    const bool hasDriveScope = m_googleAuth.hasScope(driveScope);
    if (!hasDriveScope) {
        emit logMessage(QStringLiteral("ERR   Se requiere permiso de lectura de Drive. Borra token.json o cierra sesion y vuelve a iniciar sesion para autorizar Drive."));
    }

    m_driveClient.setAccessToken(m_googleAuth.accessToken());
    m_attachmentDownloader.setDriveDownloadsEnabled(hasDriveScope && !m_googleAuth.accessToken().trimmed().isEmpty());

    m_attachmentBlobDownloaded = 0;
    m_attachmentWorkspaceExported = 0;
    m_attachmentLinksSaved = 0;
    m_attachmentSkipped = 0;
    m_attachmentErrors = 0;
    emit attachmentCountersChanged(
        m_attachmentBlobDownloaded,
        m_attachmentWorkspaceExported,
        m_attachmentLinksSaved,
        m_attachmentSkipped,
        m_attachmentErrors);

    emit logMessage(QStringLiteral("FILE  Procesando adjuntos..."));
    m_attachmentDownloader.downloadAttachmentsForAssignments(assignments);
}

QString SyncManager::syncActionLabel(SyncActionType type) const
{
    switch (type) {
    case SyncActionType::NewAssignment:
        return QStringLiteral("new");
    case SyncActionType::UpdatedAssignment:
        return QStringLiteral("updated");
    case SyncActionType::UnchangedAssignment:
        return QStringLiteral("same");
    case SyncActionType::DeletedArchivedAssignment:
        return QStringLiteral("archived");
    case SyncActionType::RestoredAssignment:
        return QStringLiteral("restored");
    }
    return QStringLiteral("unknown");
}

void SyncManager::onAttachmentProgress(int current, int total)
{
    emit attachmentProgress(current, total);
}

void SyncManager::onAttachmentFinished(int downloaded, int skipped, int errors)
{
    emit attachmentFinished(downloaded, skipped, errors);
    verifyChecksumsInBackground();
}

void SyncManager::onAttachmentLog(const QString &message)
{
    emit logMessage(message);
}

void SyncManager::onAttachmentCountersChanged(int blobDownloaded, int exported, int linksSaved, int skipped, int errors)
{
    m_attachmentBlobDownloaded = blobDownloaded;
    m_attachmentWorkspaceExported = exported;
    m_attachmentLinksSaved = linksSaved;
    m_attachmentSkipped = skipped;
    m_attachmentErrors = errors;

    emit attachmentCountersChanged(blobDownloaded, exported, linksSaved, skipped, errors);
}

void SyncManager::onTokenUpdated()
{
    if (!m_configManager.saveToken(m_googleAuth.toJson())) {
        ++m_errorCount;
        logErr(QStringLiteral("No se pudo guardar token.json"));
        emitCounters();
    }
    m_classroomClient.setAccessToken(m_googleAuth.accessToken());
    m_driveClient.setAccessToken(m_googleAuth.accessToken());
}

void SyncManager::onAuthSucceeded()
{
    m_driveClient.setAccessToken(m_googleAuth.accessToken());
    if (m_waitingForTokenRefresh) {
        m_waitingForTokenRefresh = false;
        m_classroomClient.setAccessToken(m_googleAuth.accessToken());
        if (m_syncOperationMode == SyncOperationMode::SyncCourse && !m_syncScopeCourseId.trimmed().isEmpty()) {
            logInfo(QStringLiteral("Token actualizado. Sincronizando materia..."));
            startSyncCourseInternal(m_syncScopeCourseId);
        } else if (m_syncOperationMode == SyncOperationMode::SyncAll) {
            logInfo(QStringLiteral("Token actualizado. Sincronizando Classroom completo..."));
            startSyncAllInternal();
        } else {
            logInfo(QStringLiteral("Token actualizado. Consultando Classroom API..."));
            startFetchingCourses();
        }
    }
}

void SyncManager::onChecksumFailed(const QString &courseId, const QString &assignmentId, const QStringList &attachmentKeys)
{
    if (!m_syncStateManager.load()) {
        logErr(QStringLiteral("No se pudo leer sync_state.json para reparar checksum fallido."));
        return;
    }

    QStringList retryKeys;
    retryKeys.reserve(attachmentKeys.size());
    for (const QString &attachmentKey : attachmentKeys) {
        const QString cleanKey = attachmentKey.trimmed();
        if (cleanKey.isEmpty()) {
            continue;
        }

        const QString retryId = courseId + QStringLiteral(":") + assignmentId + QStringLiteral(":") + cleanKey;
        const int attempts = m_checksumRepairAttempts.value(retryId, 0);
        if (attempts >= 1) {
            emit logMessage(
                QStringLiteral("ERR   Reintento de checksum agotado para adjunto: %1")
                    .arg(cleanKey));
            continue;
        }

        m_checksumRepairAttempts.insert(retryId, attempts + 1);
        retryKeys.append(cleanKey);
    }

    if (retryKeys.isEmpty()) {
        return;
    }

    Assignment targetAssignment;
    bool foundInMemory = false;

    const QList<Assignment> currentAssignments = m_assignmentsByCourse.value(courseId);
    for (const Assignment &assignment : currentAssignments) {
        if (assignment.id == assignmentId) {
            targetAssignment = assignment;
            foundInMemory = true;
            break;
        }
    }

    if (!foundInMemory) {
        const QJsonObject assignmentState = m_syncStateManager.assignmentState(courseId, assignmentId);
        targetAssignment.id = assignmentId;
        targetAssignment.courseId = courseId;
        targetAssignment.title = assignmentState.value(QStringLiteral("title")).toString().trimmed();

        const QJsonObject attachmentsState = m_syncStateManager.assignmentAttachmentsState(courseId, assignmentId);
        for (const QString &attachmentKey : retryKeys) {
            const QJsonObject record = attachmentsState.value(attachmentKey).toObject();
            if (record.isEmpty()) {
                continue;
            }

            AssignmentMaterial material;
            material.type = record.value(QStringLiteral("type")).toString().trimmed();
            material.title = record.value(QStringLiteral("title")).toString().trimmed();
            material.driveFileId = record.value(QStringLiteral("driveFileId")).toString().trimmed();
            material.alternateLink = record.value(QStringLiteral("webViewLink")).toString().trimmed();
            material.url = record.value(QStringLiteral("url")).toString().trimmed();
            material.rawJson = record;

            if (material.type.isEmpty()) {
                material.type = material.driveFileId.isEmpty() ? QStringLiteral("link") : QStringLiteral("driveFile");
            }
            if (material.type == QStringLiteral("driveFile") && material.driveFileId.isEmpty()) {
                material.driveFileId = attachmentKey;
            }
            if ((material.type == QStringLiteral("link")
                    || material.type == QStringLiteral("youtubeVideo")
                    || material.type == QStringLiteral("form"))
                && material.url.isEmpty()) {
                material.url = material.alternateLink;
            }

            targetAssignment.materials.append(material);
        }
    }

    if (targetAssignment.materials.isEmpty()) {
        emit logMessage(
            QStringLiteral("ERR   No se pudo asociar checksum fallido con adjuntos descargables: %1 / %2")
                .arg(courseId, assignmentId));
        return;
    }

    const QString driveScope = QStringLiteral("https://www.googleapis.com/auth/drive.readonly");
    const bool hasDriveScope = m_googleAuth.hasScope(driveScope);
    m_driveClient.setAccessToken(m_googleAuth.accessToken());
    m_attachmentDownloader.setDriveDownloadsEnabled(hasDriveScope && !m_googleAuth.accessToken().trimmed().isEmpty());

    emit logMessage(
        QStringLiteral("FILE  Re-descargando adjuntos fallidos por checksum: %1 / %2")
            .arg(courseId, assignmentId));
    m_attachmentDownloader.redownloadForAssignmentKeys(targetAssignment, retryKeys);
}

void SyncManager::onChecksumLog(const QString &message)
{
    emit logMessage(message);
}

QJsonObject SyncManager::buildMetadata(const Course &course, const Assignment &assignment) const
{
    QJsonObject metadata;
    metadata.insert(QStringLiteral("courseId"), course.id);
    metadata.insert(QStringLiteral("courseName"), course.name);
    metadata.insert(QStringLiteral("assignmentId"), assignment.id);
    metadata.insert(QStringLiteral("title"), Utils::effectiveAssignmentTitle(assignment));
    metadata.insert(QStringLiteral("description"), assignment.description);
    metadata.insert(QStringLiteral("workType"), assignment.workType);
    metadata.insert(QStringLiteral("state"), assignment.state);
    metadata.insert(QStringLiteral("alternateLink"), assignment.alternateLink);
    metadata.insert(QStringLiteral("dueDate"), Utils::dueDateObject(assignment.dueDate));
    metadata.insert(QStringLiteral("dueTime"), Utils::dueTimeObject(assignment.dueTime));
    metadata.insert(QStringLiteral("materials"), Utils::materialsToJsonArray(assignment.materials));
    if (assignment.maxPoints >= 0.0)
        metadata.insert(QStringLiteral("maxPoints"), assignment.maxPoints);

    QJsonObject submission;
    submission.insert(QStringLiteral("id"), assignment.submissionId);
    submission.insert(QStringLiteral("courseWorkId"), assignment.id);
    submission.insert(QStringLiteral("state"), assignment.submissionState);
    submission.insert(QStringLiteral("late"), assignment.submissionLate);
    submission.insert(QStringLiteral("updateTime"), assignment.submissionUpdateTime);
    submission.insert(QStringLiteral("alternateLink"), assignment.submissionAlternateLink);
    submission.insert(QStringLiteral("reliable"), assignment.submissionStateReliable);
    if (assignment.submissionAssignedGrade >= 0.0)
        submission.insert(QStringLiteral("assignedGrade"), assignment.submissionAssignedGrade);
    if (assignment.submissionDraftGrade >= 0.0)
        submission.insert(QStringLiteral("draftGrade"), assignment.submissionDraftGrade);
    metadata.insert(QStringLiteral("submission"), submission);

    return metadata;
}

void SyncManager::syncFolders()
{
    m_checksumRepairAttempts.clear();

    m_newCount = 0;
    m_updatedCount = 0;
    m_unchangedCount = 0;
    m_errorCount = 0;
    emitCounters();

    const QString configuredBasePath = m_configManager.basePath().trimmed();
    if (configuredBasePath.isEmpty()) {
        ++m_errorCount;
        logErr(QStringLiteral("Ruta base vacia. Selecciona una carpeta primero."));
        emitCounters();
        emit syncFinished(m_newCount, m_updatedCount, m_unchangedCount, m_errorCount);
        return;
    }

    const QFileInfo baseInfo(configuredBasePath);
    if (!baseInfo.exists() || !baseInfo.isDir()) {
        ++m_errorCount;
        logErr(QStringLiteral("Ruta base inexistente: %1").arg(configuredBasePath));
        emitCounters();
        emit syncFinished(m_newCount, m_updatedCount, m_unchangedCount, m_errorCount);
        return;
    }

    if (!baseInfo.isWritable()) {
        ++m_errorCount;
        logErr(QStringLiteral("Permisos insuficientes sobre ruta base: %1").arg(configuredBasePath));
        emitCounters();
        emit syncFinished(m_newCount, m_updatedCount, m_unchangedCount, m_errorCount);
        return;
    }

    if (m_courses.isEmpty()) {
        ++m_errorCount;
        logErr(QStringLiteral("No hay cursos cargados para sincronizar."));
        emitCounters();
        emit syncFinished(m_newCount, m_updatedCount, m_unchangedCount, m_errorCount);
        return;
    }

    m_folderOrganizer.setBasePath(configuredBasePath);
    emit logMessage(QStringLiteral("SYNC  Usando ruta base: %1").arg(configuredBasePath));

    if (!m_syncStateManager.load()) {
        ++m_errorCount;
        logErr(QStringLiteral("No se pudo leer sync_state.json"));
    }

    const QList<Assignment> all = allAssignments();
    int current = 0;
    const int total = all.size();

    for (const Course &course : m_courses) {
        const QString semester = semesterForCourse(course.id);
        const QString suggestedCoursePath = m_folderOrganizer.createCourseFolder(semester, course.name);
        const QString previousCoursePath = m_syncStateManager.courseFolderPath(course.id).trimmed();

        QString coursePath = suggestedCoursePath;
        if (!previousCoursePath.isEmpty()
            && QFileInfo::exists(previousCoursePath)
            && QDir::cleanPath(previousCoursePath) != QDir::cleanPath(suggestedCoursePath)
            && pathIsInsideBase(previousCoursePath, configuredBasePath)
            && !m_syncStateManager.assignmentIds(course.id).isEmpty()) {
            coursePath = previousCoursePath;
            logInfo(
                QStringLiteral("La materia '%1' ya tiene carpeta previa en %2. No se movera automaticamente.")
                    .arg(course.name, previousCoursePath));
        }

        m_syncStateManager.updateCourse(course, semester, coursePath);

        const QList<Assignment> assignments = m_assignmentsByCourse.value(course.id);
        for (const Assignment &assignment : assignments) {
            ++current;
            emit syncProgress(current, total);

            const bool knownAssignment = m_syncStateManager.hasAssignment(course.id, assignment.id);
            QString assignmentPath = m_syncStateManager.assignmentFolderPath(course.id, assignment.id);
            if (!assignmentPath.trimmed().isEmpty() && !pathIsInsideBase(assignmentPath, configuredBasePath)) {
                assignmentPath.clear();
            }

            if (assignmentPath.trimmed().isEmpty() || !QFileInfo::exists(assignmentPath)) {
                if (knownAssignment && !assignmentPath.trimmed().isEmpty()) {
                    emit logMessage(
                        QStringLiteral("MISS  Carpeta faltante local: %1 / %2")
                            .arg(course.name, Utils::effectiveAssignmentTitle(assignment)));
                }

                const QString desiredPath = QDir(coursePath).filePath(FolderOrganizer::buildAssignmentFolderName(assignment));
                const QString resolvedPath = m_folderOrganizer.resolveFolderConflict(desiredPath, assignment.id);
                const bool existedBefore = QFileInfo::exists(resolvedPath);

                assignmentPath = m_folderOrganizer.createAssignmentFolder(semester, course.name, assignment);
                if (!QFileInfo::exists(assignmentPath)) {
                    ++m_errorCount;
                    logErr(QStringLiteral("No se pudo crear carpeta: %1").arg(assignmentPath));
                    emitCounters();
                    continue;
                }

                if (!existedBefore) {
                    logInfo(QStringLiteral("Carpeta creada: %1").arg(assignmentPath));
                }
            }

            const QString metadataPath = QDir(assignmentPath).filePath(QStringLiteral("metadata.json"));
            if (knownAssignment && !QFileInfo::exists(metadataPath)) {
                emit logMessage(
                    QStringLiteral("MISS  metadata faltante: %1 / %2")
                        .arg(course.name, Utils::effectiveAssignmentTitle(assignment)));
            }

            const QJsonObject metadataNoSyncTime = buildMetadata(course, assignment);
            const QString newHash = Utils::sha256Json(metadataNoSyncTime);
            const QString oldHash = m_syncStateManager.assignmentMetadataHash(course.id, assignment.id);

            QJsonObject metadataToWrite = metadataNoSyncTime;
            metadataToWrite.insert(QStringLiteral("syncedAt"), Utils::nowIsoStringUtc());

            const bool metadataWritten = m_folderOrganizer.writeMetadataIfChanged(metadataPath, metadataToWrite, newHash, oldHash);
            const bool metadataFileExists = QFileInfo::exists(metadataPath);

            const QString mdPath = QDir(assignmentPath).filePath(QStringLiteral("descripcion.md"));
            if (metadataWritten || !QFileInfo::exists(mdPath)) {
                m_folderOrganizer.writeDescriptionMarkdown(assignmentPath, metadataNoSyncTime);
            }

            if (!metadataWritten && oldHash == newHash && metadataFileExists) {
                ++m_unchangedCount;
                logSame(
                    QStringLiteral("Sin cambios: %1 / %2")
                        .arg(course.name, Utils::effectiveAssignmentTitle(assignment)));
            } else if (metadataWritten) {
                if (knownAssignment) {
                    ++m_updatedCount;
                    logUpdated(
                        QStringLiteral("Metadata actualizado: %1 / %2")
                            .arg(course.name, Utils::effectiveAssignmentTitle(assignment)));
                } else {
                    ++m_newCount;
                    logNew(
                        QStringLiteral("Tarea nueva: %1 / %2")
                            .arg(course.name, Utils::effectiveAssignmentTitle(assignment)));
                }
            } else {
                ++m_errorCount;
                logErr(QStringLiteral("No se pudo escribir metadata: %1").arg(metadataPath));
                emitCounters();
                continue;
            }

            m_syncStateManager.updateAssignment(
                course,
                assignment,
                QFileInfo(assignmentPath).fileName(),
                assignmentPath,
                metadataPath,
                metadataNoSyncTime);
        }
    }

    m_syncStateManager.setLastSync(QDateTime::currentDateTimeUtc());
    if (!m_syncStateManager.save()) {
        ++m_errorCount;
        logErr(QStringLiteral("No se pudo guardar sync_state.json"));
    }

    emitCounters();
    emit syncFinished(m_newCount, m_updatedCount, m_unchangedCount, m_errorCount);

    logInfo(
        QStringLiteral("Sincronizacion terminada. Nuevas: %1, actualizadas: %2, sin cambios: %3, errores: %4")
            .arg(m_newCount)
            .arg(m_updatedCount)
            .arg(m_unchangedCount)
            .arg(m_errorCount));

    if (m_autoDownloadAttachments) {
        downloadAttachments();
    }
}

void SyncManager::downloadAttachments()
{
    m_checksumRepairAttempts.clear();

    if (m_courses.isEmpty()) {
        logErr(QStringLiteral("No hay cursos/tareas cargadas para descargar adjuntos."));
        return;
    }

    if (!m_syncStateManager.load()) {
        logErr(QStringLiteral("No se pudo leer sync_state.json antes de descargar adjuntos."));
        return;
    }

    const QString driveScope = QStringLiteral("https://www.googleapis.com/auth/drive.readonly");
    const bool hasDriveScope = m_googleAuth.hasScope(driveScope);
    if (!hasDriveScope) {
        emit logMessage(QStringLiteral("ERR   Se requiere permiso de lectura de Drive. Borra token.json o cierra sesion y vuelve a iniciar sesion para autorizar Drive."));
    }

    m_driveClient.setAccessToken(m_googleAuth.accessToken());
    m_attachmentDownloader.setDriveDownloadsEnabled(hasDriveScope && !m_googleAuth.accessToken().trimmed().isEmpty());

    m_attachmentBlobDownloaded = 0;
    m_attachmentWorkspaceExported = 0;
    m_attachmentLinksSaved = 0;
    m_attachmentSkipped = 0;
    m_attachmentErrors = 0;
    emit attachmentCountersChanged(
        m_attachmentBlobDownloaded,
        m_attachmentWorkspaceExported,
        m_attachmentLinksSaved,
        m_attachmentSkipped,
        m_attachmentErrors);

    QVector<Assignment> assignments;
    const QList<Assignment> all = allAssignments();
    assignments.reserve(all.size());
    for (const Assignment &assignment : all) {
        assignments.append(assignment);
    }

    m_attachmentDownloader.downloadAttachmentsForAssignments(assignments);
}

void SyncManager::logInfo(const QString &message)
{
    emit logMessage(QStringLiteral("INFO  %1").arg(message));
}

void SyncManager::logNew(const QString &message)
{
    emit logMessage(QStringLiteral("NEW   %1").arg(message));
}

void SyncManager::logSame(const QString &message)
{
    emit logMessage(QStringLiteral("SAME  %1").arg(message));
}

void SyncManager::logUpdated(const QString &message)
{
    emit logMessage(QStringLiteral("UPD   %1").arg(message));
}

void SyncManager::logErr(const QString &message)
{
    emit errorOccurred(QStringLiteral("ERR   %1").arg(message));
}

void SyncManager::emitCounters()
{
    emit countersChanged(
        m_courses.size(),
        allAssignments().size(),
        m_newCount,
        m_updatedCount,
        m_unchangedCount,
        m_errorCount);
}
