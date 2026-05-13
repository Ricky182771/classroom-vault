#include "MainWindow.hpp"

#include "GoogleAuth.hpp"
#include "SyncManager.hpp"
#include "Utils.hpp"

#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QStatusBar>
#include <QTableWidget>
#include <QTextEdit>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>

namespace {
constexpr int CourseIdRole = Qt::UserRole + 1;
constexpr int AssignmentIdRole = Qt::UserRole + 2;
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_syncManager(new SyncManager(this))
{
    setupUi();
    connectSignals();

    m_basePathEdit->setText(m_syncManager->basePath());

    appendLog(QStringLiteral("INFO  Listo. Carga datos de prueba o inicia sesion para Classroom real."));
    appendLog(QStringLiteral("INFO  Config local: %1").arg(m_syncManager->configManager().configDir()));
}

void MainWindow::setupUi()
{
    setWindowTitle(QStringLiteral("Classroom Vault / TareaSync"));
    resize(1200, 820);

    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);

    auto *pathRow = new QHBoxLayout();
    auto *pathLabel = new QLabel(QStringLiteral("Ruta base:"), central);
    m_basePathEdit = new QLineEdit(central);
    m_browseButton = new QPushButton(QStringLiteral("Seleccionar"), central);
    m_openBaseFolderButton = new QPushButton(QStringLiteral("Abrir carpeta base"), central);

    pathRow->addWidget(pathLabel);
    pathRow->addWidget(m_basePathEdit, 1);
    pathRow->addWidget(m_browseButton);
    pathRow->addWidget(m_openBaseFolderButton);

    auto *buttonRow = new QHBoxLayout();
    m_loginButton = new QPushButton(QStringLiteral("Iniciar sesion"), central);
    m_loadSampleButton = new QPushButton(QStringLiteral("Cargar datos de prueba"), central);
    m_loadClassroomButton = new QPushButton(QStringLiteral("Cargar Classroom"), central);
    m_syncButton = new QPushButton(QStringLiteral("Sincronizar carpetas"), central);
    m_clearLogsButton = new QPushButton(QStringLiteral("Limpiar logs"), central);

    buttonRow->addWidget(m_loginButton);
    buttonRow->addWidget(m_loadSampleButton);
    buttonRow->addWidget(m_loadClassroomButton);
    buttonRow->addWidget(m_syncButton);
    buttonRow->addWidget(m_clearLogsButton);
    buttonRow->addStretch(1);

    auto *counterRow = new QHBoxLayout();
    m_coursesCountLabel = new QLabel(QStringLiteral("Cursos: 0"), central);
    m_assignmentsCountLabel = new QLabel(QStringLiteral("Tareas: 0"), central);
    m_newCountLabel = new QLabel(QStringLiteral("Nuevas: 0"), central);
    m_updatedCountLabel = new QLabel(QStringLiteral("Actualizadas: 0"), central);
    m_unchangedCountLabel = new QLabel(QStringLiteral("Sin cambios: 0"), central);
    m_errorCountLabel = new QLabel(QStringLiteral("Errores: 0"), central);

    counterRow->addWidget(m_coursesCountLabel);
    counterRow->addWidget(m_assignmentsCountLabel);
    counterRow->addWidget(m_newCountLabel);
    counterRow->addWidget(m_updatedCountLabel);
    counterRow->addWidget(m_unchangedCountLabel);
    counterRow->addWidget(m_errorCountLabel);
    counterRow->addStretch(1);

    m_progressBar = new QProgressBar(central);
    m_progressBar->setRange(0, 1);
    m_progressBar->setValue(0);
    m_progressBar->setFormat(QStringLiteral("Sin progreso"));

    auto *coursesLabel = new QLabel(QStringLiteral("Cursos (edita columna Semestre asignado)"), central);
    m_coursesTable = new QTableWidget(central);
    m_coursesTable->setColumnCount(3);
    m_coursesTable->setHorizontalHeaderLabels(
        {QStringLiteral("Materia"), QStringLiteral("ID"), QStringLiteral("Semestre asignado")});
    m_coursesTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_coursesTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_coursesTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_coursesTable->setSelectionBehavior(QAbstractItemView::SelectRows);

    auto *assignmentsLabel = new QLabel(QStringLiteral("Tareas encontradas (doble clic para abrir carpeta)"), central);
    m_assignmentsTable = new QTableWidget(central);
    m_assignmentsTable->setColumnCount(4);
    m_assignmentsTable->setHorizontalHeaderLabels(
        {QStringLiteral("Materia"), QStringLiteral("Tarea"), QStringLiteral("Fecha de entrega"), QStringLiteral("Estado")});
    m_assignmentsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_assignmentsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_assignmentsTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_assignmentsTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_assignmentsTable->setSelectionBehavior(QAbstractItemView::SelectRows);

