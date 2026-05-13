#include "MainWindow.hpp"

#include "../GoogleAuth.hpp"
#include "../SyncManager.hpp"
#include "../Utils.hpp"
#include "ActivityDrawerWidget.hpp"
#include "AssignmentDetailWidget.hpp"
#include "BreadcrumbWidget.hpp"
#include "CourseDetailWidget.hpp"
#include "HomeDashboardWidget.hpp"
#include "MetadataReader.hpp"
#include "PathBarWidget.hpp"
#include "StatusBarWidget.hpp"
#include "TopBarWidget.hpp"

#include <QAction>
#include <QCoreApplication>
#include <QCursor>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QStackedWidget>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>
#include <algorithm>

namespace {

QJsonObject parseJsonFileObject(const QString &path)
{
    QFile file(path);
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
        return QJsonObject();
    }

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    if (!doc.isObject()) {
        return QJsonObject();
    }

    return doc.object();
}

QString courseStatusFromCounts(int totalTasks, int pending, int errors)
{
    if (errors > 0) {
        return QStringLiteral("error");
    }
    if (totalTasks <= 0) {
        return QStringLiteral("idle");
    }
    if (pending > 0) {
        return QStringLiteral("pending");
    }
    return QStringLiteral("complete");
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_syncManager(new SyncManager(this))
{
    setupUi();
    connectSignals();

    m_syncManager->setAutoDownloadAttachments(false);

    appendLog(QStringLiteral("INFO  Interfaz lista. Flujo activo: Inicio -> Materia -> Tarea."));
    appendLog(QStringLiteral("INFO  Config local: %1").arg(m_syncManager->configManager().configDir()));

    refreshAllViews();
    showHome();
}

void MainWindow::setupUi()
{
    setWindowTitle(QStringLiteral("Classroom Vault / TareaSync"));
    resize(1440, 920);

    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_topBar = new TopBarWidget(central);
    layout->addWidget(m_topBar);

    m_pathBar = new PathBarWidget(central);
    layout->addWidget(m_pathBar);

    m_breadcrumb = new BreadcrumbWidget(central);
    layout->addWidget(m_breadcrumb);

    m_stack = new QStackedWidget(central);
    m_home = new HomeDashboardWidget(m_stack);
    m_courseDetail = new CourseDetailWidget(m_stack);
    m_assignmentDetail = new AssignmentDetailWidget(m_stack);

    m_stack->addWidget(m_home);
    m_stack->addWidget(m_courseDetail);
    m_stack->addWidget(m_assignmentDetail);
    layout->addWidget(m_stack, 1);

    m_activityDrawer = new ActivityDrawerWidget(central);
    layout->addWidget(m_activityDrawer);

    m_statusBarWidget = new StatusBarWidget(central);
    layout->addWidget(m_statusBarWidget);

    setCentralWidget(central);
}

void MainWindow::connectSignals()
{
    connect(m_topBar, &TopBarWidget::syncRequested, this, &MainWindow::onSyncFolders);
    connect(m_topBar, &TopBarWidget::searchRequested, this, &MainWindow::onTopBarSearchChanged);
    connect(m_topBar, &TopBarWidget::searchTextChanged, this, &MainWindow::onTopBarSearchChanged);
    connect(m_topBar, &TopBarWidget::accountRequested, this, &MainWindow::onTopBarAccountRequested);

    connect(m_pathBar, &PathBarWidget::changeBasePathRequested, this, &MainWindow::onBrowseBasePath);
    connect(m_pathBar, &PathBarWidget::openBasePathRequested, this, &MainWindow::onOpenBaseFolder);
    connect(m_pathBar, &PathBarWidget::syncAllRequested, this, &MainWindow::onSyncFolders);
    connect(m_pathBar, &PathBarWidget::downloadAttachmentsRequested, this, &MainWindow::onDownloadAttachments);

    connect(m_home, &HomeDashboardWidget::syncRequested, this, &MainWindow::onSyncFolders);
    connect(m_home, &HomeDashboardWidget::openBasePathRequested, this, &MainWindow::onOpenBaseFolder);
    connect(m_home, &HomeDashboardWidget::openCourseRequested, this, &MainWindow::onHomeCourseSelected);
    connect(m_home, &HomeDashboardWidget::openFolderRequested, this, &MainWindow::onOpenCourseFolder);
    connect(m_home, &HomeDashboardWidget::syncCourseRequested, this, &MainWindow::onSyncCourseRequested);
    connect(m_home, &HomeDashboardWidget::downloadAttachmentsRequested, this, [this](const QString &courseId) {
        appendLog(QStringLiteral("INFO  Descarga por curso (%1) se resuelve con descarga global.").arg(courseId));
        onDownloadAttachments();
    });
    connect(m_home, &HomeDashboardWidget::openClassroomRequested, this, &MainWindow::onOpenCourseClassroom);
    connect(m_home, &HomeDashboardWidget::showHistoryRequested, this, [this]() {
        appendLog(QStringLiteral("INFO  Abre Actividad para ver el historial reciente."));
    });

    connect(m_courseDetail, &CourseDetailWidget::backRequested, this, &MainWindow::onCourseBackRequested);
    connect(m_courseDetail, &CourseDetailWidget::assignmentSelected, this, &MainWindow::onAssignmentSelected);
    connect(m_courseDetail, &CourseDetailWidget::openAssignmentFolderRequested, this, &MainWindow::onOpenAssignmentFolder);
    connect(m_courseDetail, &CourseDetailWidget::openAssignmentClassroomRequested, this, &MainWindow::onOpenAssignmentClassroom);
    connect(m_courseDetail, &CourseDetailWidget::downloadAssignmentAttachmentsRequested, this, &MainWindow::onDownloadAssignmentAttachments);
    connect(m_courseDetail, &CourseDetailWidget::syncCourseRequested, this, &MainWindow::onSyncCourseRequested);
    connect(m_courseDetail, &CourseDetailWidget::downloadCourseAttachmentsRequested, this, [this](const QString &courseId) {
        appendLog(QStringLiteral("INFO  Descarga por curso (%1) se resuelve con descarga global.").arg(courseId));
        onDownloadAttachments();
    });
    connect(m_courseDetail, &CourseDetailWidget::openCourseFolderRequested, this, &MainWindow::onOpenCourseFolder);
    connect(m_courseDetail, &CourseDetailWidget::openCourseClassroomRequested, this, &MainWindow::onOpenCourseClassroom);

    connect(m_assignmentDetail, &AssignmentDetailWidget::backRequested, this, &MainWindow::onAssignmentBackRequested);
    connect(m_assignmentDetail, &AssignmentDetailWidget::openClassroomRequested, this, &MainWindow::onOpenAssignmentClassroom);
    connect(m_assignmentDetail, &AssignmentDetailWidget::openFolderRequested, this, &MainWindow::onOpenAssignmentFolder);
    connect(m_assignmentDetail, &AssignmentDetailWidget::resyncMetadataRequested, this, &MainWindow::onResyncAssignmentMetadata);
    connect(m_assignmentDetail, &AssignmentDetailWidget::downloadAttachmentsRequested, this, &MainWindow::onDownloadAssignmentAttachments);

    connect(m_breadcrumb, &BreadcrumbWidget::homeRequested, this, &MainWindow::onBreadcrumbHomeRequested);
    connect(m_breadcrumb, &BreadcrumbWidget::courseRequested, this, &MainWindow::onBreadcrumbCourseRequested);

    connect(m_activityDrawer, &ActivityDrawerWidget::clearRequested, this, &MainWindow::onClearLogs);

    connect(m_syncManager, &SyncManager::coursesChanged, this, &MainWindow::onCoursesChanged);
    connect(m_syncManager, &SyncManager::assignmentsChanged, this, &MainWindow::onAssignmentsChanged);
    connect(m_syncManager, &SyncManager::syncProgress, this, &MainWindow::onSyncProgress);
    connect(m_syncManager, &SyncManager::syncFinished, this, &MainWindow::onSyncFinished);
    connect(m_syncManager, &SyncManager::countersChanged, this, &MainWindow::onCountersChanged);
    connect(m_syncManager, &SyncManager::attachmentProgress, this, &MainWindow::onAttachmentProgress);
    connect(m_syncManager, &SyncManager::attachmentFinished, this, &MainWindow::onAttachmentFinished);
    connect(m_syncManager, &SyncManager::attachmentCountersChanged, this, &MainWindow::onAttachmentCountersChanged);
    connect(m_syncManager, &SyncManager::logMessage, this, &MainWindow::appendLog);
    connect(m_syncManager, &SyncManager::errorOccurred, this, &MainWindow::appendError);

    GoogleAuth *auth = m_syncManager->googleAuth();
    connect(auth, &GoogleAuth::browserOpened, this, [this](const QUrl &) {
        appendLog(QStringLiteral("INFO  Navegador abierto para autenticacion OAuth."));
    });
    connect(auth, &GoogleAuth::authenticated, this, [this](const QString &) {
        appendLog(QStringLiteral("INFO  Autorizacion completada."));
        refreshAuthUi();
    });
    connect(auth, &GoogleAuth::tokenRefreshed, this, [this](const QString &) {
        appendLog(QStringLiteral("INFO  Token actualizado automaticamente."));
        refreshAuthUi();
    });
    connect(auth, &GoogleAuth::authStatusChanged, this, &MainWindow::onAuthStatusChanged);
    connect(auth, &GoogleAuth::authFailed, this, &MainWindow::onAuthFailed);
}

QString MainWindow::resolveSampleDataPath() const
{
    const QStringList candidates = {
        QDir::current().filePath(QStringLiteral("sample_classroom_data.json")),
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("sample_classroom_data.json")),
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("../sample_classroom_data.json")),
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("../../sample_classroom_data.json")),
    };

    for (const QString &path : candidates) {
        if (QFileInfo::exists(path)) {
            return path;
        }
    }

    return QString();
}

