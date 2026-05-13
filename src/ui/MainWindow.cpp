#include "MainWindow.hpp"

#include "../GoogleAuth.hpp"
#include "../SyncManager.hpp"
#include "../Utils.hpp"
#include "AttachmentsViewWidget.hpp"
#include "DashboardWidget.hpp"
#include "HistoryViewWidget.hpp"
#include "PathBarWidget.hpp"
#include "SettingsViewWidget.hpp"
#include "SidebarWidget.hpp"
#include "StatusBarWidget.hpp"
#include "TasksViewWidget.hpp"
#include "TopBarWidget.hpp"
#include "UiModels.hpp"

#include <QCheckBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>

namespace {

QString statusForCourse(const CourseUiState &course)
{
    if (course.errors > 0) {
        return QStringLiteral("error");
    }
    if (course.totalTasks <= 0) {
        return QStringLiteral("idle");
    }
    if (course.pending > 0) {
        return QStringLiteral("pending");
    }
    return QStringLiteral("complete");
}

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

int attachmentObjectCount(const QJsonObject &attachmentsObject)
{
    int count = 0;
    for (auto it = attachmentsObject.begin(); it != attachmentsObject.end(); ++it) {
        if (it.value().isObject()) {
            ++count;
        }
    }
    return count;
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_syncManager(new SyncManager(this))
{
    setupUi();
    connectSignals();

    m_syncManager->setAutoDownloadAttachments(false);

    appendLog(QStringLiteral("INFO  Interfaz lista. Carga Classroom o datos de prueba para comenzar."));
    appendLog(QStringLiteral("INFO  Config local: %1").arg(m_syncManager->configManager().configDir()));

    refreshAllViews();
}

void MainWindow::setupUi()
{
    setWindowTitle(QStringLiteral("Classroom Vault / TareaSync"));
    resize(1440, 920);

    auto *central = new QWidget(this);
    auto *root = new QHBoxLayout(central);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    m_sidebar = new SidebarWidget(central);
    root->addWidget(m_sidebar);

    auto *rightPane = new QWidget(central);
    auto *rightLayout = new QVBoxLayout(rightPane);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(0);

    m_topBar = new TopBarWidget(rightPane);
    rightLayout->addWidget(m_topBar);

    m_pathBar = new PathBarWidget(rightPane);
    rightLayout->addWidget(m_pathBar);

    m_stack = new QStackedWidget(rightPane);
    rightLayout->addWidget(m_stack, 1);

    m_statusBarWidget = new StatusBarWidget(rightPane);
    rightLayout->addWidget(m_statusBarWidget);

    m_dashboard = new DashboardWidget(m_stack);
    m_pageIndexById.insert(QStringLiteral("home"), m_stack->addWidget(m_dashboard));

    m_coursesPage = new QFrame(m_stack);
    m_coursesPage->setObjectName(QStringLiteral("Section"));
    auto *coursesLayout = new QVBoxLayout(m_coursesPage);
    coursesLayout->setContentsMargins(12, 12, 12, 12);
    coursesLayout->setSpacing(10);

    auto *coursesTitle = new QLabel(QStringLiteral("Cursos"), m_coursesPage);
    coursesTitle->setStyleSheet(QStringLiteral("font-size:16px;font-weight:700;"));
    coursesLayout->addWidget(coursesTitle);

    auto *coursesActions = new QHBoxLayout();
    coursesActions->setSpacing(8);

    m_loginButton = new QPushButton(QStringLiteral("Iniciar sesion"), m_coursesPage);
    m_loginButton->setProperty("variant", QStringLiteral("primary"));
    m_logoutButton = new QPushButton(QStringLiteral("Cerrar sesion"), m_coursesPage);
    m_loadClassroomButton = new QPushButton(QStringLiteral("Cargar Classroom"), m_coursesPage);
    m_loadSampleButton = new QPushButton(QStringLiteral("Cargar datos de prueba"), m_coursesPage);

    coursesActions->addWidget(m_loginButton);
    coursesActions->addWidget(m_logoutButton);
    coursesActions->addWidget(m_loadClassroomButton);
    coursesActions->addWidget(m_loadSampleButton);
    coursesActions->addStretch(1);
    coursesLayout->addLayout(coursesActions);

    auto *coursesHint = new QLabel(QStringLiteral("Edita la columna Semestre asignado para guardar por courseId."), m_coursesPage);
    coursesHint->setProperty("subtle", true);
    coursesLayout->addWidget(coursesHint);

    m_coursesTable = new QTableWidget(m_coursesPage);
    m_coursesTable->setColumnCount(3);
    m_coursesTable->setHorizontalHeaderLabels({
        QStringLiteral("Materia"),
        QStringLiteral("ID"),
        QStringLiteral("Semestre asignado"),
    });
    m_coursesTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_coursesTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_coursesTable->setAlternatingRowColors(true);
    auto *coursesHeader = m_coursesTable->horizontalHeader();
    coursesHeader->setSectionResizeMode(0, QHeaderView::Stretch);
    coursesHeader->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    coursesHeader->setSectionResizeMode(2, QHeaderView::Stretch);
    coursesLayout->addWidget(m_coursesTable, 1);

    m_pageIndexById.insert(QStringLiteral("courses"), m_stack->addWidget(m_coursesPage));

    m_tasksView = new TasksViewWidget(m_stack);
    m_pageIndexById.insert(QStringLiteral("tasks"), m_stack->addWidget(m_tasksView));

    m_attachmentsView = new AttachmentsViewWidget(m_stack);
    m_pageIndexById.insert(QStringLiteral("attachments"), m_stack->addWidget(m_attachmentsView));

    m_historyView = new HistoryViewWidget(m_stack);
    m_pageIndexById.insert(QStringLiteral("history"), m_stack->addWidget(m_historyView));

    m_settingsView = new SettingsViewWidget(m_stack);
    m_pageIndexById.insert(QStringLiteral("settings"), m_stack->addWidget(m_settingsView));

    root->addWidget(rightPane, 1);

    setCentralWidget(central);
    setCurrentPage(QStringLiteral("home"));
}

void MainWindow::connectSignals()
{
    connect(m_sidebar, &SidebarWidget::navigationRequested, this, &MainWindow::onNavigationRequested);

    connect(m_topBar, &TopBarWidget::toggleSidebarRequested, this, &MainWindow::onToggleSidebar);
    connect(m_topBar, &TopBarWidget::syncRequested, this, &MainWindow::onSyncFolders);
    connect(m_topBar, &TopBarWidget::settingsRequested, this, [this]() {
        setCurrentPage(QStringLiteral("settings"));
    });
    connect(m_topBar, &TopBarWidget::searchRequested, this, [this](const QString &query) {
        if (query.trimmed().isEmpty()) {
            appendLog(QStringLiteral("INFO  Busqueda vacia."));
            return;
        }
        appendLog(QStringLiteral("INFO  Busqueda visual solicitada: %1").arg(query.trimmed()));
    });

    connect(m_pathBar, &PathBarWidget::changeBasePathRequested, this, &MainWindow::onBrowseBasePath);
    connect(m_pathBar, &PathBarWidget::openBasePathRequested, this, &MainWindow::onOpenBaseFolder);
    connect(m_pathBar, &PathBarWidget::syncAllRequested, this, &MainWindow::onSyncFolders);
    connect(m_pathBar, &PathBarWidget::downloadAttachmentsRequested, this, &MainWindow::onDownloadAttachments);

    connect(m_dashboard, &DashboardWidget::syncRequested, this, &MainWindow::onSyncFolders);
    connect(m_dashboard, &DashboardWidget::openBasePathRequested, this, &MainWindow::onOpenBaseFolder);
    connect(m_dashboard, &DashboardWidget::openCourseRequested, this, &MainWindow::onDashboardOpenCourse);
    connect(m_dashboard, &DashboardWidget::openFolderRequested, this, &MainWindow::onDashboardOpenFolder);
    connect(m_dashboard, &DashboardWidget::syncCourseRequested, this, &MainWindow::onDashboardSyncCourse);
    connect(m_dashboard, &DashboardWidget::downloadAttachmentsRequested, this, &MainWindow::onDashboardDownloadAttachments);
    connect(m_dashboard, &DashboardWidget::openClassroomRequested, this, &MainWindow::onDashboardOpenClassroom);
    connect(m_dashboard, &DashboardWidget::showHistoryRequested, this, [this]() {
        setCurrentPage(QStringLiteral("history"));
    });

    connect(m_loginButton, &QPushButton::clicked, this, &MainWindow::onLogin);
    connect(m_logoutButton, &QPushButton::clicked, this, &MainWindow::onLogout);
    connect(m_loadSampleButton, &QPushButton::clicked, this, &MainWindow::onLoadSampleData);
    connect(m_loadClassroomButton, &QPushButton::clicked, this, &MainWindow::onLoadClassroom);

    connect(m_coursesTable, &QTableWidget::itemChanged, this, &MainWindow::onCourseTableItemChanged);
    connect(m_tasksView, &TasksViewWidget::taskActivated, this, &MainWindow::onTaskActivated);

    connect(m_historyView, &HistoryViewWidget::clearLogsRequested, this, &MainWindow::onClearLogs);

    connect(m_settingsView, &SettingsViewWidget::changeBasePathRequested, this, &MainWindow::onBrowseBasePath);
    connect(m_settingsView, &SettingsViewWidget::loginRequested, this, &MainWindow::onLogin);
    connect(m_settingsView, &SettingsViewWidget::logoutRequested, this, &MainWindow::onLogout);
    connect(m_settingsView, &SettingsViewWidget::loadClassroomRequested, this, &MainWindow::onLoadClassroom);
    connect(m_settingsView, &SettingsViewWidget::loadSampleRequested, this, &MainWindow::onLoadSampleData);
    connect(m_settingsView, &SettingsViewWidget::autoDownloadAttachmentsChanged, this, [this](bool enabled) {
        m_syncManager->setAutoDownloadAttachments(enabled);
        appendLog(QStringLiteral("INFO  Descargar adjuntos al sincronizar: %1").arg(enabled ? QStringLiteral("ON") : QStringLiteral("OFF")));
    });

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
    connect(auth, &GoogleAuth::authenticationStarted, this, [this]() {
        m_loginButton->setEnabled(false);
        m_runtimeStatus = QStringLiteral("Autenticando...");
        refreshStatusUi();
    });
    connect(auth, &GoogleAuth::browserOpened, this, [this](const QUrl &) {
        appendLog(QStringLiteral("INFO  Navegador abierto para autenticacion OAuth."));
    });
    connect(auth, &GoogleAuth::authenticated, this, [this](const QString &) {
        m_loginButton->setEnabled(true);
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

void MainWindow::setCurrentPage(const QString &pageId)
{
    const QString normalized = pageId.trimmed().isEmpty() ? QStringLiteral("home") : pageId.trimmed();
    const int index = m_pageIndexById.value(normalized, m_pageIndexById.value(QStringLiteral("home"), 0));

    m_stack->setCurrentIndex(index);
    m_sidebar->setCurrentPage(normalized);

    if (normalized == QStringLiteral("home")) {
        m_topBar->setBreadcrumb(QStringLiteral("Inicio"), QStringLiteral("Resumen de respaldo"));
    } else if (normalized == QStringLiteral("courses")) {
        m_topBar->setBreadcrumb(QStringLiteral("Cursos"), QStringLiteral("Materias y semestres"));
    } else if (normalized == QStringLiteral("tasks")) {
        m_topBar->setBreadcrumb(QStringLiteral("Tareas"), QStringLiteral("Vista de tareas"));
    } else if (normalized == QStringLiteral("attachments")) {
        m_topBar->setBreadcrumb(QStringLiteral("Adjuntos"), QStringLiteral("Archivos y enlaces"));
    } else if (normalized == QStringLiteral("history")) {
        m_topBar->setBreadcrumb(QStringLiteral("Historial"), QStringLiteral("Actividad y logs"));
    } else if (normalized == QStringLiteral("settings")) {
        m_topBar->setBreadcrumb(QStringLiteral("Configuracion"), QStringLiteral("Preferencias"));
    }
}

QJsonObject MainWindow::readSyncState() const
{
    return parseJsonFileObject(m_syncManager->configManager().syncStatePath());
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

void MainWindow::onBrowseBasePath()
{
    const QString selected = QFileDialog::getExistingDirectory(this, QStringLiteral("Selecciona ruta base"), m_syncManager->basePath());
    if (selected.isEmpty()) {
        return;
    }

    m_syncManager->setBasePath(selected);
    appendLog(QStringLiteral("INFO  Ruta base guardada: %1").arg(selected));
    refreshPathUi();
    refreshDashboard();
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
    refreshHistoryView();
    refreshDashboard();
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
        m_loginButton->setEnabled(false);
        auth->refreshAccessToken();
        return;
    }

    appendLog(QStringLiteral("INFO  Abriendo navegador para iniciar sesion con Google..."));
    m_loginButton->setEnabled(false);
    auth->startAuthorization();
}

void MainWindow::onLogout()
{
    m_syncManager->googleAuth()->signOut();
    appendLog(QStringLiteral("INFO  Sesion cerrada localmente."));
    m_loginButton->setEnabled(true);
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

    QHash<QString, QString> mapping;
    for (int row = 0; row < m_coursesTable->rowCount(); ++row) {
        auto *idItem = m_coursesTable->item(row, 1);
        auto *semesterItem = m_coursesTable->item(row, 2);
        if (!idItem) {
            continue;
        }

        const QString id = idItem->text().trimmed();
        const QString semester = semesterItem ? semesterItem->text().trimmed() : QString();
        if (!id.isEmpty() && !semester.isEmpty()) {
            mapping.insert(id, semester);
        }
    }

    m_syncManager->setSemesterMapping(mapping);
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
            m_loginButton->setEnabled(true);
            return;
        }

        m_syncManager->googleAuth()->exchangeAuthorizationCode(code.trimmed());
        return;
    }

    if (status == QStringLiteral("authenticated") || status == QStringLiteral("token_refreshed")) {
        m_loginButton->setEnabled(true);
        m_runtimeStatus = QStringLiteral("Conectado");
        refreshStatusUi();
        refreshAuthUi();
        return;
    }

    if (status == QStringLiteral("failed") || status == QStringLiteral("cancelled") || status == QStringLiteral("signed_out")) {
        m_loginButton->setEnabled(true);
        m_runtimeStatus = QStringLiteral("No conectado");
        refreshStatusUi();
        refreshAuthUi();
    }
}

void MainWindow::onAuthFailed(const QString &errorMessage)
{
    Q_UNUSED(errorMessage)
    m_loginButton->setEnabled(true);
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

void MainWindow::onCourseTableItemChanged(QTableWidgetItem *item)
{
    if (!item || m_updatingCourseTable || item->column() != 2) {
        return;
    }

    auto *idItem = m_coursesTable->item(item->row(), 1);
    if (!idItem) {
        return;
    }

    m_syncManager->setSemesterForCourse(idItem->text().trimmed(), item->text().trimmed());
    refreshDashboard();
}

void MainWindow::onTaskActivated(const QString &courseId, const QString &assignmentId)
{
    const QString path = m_syncManager->assignmentFolderPath(courseId, assignmentId);
    if (path.trimmed().isEmpty() || !QFileInfo::exists(path)) {
        QMessageBox::information(
            this,
            QStringLiteral("Carpeta no encontrada"),
            QStringLiteral("La tarea aun no tiene carpeta sincronizada."));
        return;
    }

    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
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
    refreshDashboard();
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
    refreshDashboard();
}

void MainWindow::onNavigationRequested(const QString &pageId)
{
    setCurrentPage(pageId);
}

void MainWindow::onToggleSidebar()
{
    m_sidebar->setCompact(!m_sidebar->isCompact());
}

void MainWindow::onDashboardOpenCourse(const QString &courseId)
{
    setCurrentPage(QStringLiteral("tasks"));
    appendLog(QStringLiteral("INFO  Curso seleccionado: %1").arg(courseId));
}

void MainWindow::onDashboardOpenFolder(const QString &courseId)
{
    const QJsonObject state = readSyncState();
    const QJsonObject courseObj = state.value(QStringLiteral("courses")).toObject().value(courseId).toObject();
    const QString path = courseObj.value(QStringLiteral("folderPath")).toString().trimmed();

    if (path.isEmpty() || !QFileInfo::exists(path)) {
        appendError(QStringLiteral("No hay carpeta de curso sincronizada para %1").arg(courseId));
        return;
    }

    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

void MainWindow::onDashboardSyncCourse(const QString &courseId)
{
    appendLog(QStringLiteral("INFO  Sync por curso (%1) aun no es granular; se ejecuta sync general.").arg(courseId));
    onSyncFolders();
}

void MainWindow::onDashboardDownloadAttachments(const QString &courseId)
{
    appendLog(QStringLiteral("INFO  Descarga de adjuntos por curso (%1) aun no es granular; se ejecuta descarga general.").arg(courseId));
    onDownloadAttachments();
}

void MainWindow::onDashboardOpenClassroom(const QString &courseId)
{
    QString url;
    for (const Course &course : m_currentCourses) {
        if (course.id == courseId) {
            url = course.alternateLink.trimmed();
            break;
        }
    }

    if (url.isEmpty()) {
        appendError(QStringLiteral("Curso sin enlace de Classroom: %1").arg(courseId));
        return;
    }

    QDesktopServices::openUrl(QUrl(url));
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

    refreshHistoryView();
    refreshDashboard();
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

    m_settingsView->setConnectedEmail(QString());
    m_settingsView->setTokenStatus(state);
}

void MainWindow::refreshPathUi()
{
    const QString basePath = m_syncManager->basePath();
    m_pathBar->setBasePath(basePath);
    m_settingsView->setBasePath(basePath);

    const QJsonObject state = readSyncState();
    const QString lastSyncIso = state.value(QStringLiteral("lastSync")).toString();
    m_pathBar->setLastSyncText(formatIsoDateTime(lastSyncIso));
}

void MainWindow::refreshStatusUi()
{
    m_statusBarWidget->setStatusText(m_runtimeStatus);
    m_statusBarWidget->setErrorCount(m_errorCount + m_attachmentErrors);
    m_statusBarWidget->setProgress(m_syncProgressCurrent, m_syncProgressTotal);
}

void MainWindow::refreshCourseTable()
{
    m_updatingCourseTable = true;
    m_coursesTable->setRowCount(m_currentCourses.size());

    for (int row = 0; row < m_currentCourses.size(); ++row) {
        const Course &course = m_currentCourses.at(row);

        auto *nameItem = new QTableWidgetItem(course.name);
        auto *idItem = new QTableWidgetItem(course.id);
        auto *semesterItem = new QTableWidgetItem(m_syncManager->semesterForCourse(course.id));

        nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
        idItem->setFlags(idItem->flags() & ~Qt::ItemIsEditable);

        m_coursesTable->setItem(row, 0, nameItem);
        m_coursesTable->setItem(row, 1, idItem);
        m_coursesTable->setItem(row, 2, semesterItem);
    }

    m_updatingCourseTable = false;
}

void MainWindow::refreshTasksView()
{
    const QJsonObject state = readSyncState();
    const QJsonObject coursesState = state.value(QStringLiteral("courses")).toObject();

    QHash<QString, QString> courseNameById;
    for (const Course &course : m_currentCourses) {
        courseNameById.insert(course.id, course.name);
    }

    QVector<TaskRowData> rows;
    rows.reserve(m_currentAssignments.size());

    for (const Assignment &assignment : m_currentAssignments) {
        TaskRowData row;
        row.courseId = assignment.courseId;
        row.assignmentId = assignment.id;
        row.courseName = courseNameById.value(assignment.courseId, assignment.courseId);
        row.title = Utils::effectiveAssignmentTitle(assignment);
        row.dueDate = assignment.dueDate.isValid() ? assignment.dueDate.toString(QStringLiteral("yyyy-MM-dd")) : QStringLiteral("Sin fecha");
        row.state = assignment.state;

        const QJsonObject courseObj = coursesState.value(assignment.courseId).toObject();
        const QJsonObject assignmentsObj = courseObj.value(QStringLiteral("assignments")).toObject();
        const QJsonObject assignmentObj = assignmentsObj.value(assignment.id).toObject();

        const QString metadataHash = assignmentObj.value(QStringLiteral("metadataHash")).toString().trimmed();
        row.folderPath = assignmentObj.value(QStringLiteral("folderPath")).toString().trimmed();
        row.metadataStatus = metadataHash.isEmpty() ? QStringLiteral("Pendiente") : QStringLiteral("OK");

        const QJsonObject attachmentsObj = assignmentObj.value(QStringLiteral("attachments")).toObject();
        const int attachmentCount = attachmentObjectCount(attachmentsObj);
        row.attachmentsStatus = attachmentCount > 0 ? QString::number(attachmentCount) : QStringLiteral("—");
        row.files = attachmentCount;

        const QString lastSeen = assignmentObj.value(QStringLiteral("lastSeen")).toString();
        const QString lastUpdated = assignmentObj.value(QStringLiteral("lastUpdated")).toString();
        row.lastSync = formatIsoDateTime(!lastUpdated.isEmpty() ? lastUpdated : lastSeen);

        rows.append(row);
    }

    m_tasksView->setRows(rows);
}

void MainWindow::refreshAttachmentsView()
{
    const QJsonObject state = readSyncState();
    const QJsonObject coursesState = state.value(QStringLiteral("courses")).toObject();

    QHash<QString, QString> courseNameById;
    for (const Course &course : m_currentCourses) {
        courseNameById.insert(course.id, course.name);
    }

    QHash<QString, QString> assignmentTitleByKey;
    for (const Assignment &assignment : m_currentAssignments) {
        assignmentTitleByKey.insert(assignment.courseId + QStringLiteral(":") + assignment.id, Utils::effectiveAssignmentTitle(assignment));
    }

    QVector<AttachmentRowData> rows;

    for (auto courseIt = coursesState.begin(); courseIt != coursesState.end(); ++courseIt) {
        const QString courseId = courseIt.key();
        const QJsonObject courseObj = courseIt.value().toObject();
        const QJsonObject assignmentsObj = courseObj.value(QStringLiteral("assignments")).toObject();

        for (auto assignmentIt = assignmentsObj.begin(); assignmentIt != assignmentsObj.end(); ++assignmentIt) {
            const QString assignmentId = assignmentIt.key();
            const QJsonObject assignmentObj = assignmentIt.value().toObject();
            const QJsonObject attachmentsObj = assignmentObj.value(QStringLiteral("attachments")).toObject();
            if (attachmentsObj.isEmpty()) {
                continue;
            }

            const QString assignmentKey = courseId + QStringLiteral(":") + assignmentId;
            const QString assignmentTitle = assignmentTitleByKey.value(
                assignmentKey,
                assignmentObj.value(QStringLiteral("title")).toString().trimmed());

            for (auto attachmentIt = attachmentsObj.begin(); attachmentIt != attachmentsObj.end(); ++attachmentIt) {
                const QJsonObject attachment = attachmentIt.value().toObject();
                if (attachment.isEmpty()) {
                    continue;
                }

                AttachmentRowData row;
                row.courseName = courseNameById.value(courseId, courseId);
                row.assignmentTitle = assignmentTitle.isEmpty() ? QStringLiteral("Tarea") : assignmentTitle;
                row.fileName = attachment.value(QStringLiteral("localFileName")).toString();
                if (row.fileName.isEmpty()) {
                    row.fileName = attachment.value(QStringLiteral("name")).toString();
                }
                if (row.fileName.isEmpty()) {
                    row.fileName = attachment.value(QStringLiteral("title")).toString();
                }

                row.type = attachment.value(QStringLiteral("type")).toString();
                if (row.type.isEmpty()) {
                    row.type = attachment.value(QStringLiteral("sourceMimeType")).toString();
                }
                row.status = attachment.value(QStringLiteral("status")).toString();
                row.localPath = attachment.value(QStringLiteral("localPath")).toString();

                if (attachment.contains(QStringLiteral("size"))) {
                    const qint64 size = attachment.value(QStringLiteral("size")).toVariant().toLongLong();
                    if (size > 0) {
                        row.size = QString::number(size);
                    }
                }
                if (row.size.isEmpty()) {
                    row.size = QStringLiteral("—");
                }

                rows.append(row);
            }
        }
    }

    m_attachmentsView->setRows(rows);
}

void MainWindow::refreshDashboard()
{
    const QJsonObject state = readSyncState();
    const QJsonObject coursesState = state.value(QStringLiteral("courses")).toObject();

    QHash<QString, QList<Assignment>> assignmentsByCourse;
    for (const Assignment &assignment : m_currentAssignments) {
        assignmentsByCourse[assignment.courseId].append(assignment);
    }

    int pendingTotal = 0;
    int attachmentsTotal = 0;

    QVector<CourseUiState> coursesUi;
    coursesUi.reserve(m_currentCourses.size());

    for (const Course &course : m_currentCourses) {
        CourseUiState ui;
        ui.id = course.id;
        ui.name = course.name;
        ui.code = course.section;
        ui.semester = m_syncManager->semesterForCourse(course.id);
        ui.classroomUrl = course.alternateLink;

        const QJsonObject courseObj = coursesState.value(course.id).toObject();
        ui.folderPath = courseObj.value(QStringLiteral("folderPath")).toString();

        const QList<Assignment> courseAssignments = assignmentsByCourse.value(course.id);
        ui.totalTasks = courseAssignments.size();

        const QJsonObject assignmentsObj = courseObj.value(QStringLiteral("assignments")).toObject();
        QDateTime newest;

        for (const Assignment &assignment : courseAssignments) {
            const QJsonObject assignmentObj = assignmentsObj.value(assignment.id).toObject();
            const QString assignmentFolder = assignmentObj.value(QStringLiteral("folderPath")).toString().trimmed();
            if (!assignmentFolder.isEmpty() && QFileInfo::exists(assignmentFolder)) {
                ++ui.backedUpTasks;
            }

            const QJsonObject attachmentsObj = assignmentObj.value(QStringLiteral("attachments")).toObject();
            const int courseAttachmentCount = attachmentObjectCount(attachmentsObj);
            ui.attachments += courseAttachmentCount;

            const QString seenIso = assignmentObj.value(QStringLiteral("lastSeen")).toString().trimmed();
            const QString updatedIso = assignmentObj.value(QStringLiteral("lastUpdated")).toString().trimmed();
            const QString dateIso = !updatedIso.isEmpty() ? updatedIso : seenIso;
            const QDateTime dt = QDateTime::fromString(dateIso, Qt::ISODate);
            if (dt.isValid() && (!newest.isValid() || dt > newest)) {
                newest = dt;
            }

            if (!assignmentFolder.isEmpty() && !QFileInfo::exists(assignmentFolder)) {
                ++ui.errors;
            }
        }

        ui.pending = qMax(0, ui.totalTasks - ui.backedUpTasks);
        ui.lastSync = newest.isValid() ? formatIsoDateTime(newest.toString(Qt::ISODate)) : QStringLiteral("—");
        ui.status = statusForCourse(ui);

        pendingTotal += ui.pending;
        attachmentsTotal += ui.attachments;

        coursesUi.append(ui);
    }

    QVector<KpiData> kpis;
    kpis.reserve(5);
    kpis.append(KpiData{QStringLiteral("Cursos"), m_coursesCount, QStringLiteral("◧"), QStringLiteral("idle")});
    kpis.append(KpiData{QStringLiteral("Tareas"), m_assignmentsCount, QStringLiteral("✓"), QStringLiteral("idle")});
    kpis.append(KpiData{QStringLiteral("Adjuntos"), attachmentsTotal, QStringLiteral("⬇"), QStringLiteral("complete")});
    kpis.append(KpiData{QStringLiteral("Pendientes"), pendingTotal, QStringLiteral("◔"), QStringLiteral("pending")});
    kpis.append(KpiData{QStringLiteral("Errores"), m_errorCount + m_attachmentErrors, QStringLiteral("!"), QStringLiteral("error")});

    QVector<ActivityItem> recent;
    const int recentCount = qMin(25, m_logLines.size());
    recent.reserve(recentCount);

    for (int i = m_logLines.size() - 1; i >= 0 && recent.size() < recentCount; --i) {
        const QString line = m_logLines.at(i);

        ActivityItem item;
        item.time = QStringLiteral("--:--:--");
        item.kind = QStringLiteral("INFO");
        item.message = line;

        if (line.startsWith(QLatin1Char('[')) && line.size() > 10) {
            item.time = line.mid(1, 8);
            QString payload = line.mid(11).trimmed();
            const int firstSpace = payload.indexOf(QLatin1Char(' '));
            if (firstSpace > 0) {
                item.kind = payload.left(firstSpace).trimmed();
                item.message = payload.mid(firstSpace).trimmed();
            } else {
                item.message = payload;
            }
        }

        recent.append(item);
    }

    m_dashboard->setKpis(kpis);
    m_dashboard->setCourses(coursesUi);
    m_dashboard->setActivity(recent);
    m_dashboard->setBasePath(m_syncManager->basePath());
    m_dashboard->setStorageSummary(QStringLiteral("No calculado"), attachmentsTotal);

    const QString lastSyncIso = state.value(QStringLiteral("lastSync")).toString();
    m_dashboard->setLastSyncText(QStringLiteral("Ultima sync · %1").arg(formatIsoDateTime(lastSyncIso)));
}

void MainWindow::refreshHistoryView()
{
    m_historyView->setLogLines(m_logLines);
}

void MainWindow::refreshAllViews()
{
    refreshAuthUi();
    refreshPathUi();
    refreshStatusUi();
    refreshCourseTable();
    refreshTasksView();
    refreshAttachmentsView();
    refreshHistoryView();
    refreshDashboard();

    m_settingsView->setAutoDownloadAttachments(m_syncManager->autoDownloadAttachments());
}
