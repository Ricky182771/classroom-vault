#include "SyncManager.hpp"

#include "Utils.hpp"

#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>

SyncManager::SyncManager(QObject *parent)
    : QObject(parent)
    , m_classroomClient(this)
    , m_googleAuth(this)
{
    m_configManager.load();
    m_semesterByCourse = m_configManager.semesterMapping();

    m_folderOrganizer.setBasePath(m_configManager.basePath());

    refreshAuthConfig();
    m_googleAuth.loadFromJson(m_configManager.loadToken());

    connect(&m_classroomClient, &ClassroomClient::coursesFetched, this, &SyncManager::onCoursesFetched);
    connect(&m_classroomClient, &ClassroomClient::courseWorkFetched, this, &SyncManager::onCourseWorkFetched);
    connect(&m_classroomClient, &ClassroomClient::errorOccurred, this, &SyncManager::onClientError);
    connect(&m_classroomClient, &ClassroomClient::logMessage, this, &SyncManager::logMessage);

    connect(&m_googleAuth, &GoogleAuth::tokenUpdated, this, &SyncManager::onTokenUpdated);
    connect(&m_googleAuth, &GoogleAuth::authSucceeded, this, &SyncManager::onAuthSucceeded);
    connect(&m_googleAuth, &GoogleAuth::logMessage, this, &SyncManager::logMessage);
    connect(&m_googleAuth, &GoogleAuth::errorOccurred, this, &SyncManager::errorOccurred);
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
        emit errorOccurred(QStringLiteral("No se pudo guardar config.json"));
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
    m_configManager.save();
}

void SyncManager::setSemesterMapping(const QHash<QString, QString> &mapping)
{
    m_semesterByCourse = mapping;
    m_configManager.setSemesterMapping(mapping);
    m_configManager.save();
}

void SyncManager::refreshAuthConfig()
{
    m_googleAuth.setClientConfig(
        m_configManager.oauthClientId(),
        m_configManager.oauthClientSecret(),
        m_configManager.oauthRedirectUri(),
        m_configManager.oauthScopes());
}

void SyncManager::loadSampleData(const QString &samplePath)
{
    refreshAuthConfig();

    m_classroomClient.setUseSampleData(true);
    if (!samplePath.trimmed().isEmpty()) {
        m_classroomClient.setSampleDataPath(samplePath);
    }

    emit logMessage(QStringLiteral("Cargando datos de prueba de Classroom..."));
    startFetchingCourses();
}

void SyncManager::fetchFromClassroom()
{
    refreshAuthConfig();

    m_classroomClient.setUseSampleData(false);

    if (m_googleAuth.hasUsableAccessToken()) {
        m_classroomClient.setAccessToken(m_googleAuth.accessToken());
        emit logMessage(QStringLiteral("Consultando Classroom API..."));
        startFetchingCourses();
        return;
    }

    if (!m_googleAuth.refreshToken().isEmpty()) {
        emit logMessage(QStringLiteral("Access token vencido, intentando refresh token..."));
        m_waitingForTokenRefresh = true;
        m_googleAuth.refreshAccessToken();
        return;
    }

    emit errorOccurred(QStringLiteral("No hay token valido. Usa 'Iniciar sesion' primero."));
}

void SyncManager::startFetchingCourses()
{
    m_courses.clear();
    m_assignmentsByCourse.clear();
    m_pendingCourseWorkRequests = 0;

    m_classroomClient.fetchCourses();
}

void SyncManager::onCoursesFetched(const QList<Course> &courses)
{
    m_courses = courses;
    emit coursesChanged(m_courses);

    m_pendingCourseWorkRequests = m_courses.size();

    if (m_courses.isEmpty()) {
        emit logMessage(QStringLiteral("No se encontraron cursos."));
        emit assignmentsChanged({});
        return;
    }

    emit logMessage(QStringLiteral("Cursos cargados: %1").arg(m_courses.size()));

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
        emit logMessage(QStringLiteral("Tareas cargadas: %1").arg(all.size()));
    }
}

void SyncManager::onClientError(const QString &operation, const QString &message)
{
    emit errorOccurred(QStringLiteral("%1 -> %2").arg(operation, message));

    if (operation.startsWith(QStringLiteral("courseWork:")) && m_pendingCourseWorkRequests > 0) {
        --m_pendingCourseWorkRequests;
        if (m_pendingCourseWorkRequests == 0) {
            emit assignmentsChanged(allAssignments());
        }
    }
}