QString MainWindow::formatIsoDateTime(const QString &isoText) const
{
    const QString clean = isoText.trimmed();
    if (clean.isEmpty()) {
        return QStringLiteral("—");
    }

    QDateTime dt = QDateTime::fromString(clean, Qt::ISODate);
    if (!dt.isValid()) {
        return clean;
    }

    if (dt.timeSpec() == Qt::UTC || dt.offsetFromUtc() == 0) {
        dt = dt.toLocalTime();
    }

    return dt.toString(QStringLiteral("yyyy-MM-dd HH:mm"));
}

void MainWindow::showHome()
{
    m_currentPage = ViewPage::Home;
    m_currentCourseId.clear();
    m_currentAssignmentId.clear();

    m_stack->setCurrentWidget(m_home);
    m_breadcrumb->setHome();
    m_topBar->setPageTitle(QStringLiteral("Inicio"));
    m_topBar->setSearchPlaceholder(QStringLiteral("Buscar materias, tareas o archivos…"));

    m_home->setSearchText(m_homeSearchText);
    refreshHomeUi();
}

void MainWindow::showCourseDetail(const QString &courseId)
{
    const Course *course = findCourse(courseId);
    if (!course) {
        appendError(QStringLiteral("Curso no encontrado: %1").arg(courseId));
        return;
    }

    m_currentPage = ViewPage::CourseDetail;
    m_currentCourseId = courseId;
    m_currentAssignmentId.clear();

    m_stack->setCurrentWidget(m_courseDetail);
    m_breadcrumb->setCourse(courseId, course->name);
    m_topBar->setPageTitle(course->name);
    m_topBar->setSearchPlaceholder(QStringLiteral("Buscar tareas de esta materia…"));

    refreshCourseUi();
}