    auto *logsLabel = new QLabel(QStringLiteral("Logs"), central);
    m_logText = new QTextEdit(central);
    m_logText->setReadOnly(true);

    layout->addLayout(pathRow);
    layout->addLayout(buttonRow);
    layout->addLayout(counterRow);
    layout->addWidget(m_progressBar);
    layout->addWidget(coursesLabel);
    layout->addWidget(m_coursesTable, 2);
    layout->addWidget(assignmentsLabel);
    layout->addWidget(m_assignmentsTable, 2);
    layout->addWidget(logsLabel);
    layout->addWidget(m_logText, 1);

    setCentralWidget(central);
}

void MainWindow::connectSignals()
{
    connect(m_browseButton, &QPushButton::clicked, this, &MainWindow::onBrowseBasePath);
    connect(m_openBaseFolderButton, &QPushButton::clicked, this, &MainWindow::onOpenBaseFolder);
    connect(m_clearLogsButton, &QPushButton::clicked, this, &MainWindow::onClearLogs);
    connect(m_loginButton, &QPushButton::clicked, this, &MainWindow::onLogin);
    connect(m_loadSampleButton, &QPushButton::clicked, this, &MainWindow::onLoadSampleData);
    connect(m_loadClassroomButton, &QPushButton::clicked, this, &MainWindow::onLoadClassroom);
    connect(m_syncButton, &QPushButton::clicked, this, &MainWindow::onSyncFolders);

    connect(m_coursesTable, &QTableWidget::itemChanged, this, &MainWindow::onCourseTableItemChanged);
    connect(m_assignmentsTable, &QTableWidget::cellDoubleClicked, this, &MainWindow::onAssignmentDoubleClicked);

    connect(m_syncManager, &SyncManager::coursesChanged, this, &MainWindow::onCoursesChanged);
    connect(m_syncManager, &SyncManager::assignmentsChanged, this, &MainWindow::onAssignmentsChanged);
    connect(m_syncManager, &SyncManager::syncProgress, this, &MainWindow::onSyncProgress);
    connect(m_syncManager, &SyncManager::syncFinished, this, &MainWindow::onSyncFinished);
    connect(m_syncManager, &SyncManager::countersChanged, this, &MainWindow::onCountersChanged);
    connect(m_syncManager, &SyncManager::logMessage, this, &MainWindow::appendLog);
    connect(m_syncManager, &SyncManager::errorOccurred, this, &MainWindow::appendError);
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

void MainWindow::onBrowseBasePath()
{
    const QString selected = QFileDialog::getExistingDirectory(this, QStringLiteral("Selecciona ruta base"), m_basePathEdit->text());
    if (selected.isEmpty()) {
        return;
    }

    m_basePathEdit->setText(selected);
    m_syncManager->setBasePath(selected);
    appendLog(QStringLiteral("INFO  Ruta base guardada: %1").arg(selected));
}

void MainWindow::onOpenBaseFolder()
{
    const QString basePath = m_basePathEdit->text().trimmed();
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
    m_logText->clear();
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

    auth->startAuthorization();

    bool ok = false;
    const QString code = QInputDialog::getText(
        this,
        QStringLiteral("Codigo de autorizacion"),
        QStringLiteral("Pega aqui el authorization code de Google:"),
        QLineEdit::Normal,
        QString(),
        &ok);

    if (!ok || code.trimmed().isEmpty()) {
        appendLog(QStringLiteral("INFO  Autorizacion cancelada por el usuario."));
        return;
    }

    auth->exchangeAuthorizationCode(code.trimmed());
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
    const QString basePath = m_basePathEdit->text().trimmed();
    if (basePath.isEmpty()) {
        appendError(QStringLiteral("Ruta base vacia. Selecciona una carpeta primero."));
        return;
    }

    m_progressBar->setRange(0, 1);
    m_progressBar->setValue(0);
    m_progressBar->setFormat(QStringLiteral("Preparando sincronizacion..."));

    m_syncManager->setBasePath(basePath);

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
    m_syncManager->syncFolders();
}

void MainWindow::onCoursesChanged(const QList<Course> &courses)
{
    m_currentCourses = courses;

    m_updatingCourseTable = true;
    m_coursesTable->setRowCount(courses.size());

    for (int row = 0; row < courses.size(); ++row) {
        const Course &course = courses.at(row);

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

void MainWindow::onAssignmentsChanged(const QList<Assignment> &assignments)
{
    QHash<QString, QString> courseNameById;
    for (const Course &course : m_currentCourses) {
        courseNameById.insert(course.id, course.name);
    }

    m_assignmentsTable->setRowCount(assignments.size());

    for (int row = 0; row < assignments.size(); ++row) {
        const Assignment &assignment = assignments.at(row);
        const QString courseName = courseNameById.value(assignment.courseId, assignment.courseId);
        const QString dueDateText = assignment.dueDate.isValid() ? assignment.dueDate.toString(QStringLiteral("yyyy-MM-dd")) : QStringLiteral("Sin fecha");

        auto *courseItem = new QTableWidgetItem(courseName);
        auto *titleItem = new QTableWidgetItem(Utils::effectiveAssignmentTitle(assignment));
        auto *dueDateItem = new QTableWidgetItem(dueDateText);
        auto *stateItem = new QTableWidgetItem(assignment.state);

        courseItem->setData(CourseIdRole, assignment.courseId);
        courseItem->setData(AssignmentIdRole, assignment.id);

        courseItem->setFlags(courseItem->flags() & ~Qt::ItemIsEditable);
        titleItem->setFlags(titleItem->flags() & ~Qt::ItemIsEditable);
        dueDateItem->setFlags(dueDateItem->flags() & ~Qt::ItemIsEditable);
        stateItem->setFlags(stateItem->flags() & ~Qt::ItemIsEditable);

        m_assignmentsTable->setItem(row, 0, courseItem);
        m_assignmentsTable->setItem(row, 1, titleItem);
        m_assignmentsTable->setItem(row, 2, dueDateItem);
        m_assignmentsTable->setItem(row, 3, stateItem);
    }
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
}

void MainWindow::onAssignmentDoubleClicked(int row, int column)
{
    Q_UNUSED(column)

    auto *item = m_assignmentsTable->item(row, 0);
    if (!item) {
        return;
    }

    const QString courseId = item->data(CourseIdRole).toString();
    const QString assignmentId = item->data(AssignmentIdRole).toString();

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
    if (total <= 0) {
        m_progressBar->setRange(0, 1);
        m_progressBar->setValue(0);
        m_progressBar->setFormat(QStringLiteral("Sin tareas"));
        statusBar()->showMessage(QStringLiteral("Sin tareas para sincronizar"));
        return;
    }

    m_progressBar->setRange(0, total);
    m_progressBar->setValue(current);
    m_progressBar->setFormat(QStringLiteral("%v/%m"));

    statusBar()->showMessage(QStringLiteral("Sincronizando %1/%2...").arg(current).arg(total));
}

void MainWindow::onSyncFinished(int newCount, int updatedCount, int unchangedCount, int errorCount)
{
    statusBar()->showMessage(QStringLiteral("Sincronizacion completada."), 6000);
    m_progressBar->setFormat(QStringLiteral("Completado"));

    appendLog(
        QStringLiteral("INFO  Resultado sync -> nuevas: %1, actualizadas: %2, sin cambios: %3, errores: %4")
            .arg(newCount)
            .arg(updatedCount)
            .arg(unchangedCount)
            .arg(errorCount));
}

void MainWindow::onCountersChanged(
    int courses,
    int assignments,
    int newCount,
    int updatedCount,
    int unchangedCount,
    int errorCount)
{
    m_coursesCountLabel->setText(QStringLiteral("Cursos: %1").arg(courses));
    m_assignmentsCountLabel->setText(QStringLiteral("Tareas: %1").arg(assignments));
    m_newCountLabel->setText(QStringLiteral("Nuevas: %1").arg(newCount));
    m_updatedCountLabel->setText(QStringLiteral("Actualizadas: %1").arg(updatedCount));
    m_unchangedCountLabel->setText(QStringLiteral("Sin cambios: %1").arg(unchangedCount));
    m_errorCountLabel->setText(QStringLiteral("Errores: %1").arg(errorCount));
}

void MainWindow::appendLog(const QString &message)
{
    const QString line = QStringLiteral("[%1] %2")
                             .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")), message);
    m_logText->append(line);
}

void MainWindow::appendError(const QString &message)
{
    QString normalized = message.trimmed();
    if (!normalized.startsWith(QStringLiteral("ERR"))) {
        normalized = QStringLiteral("ERR   ") + normalized;
    }
    appendLog(normalized);
}