void SyncManager::onTokenUpdated()
{
    if (!m_configManager.saveToken(m_googleAuth.toJson())) {
        emit errorOccurred(QStringLiteral("No se pudo guardar token.json"));
    }
}

void SyncManager::onAuthSucceeded()
{
    if (m_waitingForTokenRefresh) {
        m_waitingForTokenRefresh = false;
        m_classroomClient.setAccessToken(m_googleAuth.accessToken());
        emit logMessage(QStringLiteral("Token actualizado. Consultando Classroom API..."));
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
    return metadata;
}

QString SyncManager::metadataHash(const QJsonObject &metadata) const
{
    const QByteArray compact = QJsonDocument(metadata).toJson(QJsonDocument::Compact);
    return QString::fromUtf8(QCryptographicHash::hash(compact, QCryptographicHash::Sha256).toHex());
}

void SyncManager::syncFolders()
{
    const QString configuredBasePath = m_configManager.basePath().trimmed();
    if (configuredBasePath.isEmpty()) {
        emit errorOccurred(QStringLiteral("Selecciona primero una ruta base de respaldo."));
        return;
    }

    if (m_courses.isEmpty()) {
        emit errorOccurred(QStringLiteral("No hay cursos cargados para sincronizar."));
        return;
    }

    m_folderOrganizer.setBasePath(configuredBasePath);

    QJsonObject syncState = m_configManager.loadSyncState();
    QJsonObject assignmentsState = syncState.value(QStringLiteral("assignments")).toObject();

    int changedCount = 0;
    int unchangedCount = 0;

    const QList<Assignment> all = allAssignments();
    int current = 0;
    const int total = all.size();

    for (const Course &course : m_courses) {
        const QString semester = semesterForCourse(course.id);
        const QList<Assignment> assignments = m_assignmentsByCourse.value(course.id);

        for (const Assignment &assignment : assignments) {
            ++current;
            emit syncProgress(current, total);

            const QString assignmentPath = m_folderOrganizer.createAssignmentFolder(semester, course.name, assignment);
            const QString metadataPath = QDir(assignmentPath).filePath(QStringLiteral("metadata.json"));

            const QJsonObject metadataWithoutSyncTime = buildMetadata(course, assignment);
            const QString currentHash = metadataHash(metadataWithoutSyncTime);
            const QString stateKey = course.id + QStringLiteral(":") + assignment.id;
            const QJsonObject previous = assignmentsState.value(stateKey).toObject();
            const QString previousHash = previous.value(QStringLiteral("metadataHash")).toString();
            const bool metadataFileExists = QFileInfo::exists(metadataPath);

            if (previousHash == currentHash && metadataFileExists) {
                ++unchangedCount;
            } else {
                QJsonObject metadataWithSyncTime = metadataWithoutSyncTime;
                metadataWithSyncTime.insert(QStringLiteral("syncedAt"), Utils::nowIsoStringUtc());
                if (!m_folderOrganizer.writeMetadata(metadataPath, metadataWithSyncTime)) {
                    emit errorOccurred(QStringLiteral("No se pudo escribir metadata: %1").arg(metadataPath));
                }
                ++changedCount;
            }

            QJsonObject stateEntry;
            stateEntry.insert(QStringLiteral("courseId"), course.id);
            stateEntry.insert(QStringLiteral("assignmentId"), assignment.id);
            stateEntry.insert(QStringLiteral("folderPath"), assignmentPath);
            stateEntry.insert(QStringLiteral("metadataHash"), currentHash);
            stateEntry.insert(QStringLiteral("lastSynced"), Utils::nowIsoStringUtc());
            assignmentsState.insert(stateKey, stateEntry);
        }
    }

    syncState.insert(QStringLiteral("assignments"), assignmentsState);

    if (!m_configManager.saveSyncState(syncState)) {
        emit errorOccurred(QStringLiteral("No se pudo guardar sync_state.json"));
    }

    emit syncFinished(changedCount, unchangedCount);
    emit logMessage(
        QStringLiteral("Sincronizacion terminada. Actualizados: %1, sin cambios: %2")
            .arg(changedCount)
            .arg(unchangedCount));
}