void MainWindow::showAssignmentDetail(const QString &courseId, const QString &assignmentId)
{
    const Course *course = findCourse(courseId);
    const Assignment *assignment = findAssignment(courseId, assignmentId);
    if (!course || !assignment) {
        appendError(QStringLiteral("No se encontro la tarea seleccionada."));
        return;
    }

    m_currentPage = ViewPage::AssignmentDetail;
    m_currentCourseId = courseId;
    m_currentAssignmentId = assignmentId;

    m_stack->setCurrentWidget(m_assignmentDetail);
    m_breadcrumb->setAssignment(courseId, course->name, assignmentId, Utils::effectiveAssignmentTitle(*assignment));
    m_topBar->setPageTitle(Utils::effectiveAssignmentTitle(*assignment));
    m_topBar->setSearchPlaceholder(QStringLiteral("Buscar adjuntos o detalles…"));

    refreshAssignmentUi();
}

QVector<CourseUiState> MainWindow::buildCourseUiStates() const
{
    QHash<QString, QList<Assignment>> assignmentsByCourse;
    for (const Assignment &assignment : m_currentAssignments) {
        assignmentsByCourse[assignment.courseId].append(assignment);
    }

    QVector<CourseUiState> result;
    result.reserve(m_currentCourses.size());

    for (const Course &course : m_currentCourses) {
        CourseUiState ui;
        ui.id = course.id;
        ui.name = course.name;
        ui.code = course.section;
        ui.semester = m_syncManager->semesterForCourse(course.id);
        ui.classroomUrl = course.alternateLink;
        ui.folderPath = m_syncManager->courseFolderPath(course.id);

        const QList<Assignment> list = assignmentsByCourse.value(course.id);
        ui.totalTasks = list.size();

        QDateTime newest;
        for (const Assignment &assignment : list) {
            const QJsonObject state = m_syncManager->assignmentState(course.id, assignment.id);
            const QString folder = state.value(QStringLiteral("folderPath")).toString().trimmed();
            if (!folder.isEmpty() && QFileInfo::exists(folder)) {
                ++ui.backedUpTasks;
            }

            const QJsonObject attachments = m_syncManager->assignmentAttachmentsState(course.id, assignment.id);
            ui.attachments += attachmentObjectCount(attachments);

            const QString updatedIso = state.value(QStringLiteral("lastUpdated")).toString().trimmed();
            const QString seenIso = state.value(QStringLiteral("lastSeen")).toString().trimmed();
            const QString candidateIso = !updatedIso.isEmpty() ? updatedIso : seenIso;
            const QDateTime dt = QDateTime::fromString(candidateIso, Qt::ISODate);
            if (dt.isValid() && (!newest.isValid() || dt > newest)) {
                newest = dt;
            }

            if (!folder.isEmpty() && !QFileInfo::exists(folder)) {
                ++ui.errors;
            }
        }

        ui.pending = qMax(0, ui.totalTasks - ui.backedUpTasks);
        ui.lastSync = newest.isValid() ? formatIsoDateTime(newest.toString(Qt::ISODate)) : QStringLiteral("—");
        ui.status = courseStatusFromCounts(ui.totalTasks, ui.pending, ui.errors);

        result.append(ui);
    }

    return result;
}

