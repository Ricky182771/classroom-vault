#include "SyncManager.hpp"

#include "Utils.hpp"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>

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

    connect(&m_classroomClient, &ClassroomClient::coursesFetched, this, &SyncManager::onCoursesFetched);
    connect(&m_classroomClient, &ClassroomClient::courseWorkFetched, this, &SyncManager::onCourseWorkFetched);
    connect(&m_classroomClient, &ClassroomClient::requestFailed, this, &SyncManager::onClientRequestFailed);
    connect(&m_attachmentDownloader, &AttachmentDownloader::attachmentProgress, this, &SyncManager::onAttachmentProgress);
    connect(&m_attachmentDownloader, &AttachmentDownloader::attachmentFinished, this, &SyncManager::onAttachmentFinished);
    connect(&m_attachmentDownloader, &AttachmentDownloader::attachmentLog, this, &SyncManager::onAttachmentLog);
    connect(&m_attachmentDownloader, &AttachmentDownloader::attachmentCountersChanged, this, &SyncManager::onAttachmentCountersChanged);

    connect(&m_googleAuth, &GoogleAuth::tokenUpdated, this, &SyncManager::onTokenUpdated);
    connect(&m_googleAuth, &GoogleAuth::authSucceeded, this, &SyncManager::onAuthSucceeded);
    connect(&m_googleAuth, &GoogleAuth::logMessage, this, [this](const QString &message) {
        logInfo(message);
    });
    connect(&m_googleAuth, &GoogleAuth::errorOccurred, this, [this](const QString &message) {
        ++m_errorCount;
        logErr(message);
        emitCounters();
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
    m_configManager.setBasePath(basePath);
    m_folderOrganizer.setBasePath(basePath);
    if (!m_configManager.save()) {
        ++m_errorCount;
        logErr(QStringLiteral("No se pudo guardar config.json"));
        emitCounters();
    }
}

QString SyncManager::semesterForCourse(const QString &courseId) const
{
    const QString semester = m_semesterByCourse.value(courseId).trimmed();
    return semester.isEmpty() ? QStringLiteral("Sin semestre") : semester;
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

QString SyncManager::assignmentFolderPath(const QString &courseId, const QString &assignmentId) const
{
    return m_syncStateManager.assignmentFolderPath(courseId, assignmentId);
}

void SyncManager::refreshAuthConfig()
{
    m_googleAuth.setClientConfig(
        m_configManager.oauthClientId(),
        m_configManager.oauthClientSecret(),
        m_configManager.oauthRedirectUri(),
        m_configManager.oauthScopes());

    m_syncStateManager.setStatePath(m_configManager.syncStatePath());
}

void SyncManager::loadSampleData(const QString &samplePath)
{
    refreshAuthConfig();

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

void SyncManager::setAutoDownloadAttachments(bool enabled)
{
    m_autoDownloadAttachments = enabled;
}

bool SyncManager::autoDownloadAttachments() const
{
    return m_autoDownloadAttachments;
}

void SyncManager::startFetchingCourses()
{
    m_courses.clear();
    m_assignmentsByCourse.clear();
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
            m_semesterByCourse.insert(course.id, legacySemester);
            m_configManager.setSemesterForCourse(course.id, legacySemester);
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
    m_assignmentsByCourse.insert(courseId, courseWork);

    if (m_pendingCourseWorkRequests > 0) {
        --m_pendingCourseWorkRequests;
    }

    if (m_pendingCourseWorkRequests == 0) {
        const QList<Assignment> all = allAssignments();
        emit assignmentsChanged(all);
        logInfo(QStringLiteral("Tareas cargadas: %1").arg(all.size()));
        emitCounters();
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

    if (context.startsWith(QStringLiteral("courseWork:")) && m_pendingCourseWorkRequests > 0) {
        --m_pendingCourseWorkRequests;
        if (m_pendingCourseWorkRequests == 0) {
            emit assignmentsChanged(allAssignments());
            emitCounters();
        }
    }
}

void SyncManager::onAttachmentProgress(int current, int total)
{
    emit attachmentProgress(current, total);
}

void SyncManager::onAttachmentFinished(int downloaded, int skipped, int errors)
{
    emit attachmentFinished(downloaded, skipped, errors);
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
    m_driveClient.setAccessToken(m_googleAuth.accessToken());
}

void SyncManager::onAuthSucceeded()
{
    m_driveClient.setAccessToken(m_googleAuth.accessToken());
    if (m_waitingForTokenRefresh) {
        m_waitingForTokenRefresh = false;
        m_classroomClient.setAccessToken(m_googleAuth.accessToken());
        logInfo(QStringLiteral("Token actualizado. Consultando Classroom API..."));
        startFetchingCourses();
    }
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
    return metadata;
}

void SyncManager::syncFolders()
{
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

    if (!m_syncStateManager.load()) {
        ++m_errorCount;
        logErr(QStringLiteral("No se pudo leer sync_state.json"));
    }

    const QList<Assignment> all = allAssignments();
    int current = 0;
    const int total = all.size();

    for (const Course &course : m_courses) {
        const QString semester = semesterForCourse(course.id);
        const QString coursePath = m_folderOrganizer.createCourseFolder(semester, course.name);
        m_syncStateManager.updateCourse(course, semester, coursePath);

        const QList<Assignment> assignments = m_assignmentsByCourse.value(course.id);
        for (const Assignment &assignment : assignments) {
            ++current;
            emit syncProgress(current, total);

            const bool knownAssignment = m_syncStateManager.hasAssignment(course.id, assignment.id);
            QString assignmentPath = m_syncStateManager.assignmentFolderPath(course.id, assignment.id);

            if (assignmentPath.trimmed().isEmpty() || !QFileInfo::exists(assignmentPath)) {
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
            const QJsonObject metadataNoSyncTime = buildMetadata(course, assignment);
            const QString newHash = Utils::sha256Json(metadataNoSyncTime);
            const QString oldHash = m_syncStateManager.assignmentMetadataHash(course.id, assignment.id);

            QJsonObject metadataToWrite = metadataNoSyncTime;
            metadataToWrite.insert(QStringLiteral("syncedAt"), Utils::nowIsoStringUtc());

            const bool metadataWritten = m_folderOrganizer.writeMetadataIfChanged(metadataPath, metadataToWrite, newHash, oldHash);
            const bool metadataFileExists = QFileInfo::exists(metadataPath);

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