QVector<AssignmentListItemData> MainWindow::buildCourseAssignments(const QString &courseId) const
{
    QVector<AssignmentListItemData> result;

    for (const Assignment &assignment : m_currentAssignments) {
        if (assignment.courseId != courseId) {
            continue;
        }

        AssignmentListItemData item;
        item.courseId = courseId;
        item.assignmentId = assignment.id;
        item.title = Utils::effectiveAssignmentTitle(assignment);
        item.dueDateText = assignment.dueDate.isValid() ? assignment.dueDate.toString(QStringLiteral("yyyy-MM-dd")) : QStringLiteral("Sin fecha");
        item.stateText = assignment.state;
        item.classroomUrl = assignment.alternateLink;

        const QJsonObject state = m_syncManager->assignmentState(courseId, assignment.id);
        item.folderPath = state.value(QStringLiteral("folderPath")).toString().trimmed();

        const QString metadataPath = m_syncManager->assignmentMetadataPath(courseId, assignment.id);
        item.metadataStatus = QFileInfo::exists(metadataPath) ? QStringLiteral("Metadata OK") : QStringLiteral("Pendiente");

        const QJsonObject attachments = m_syncManager->assignmentAttachmentsState(courseId, assignment.id);
        item.attachmentsCount = attachmentObjectCount(attachments);
        item.attachmentsStatus = item.attachmentsCount > 0 ? QStringLiteral("Adjuntos guardados") : QStringLiteral("Sin adjuntos");

        const QString updatedIso = state.value(QStringLiteral("lastUpdated")).toString().trimmed();
        const QString seenIso = state.value(QStringLiteral("lastSeen")).toString().trimmed();
        item.lastSyncText = formatIsoDateTime(!updatedIso.isEmpty() ? updatedIso : seenIso);

        QString preview = assignment.description.trimmed();
        if (preview.length() > 160) {
            preview = preview.left(157) + QStringLiteral("...");
        }
        item.descriptionPreview = preview;

        result.append(item);
    }

    std::sort(result.begin(), result.end(), [](const AssignmentListItemData &a, const AssignmentListItemData &b) {
        return a.title.toLower() < b.title.toLower();
    });

    return result;
}

AssignmentPreviewData MainWindow::buildAssignmentPreview(const QString &courseId, const QString &assignmentId) const
{
    AssignmentPreviewData preview;

    const Assignment *assignment = findAssignment(courseId, assignmentId);
    const Course *course = findCourse(courseId);

    const QString metadataPath = m_syncManager->assignmentMetadataPath(courseId, assignmentId);
    QString metadataError;
    if (MetadataReader::readMetadataFile(metadataPath, &preview, &metadataError)) {
        preview.metadataPath = metadataPath;
        preview.localFolderPath = m_syncManager->assignmentFolderPath(courseId, assignmentId);

        if (preview.courseName.trimmed().isEmpty() && course) {
            preview.courseName = course->name;
        }
        if (preview.alternateLink.trimmed().isEmpty() && assignment) {
            preview.alternateLink = assignment->alternateLink;
        }

        return preview;
    }

    preview.courseId = courseId;
    preview.assignmentId = assignmentId;
    preview.courseName = course ? course->name : courseId;
    preview.title = assignment ? Utils::effectiveAssignmentTitle(*assignment) : QStringLiteral("Tarea");
    preview.description = assignment ? assignment->description : QString();
    preview.workType = assignment ? assignment->workType : QString();
    preview.state = assignment ? assignment->state : QString();
    preview.dueDateText = assignment && assignment->dueDate.isValid() ? assignment->dueDate.toString(QStringLiteral("yyyy-MM-dd")) : QStringLiteral("Sin fecha");
    preview.dueTimeText = assignment && assignment->dueTime.isValid() ? assignment->dueTime.toString(QStringLiteral("HH:mm")) : QStringLiteral("Sin hora");
    preview.alternateLink = assignment ? assignment->alternateLink : QString();
    preview.localFolderPath = m_syncManager->assignmentFolderPath(courseId, assignmentId);
    preview.metadataPath = metadataPath;

    const QJsonObject state = m_syncManager->assignmentState(courseId, assignmentId);
    preview.syncedAt = state.value(QStringLiteral("lastUpdated")).toString();

    const QJsonObject attachments = m_syncManager->assignmentAttachmentsState(courseId, assignmentId);
    for (auto it = attachments.begin(); it != attachments.end(); ++it) {
        if (!it.value().isObject()) {
            continue;
        }

        const QJsonObject obj = it.value().toObject();
        AttachmentUiState attachment;
        attachment.id = it.key();
        attachment.title = obj.value(QStringLiteral("title")).toString();
        attachment.fileName = obj.value(QStringLiteral("localFileName")).toString();
        if (attachment.fileName.isEmpty()) {
            attachment.fileName = obj.value(QStringLiteral("name")).toString();
        }
        attachment.type = obj.value(QStringLiteral("type")).toString();
        attachment.status = obj.value(QStringLiteral("status")).toString();
        attachment.localPath = obj.value(QStringLiteral("localPath")).toString();
        attachment.url = obj.value(QStringLiteral("url")).toString();
        if (attachment.url.isEmpty()) {
            attachment.url = obj.value(QStringLiteral("webViewLink")).toString();
        }
        attachment.sourceMimeType = obj.value(QStringLiteral("sourceMimeType")).toString();
        attachment.exportMimeType = obj.value(QStringLiteral("exportMimeType")).toString();
        if (obj.contains(QStringLiteral("size"))) {
            const qint64 size = obj.value(QStringLiteral("size")).toVariant().toLongLong();
            if (size > 0) {
                attachment.sizeText = QString::number(size);
            }
        }
        attachment.existsLocally = !attachment.localPath.trimmed().isEmpty() && QFileInfo::exists(attachment.localPath);
        preview.attachments.append(attachment);
    }

    if (!metadataError.trimmed().isEmpty()) {
        preview.description = preview.description.trimmed().isEmpty()
            ? QStringLiteral("No se encontro metadata.json para esta tarea.")
            : preview.description;
    }

    return preview;
}

QVector<ActivityItem> MainWindow::recentActivityItems(int maxItems) const
{
    QVector<ActivityItem> items;
    const int count = qMin(maxItems, m_logLines.size());
    items.reserve(count);

    for (int i = m_logLines.size() - 1; i >= 0 && items.size() < count; --i) {
        const QString line = m_logLines.at(i);

        ActivityItem item;
        item.time = QStringLiteral("--:--:--");
        item.kind = QStringLiteral("INFO");
        item.message = line;

        if (line.startsWith(QLatin1Char('[')) && line.size() > 10) {
            item.time = line.mid(1, 8);
            const QString payload = line.mid(11).trimmed();
            const int firstSpace = payload.indexOf(QLatin1Char(' '));
            if (firstSpace > 0) {
                item.kind = payload.left(firstSpace).trimmed();
                item.message = payload.mid(firstSpace).trimmed();
            } else {
                item.message = payload;
            }
        }

        items.append(item);
    }

    return items;
}

const Course *MainWindow::findCourse(const QString &courseId) const
{
    for (const Course &course : m_currentCourses) {
        if (course.id == courseId) {
            return &course;
        }
    }
    return nullptr;
}

const Assignment *MainWindow::findAssignment(const QString &courseId, const QString &assignmentId) const
{
    for (const Assignment &assignment : m_currentAssignments) {
        if (assignment.courseId == courseId && assignment.id == assignmentId) {
            return &assignment;
        }
    }
    return nullptr;
}

CourseUiState MainWindow::courseUiById(const QString &courseId) const
{
    const QVector<CourseUiState> all = buildCourseUiStates();
    for (const CourseUiState &course : all) {
        if (course.id == courseId) {
            return course;
        }
    }

    CourseUiState fallback;
    fallback.id = courseId;
    fallback.name = courseId;
    fallback.semester = QStringLiteral("Sin semestre");
    fallback.status = QStringLiteral("idle");
    return fallback;
}

int MainWindow::attachmentObjectCount(const QJsonObject &attachmentsObject) const
{
    int count = 0;
    for (auto it = attachmentsObject.begin(); it != attachmentsObject.end(); ++it) {
        if (it.value().isObject()) {
            ++count;
        }
    }
    return count;
}

void MainWindow::onBrowseBasePath()
{
    const QString selected = QFileDialog::getExistingDirectory(this, QStringLiteral("Selecciona ruta base"), m_syncManager->basePath());
    if (selected.isEmpty()) {
        return;
    }

    m_syncManager->setBasePath(selected);
    appendLog(QStringLiteral("INFO  Ruta base guardada: %1").arg(selected));
    refreshPathUi();
    refreshHomeUi();
}

void MainWindow::onOpenBaseFolder()
{
    const QString basePath = m_syncManager->basePath().trimmed();
    if (basePath.isEmpty()) {
        appendError(QStringLiteral("Ruta base vacia."));
        return;
    }

    if (!QFileInfo::exists(basePath)) {
        appendError(QStringLiteral("Ruta base inexistente: %1").arg(basePath));
        return;
    }

    QDesktopServices::openUrl(QUrl::fromLocalFile(basePath));
}

void MainWindow::onClearLogs()
{
    m_logLines.clear();
    refreshActivityUi();
    refreshStatusUi();
}

void MainWindow::onLogin()
{
    GoogleAuth *auth = m_syncManager->googleAuth();

    if (!auth->isConfigured()) {
        QMessageBox::warning(
            this,
            QStringLiteral("OAuth no configurado"),
            QStringLiteral("Completa clientId/clientSecret en %1")
                .arg(m_syncManager->configManager().configPath()));
        return;
    }

    if (auth->hasValidAccessToken()) {
        appendLog(QStringLiteral("INFO  Ya hay una sesion activa."));
        refreshAuthUi();
        return;
    }

    if (!auth->refreshToken().trimmed().isEmpty()) {
        appendLog(QStringLiteral("INFO  Intentando refrescar sesion..."));
        auth->refreshAccessToken();
        return;
    }

    appendLog(QStringLiteral("INFO  Abriendo navegador para iniciar sesion con Google..."));
    auth->startAuthorization();
}

void MainWindow::onLogout()
{
    m_syncManager->googleAuth()->signOut();
    appendLog(QStringLiteral("INFO  Sesion cerrada localmente."));
    refreshAuthUi();
}

void MainWindow::onLoadSampleData()
{
    const QString samplePath = resolveSampleDataPath();
    if (samplePath.isEmpty()) {
        appendLog(QStringLiteral("INFO  No se encontro sample local; se intentara recurso embebido."));
    } else {
        appendLog(QStringLiteral("INFO  Cargando sample desde: %1").arg(samplePath));
    }

    m_syncManager->loadSampleData(samplePath);
}

void MainWindow::onLoadClassroom()
{
    m_syncManager->fetchFromClassroom();
}

void MainWindow::onSyncFolders()
{
    const QString basePath = m_syncManager->basePath().trimmed();
    if (basePath.isEmpty()) {
        appendError(QStringLiteral("Ruta base vacia. Selecciona una carpeta primero."));
        return;
    }

    m_runtimeStatus = QStringLiteral("Sincronizando");
    m_syncProgressCurrent = 0;
    m_syncProgressTotal = 0;
    refreshStatusUi();

    m_syncManager->syncFolders();
}

void MainWindow::onDownloadAttachments()
{
    m_runtimeStatus = QStringLiteral("Descargando adjuntos");
    m_syncProgressCurrent = 0;
    m_syncProgressTotal = 0;
    refreshStatusUi();

    m_syncManager->downloadAttachments();
}

void MainWindow::onAuthStatusChanged(const QString &status)
{
    if (status == QStringLiteral("waiting_callback")) {
        appendLog(QStringLiteral("INFO  Esperando autorizacion en navegador..."));
        m_runtimeStatus = QStringLiteral("Esperando OAuth");
        refreshStatusUi();
        return;
    }

    if (status == QStringLiteral("authorization_code_received")) {
        appendLog(QStringLiteral("INFO  Codigo de autorizacion recibido."));
        return;
    }

    if (status == QStringLiteral("manual_code_required")) {
        appendError(QStringLiteral("No se pudo iniciar servidor local de autenticacion. Se usara fallback manual."));

        bool ok = false;
        const QString code = QInputDialog::getText(
            this,
            QStringLiteral("Codigo de autorizacion"),
            QStringLiteral("Pega aqui el authorization code de Google:"),
            QLineEdit::Normal,
            QString(),
            &ok);

        if (!ok || code.trimmed().isEmpty()) {
            appendLog(QStringLiteral("INFO  Autorizacion manual cancelada."));
            return;
        }

        m_syncManager->googleAuth()->exchangeAuthorizationCode(code.trimmed());
        return;
    }

    if (status == QStringLiteral("authenticated") || status == QStringLiteral("token_refreshed")) {
        m_runtimeStatus = QStringLiteral("Conectado");
        refreshStatusUi();
        refreshAuthUi();
        return;
    }

    if (status == QStringLiteral("failed") || status == QStringLiteral("cancelled") || status == QStringLiteral("signed_out")) {
        m_runtimeStatus = QStringLiteral("No conectado");
        refreshStatusUi();
        refreshAuthUi();
    }
}

void MainWindow::onAuthFailed(const QString &errorMessage)
{
    Q_UNUSED(errorMessage)
    refreshAuthUi();
}

void MainWindow::onCoursesChanged(const QList<Course> &courses)
{
    m_currentCourses = courses;
    refreshAllViews();
}

void MainWindow::onAssignmentsChanged(const QList<Assignment> &assignments)
{
    m_currentAssignments = assignments;
    refreshAllViews();
}

void MainWindow::onSyncProgress(int current, int total)
{
    m_syncProgressCurrent = current;
    m_syncProgressTotal = total;
    m_runtimeStatus = QStringLiteral("Sincronizando");
    refreshStatusUi();
}

void MainWindow::onSyncFinished(int newCount, int updatedCount, int unchangedCount, int errorCount)
{
    m_runtimeStatus = QStringLiteral("Listo");
    m_syncProgressCurrent = m_syncProgressTotal;
    refreshStatusUi();

    appendLog(
        QStringLiteral("INFO  Resultado sync -> nuevas: %1, actualizadas: %2, sin cambios: %3, errores: %4")
            .arg(newCount)
            .arg(updatedCount)
            .arg(unchangedCount)
            .arg(errorCount));

    refreshAllViews();
}

void MainWindow::onCountersChanged(
    int courses,
    int assignments,
    int newCount,
    int updatedCount,
    int unchangedCount,
    int errorCount)
{
    m_coursesCount = courses;
    m_assignmentsCount = assignments;
    m_newCount = newCount;
    m_updatedCount = updatedCount;
    m_unchangedCount = unchangedCount;
    m_errorCount = errorCount;

    refreshStatusUi();
    refreshHomeUi();
}

void MainWindow::onAttachmentProgress(int current, int total)
{
    m_syncProgressCurrent = current;
    m_syncProgressTotal = total;
    m_runtimeStatus = QStringLiteral("Descargando adjuntos");
    refreshStatusUi();
}

void MainWindow::onAttachmentFinished(int downloaded, int skipped, int errors)
{
    Q_UNUSED(downloaded)
    Q_UNUSED(skipped)
    Q_UNUSED(errors)

    m_runtimeStatus = QStringLiteral("Listo");
    m_syncProgressCurrent = m_syncProgressTotal;
    refreshStatusUi();

    refreshAllViews();
}

void MainWindow::onAttachmentCountersChanged(int blobDownloaded, int exported, int linksSaved, int skipped, int errors)
{
    m_attachmentBlobDownloaded = blobDownloaded;
    m_attachmentExported = exported;
    m_attachmentLinksSaved = linksSaved;
    m_attachmentSkipped = skipped;
    m_attachmentErrors = errors;

    refreshStatusUi();
    refreshHomeUi();
}

void MainWindow::onHomeCourseSelected(const QString &courseId)
{
    showCourseDetail(courseId);
}

void MainWindow::onOpenCourseFolder(const QString &courseId)
{
    const QString path = m_syncManager->courseFolderPath(courseId).trimmed();
    if (path.isEmpty() || !QFileInfo::exists(path)) {
        appendError(QStringLiteral("No hay carpeta sincronizada para este curso."));
        return;
    }

    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

void MainWindow::onSyncCourseRequested(const QString &courseId)
{
    appendLog(QStringLiteral("INFO  Sincronizacion por curso (%1) pendiente. Se ejecuta sincronizacion general.").arg(courseId));
    onSyncFolders();
}

void MainWindow::onOpenCourseClassroom(const QString &courseId)
{
    const Course *course = findCourse(courseId);
    if (!course || course->alternateLink.trimmed().isEmpty()) {
        appendError(QStringLiteral("Curso sin enlace de Classroom."));
        return;
    }

    QDesktopServices::openUrl(QUrl(course->alternateLink));
}

void MainWindow::onCourseBackRequested()
{
    showHome();
}

void MainWindow::onAssignmentSelected(const QString &courseId, const QString &assignmentId)
{
    showAssignmentDetail(courseId, assignmentId);
}

void MainWindow::onOpenAssignmentFolder(const QString &courseId, const QString &assignmentId)
{
    const QString path = m_syncManager->assignmentFolderPath(courseId, assignmentId).trimmed();
    if (path.isEmpty() || !QFileInfo::exists(path)) {
        QMessageBox::information(this, QStringLiteral("Tarea"), QStringLiteral("La tarea no tiene carpeta sincronizada."));
        return;
    }

    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

void MainWindow::onOpenAssignmentClassroom(const QString &courseId, const QString &assignmentId)
{
    const Assignment *assignment = findAssignment(courseId, assignmentId);
    if (!assignment || assignment->alternateLink.trimmed().isEmpty()) {
        appendError(QStringLiteral("Tarea sin enlace de Classroom."));
        return;
    }

    QDesktopServices::openUrl(QUrl(assignment->alternateLink));
}

void MainWindow::onDownloadAssignmentAttachments(const QString &courseId, const QString &assignmentId)
{
    appendLog(
        QStringLiteral("INFO  Descarga por tarea (%1:%2) pendiente; se ejecuta descarga global de adjuntos.")
            .arg(courseId, assignmentId));
    onDownloadAttachments();
}

void MainWindow::onAssignmentBackRequested()
{
    if (m_currentCourseId.trimmed().isEmpty()) {
        showHome();
        return;
    }

    showCourseDetail(m_currentCourseId);
}

void MainWindow::onResyncAssignmentMetadata(const QString &courseId, const QString &assignmentId)
{
    appendLog(
        QStringLiteral("INFO  Re-sincronizando metadata de tarea (%1:%2) via sincronizacion general.")
            .arg(courseId, assignmentId));
    onSyncFolders();
}

void MainWindow::onBreadcrumbHomeRequested()
{
    showHome();
}

void MainWindow::onBreadcrumbCourseRequested(const QString &courseId)
{
    showCourseDetail(courseId);
}

void MainWindow::onTopBarSearchChanged(const QString &text)
{
    if (m_currentPage == ViewPage::Home) {
        m_homeSearchText = text.trimmed();
        m_home->setSearchText(m_homeSearchText);
        return;
    }

    if (m_currentPage == ViewPage::CourseDetail) {
        m_courseSearchText = text.trimmed();
        m_courseDetail->setSearchText(m_courseSearchText);
        return;
    }
}

void MainWindow::onTopBarAccountRequested()
{
    QMenu menu(this);
    QAction *loginAction = menu.addAction(QStringLiteral("Iniciar sesion"));
    QAction *logoutAction = menu.addAction(QStringLiteral("Cerrar sesion"));
    menu.addSeparator();
    QAction *classroomAction = menu.addAction(QStringLiteral("Cargar Classroom"));
    QAction *sampleAction = menu.addAction(QStringLiteral("Cargar datos de prueba"));

    QAction *selected = menu.exec(QCursor::pos());
    if (!selected) {
        return;
    }

    if (selected == loginAction) {
        onLogin();
    } else if (selected == logoutAction) {
        onLogout();
    } else if (selected == classroomAction) {
        onLoadClassroom();
    } else if (selected == sampleAction) {
        onLoadSampleData();
    }
}

void MainWindow::appendLog(const QString &message)
{
    const QString normalized = message.trimmed();
    const QString line = QStringLiteral("[%1] %2")
                             .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")), normalized);

    m_logLines.append(line);
    if (m_logLines.size() > 3000) {
        m_logLines.removeFirst();
    }

    m_statusBarWidget->setLastActionText(normalized);
    refreshActivityUi();
}

void MainWindow::appendError(const QString &message)
{
    QString normalized = message.trimmed();
    if (!normalized.startsWith(QStringLiteral("ERR"))) {
        normalized = QStringLiteral("ERR   ") + normalized;
    }
    appendLog(normalized);
}

void MainWindow::refreshAuthUi()
{
    GoogleAuth *auth = m_syncManager->googleAuth();

    QString state = QStringLiteral("No conectado");
    if (auth->hasValidAccessToken()) {
        state = QStringLiteral("Conectado");
    } else if (!auth->refreshToken().trimmed().isEmpty()) {
        state = QStringLiteral("Sesion guardada");
    }

    m_topBar->setConnectionState(state);
    m_topBar->setConnectedEmail(QString());
}

void MainWindow::refreshPathUi()
{
    m_pathBar->setBasePath(m_syncManager->basePath());

    const QJsonObject state = parseJsonFileObject(m_syncManager->configManager().syncStatePath());
    m_pathBar->setLastSyncText(formatIsoDateTime(state.value(QStringLiteral("lastSync")).toString()));
}

void MainWindow::refreshStatusUi()
{
    m_statusBarWidget->setStatusText(m_runtimeStatus);
    m_statusBarWidget->setErrorCount(m_errorCount + m_attachmentErrors);
    m_statusBarWidget->setProgress(m_syncProgressCurrent, m_syncProgressTotal);
}

void MainWindow::refreshActivityUi()
{
    m_activityDrawer->setItems(recentActivityItems(80));
}

void MainWindow::refreshHomeUi()
{
    const QVector<CourseUiState> courses = buildCourseUiStates();

    int pendingTotal = 0;
    int attachmentsTotal = 0;
    int courseErrors = 0;
    for (const CourseUiState &course : courses) {
        pendingTotal += course.pending;
        attachmentsTotal += course.attachments;
        courseErrors += course.errors;
    }

    QVector<KpiData> kpis;
    kpis.reserve(5);
    kpis.append(KpiData{QStringLiteral("Cursos"), m_coursesCount, QStringLiteral("[C]"), QStringLiteral("idle")});
    kpis.append(KpiData{QStringLiteral("Tareas"), m_assignmentsCount, QStringLiteral("[T]"), QStringLiteral("idle")});
    kpis.append(KpiData{QStringLiteral("Adjuntos"), attachmentsTotal, QStringLiteral("[A]"), QStringLiteral("complete")});
    kpis.append(KpiData{QStringLiteral("Pendientes"), pendingTotal, QStringLiteral("[P]"), QStringLiteral("pending")});
    kpis.append(KpiData{QStringLiteral("Errores"), m_errorCount + m_attachmentErrors + courseErrors, QStringLiteral("[E]"), QStringLiteral("error")});

    m_home->setKpis(kpis);
    m_home->setCourses(courses);
    m_home->setActivity(recentActivityItems(24));
    m_home->setBasePath(m_syncManager->basePath());
    m_home->setStorageSummary(QStringLiteral("No calculado"), attachmentsTotal);

    const QJsonObject state = parseJsonFileObject(m_syncManager->configManager().syncStatePath());
    m_home->setLastSyncText(QStringLiteral("Ultima sync · %1").arg(formatIsoDateTime(state.value(QStringLiteral("lastSync")).toString())));

    m_home->setSearchText(m_homeSearchText);
}

void MainWindow::refreshCourseUi()
{
    if (m_currentCourseId.trimmed().isEmpty()) {
        return;
    }

    m_courseDetail->setCourse(courseUiById(m_currentCourseId));
    m_courseDetail->setAssignments(buildCourseAssignments(m_currentCourseId));
    m_courseDetail->setSearchText(m_courseSearchText);
}

void MainWindow::refreshAssignmentUi()
{
    if (m_currentCourseId.trimmed().isEmpty() || m_currentAssignmentId.trimmed().isEmpty()) {
        return;
    }

    m_assignmentDetail->setPreviewData(buildAssignmentPreview(m_currentCourseId, m_currentAssignmentId));
}

void MainWindow::refreshAllViews()
{
    refreshAuthUi();
    refreshPathUi();
    refreshStatusUi();
    refreshActivityUi();
    refreshHomeUi();

    if (m_currentPage == ViewPage::CourseDetail) {
        refreshCourseUi();
    } else if (m_currentPage == ViewPage::AssignmentDetail) {
        refreshCourseUi();
        refreshAssignmentUi();
    }
}
