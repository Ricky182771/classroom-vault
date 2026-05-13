#include "MainWindow.hpp"

#include "GoogleAuth.hpp"
#include "SyncManager.hpp"
#include "Utils.hpp"

#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QCoreApplication>
#include <QStatusBar>
#include <QTableWidget>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QWidget>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_syncManager(new SyncManager(this))
{
    setupUi();
    connectSignals();

    m_basePathEdit->setText(m_syncManager->basePath());

    appendLog(QStringLiteral("Listo. Carga datos de prueba o inicia sesion para Classroom real."));
    appendLog(QStringLiteral("Config local: %1").arg(m_syncManager->configManager().configDir()));
}

void MainWindow::setupUi()
{
    setWindowTitle(QStringLiteral("Classroom Vault / TareaSync"));
    resize(1100, 760);

    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);

    auto *pathRow = new QHBoxLayout();
    auto *pathLabel = new QLabel(QStringLiteral("Ruta base:"), central);
    m_basePathEdit = new QLineEdit(central);
    m_browseButton = new QPushButton(QStringLiteral("Seleccionar"), central);

    pathRow->addWidget(pathLabel);
    pathRow->addWidget(m_basePathEdit, 1);
    pathRow->addWidget(m_browseButton);

    auto *buttonRow = new QHBoxLayout();
    m_loginButton = new QPushButton(QStringLiteral("Iniciar sesion"), central);
    m_loadSampleButton = new QPushButton(QStringLiteral("Cargar datos de prueba"), central);
    m_loadClassroomButton = new QPushButton(QStringLiteral("Cargar Classroom"), central);
    m_syncButton = new QPushButton(QStringLiteral("Sincronizar carpetas"), central);

    buttonRow->addWidget(m_loginButton);
    buttonRow->addWidget(m_loadSampleButton);
    buttonRow->addWidget(m_loadClassroomButton);
    buttonRow->addWidget(m_syncButton);
    buttonRow->addStretch(1);

    auto *coursesLabel = new QLabel(QStringLiteral("Cursos (edita columna Semestre asignado)"), central);
    m_coursesTable = new QTableWidget(central);
    m_coursesTable->setColumnCount(3);
    m_coursesTable->setHorizontalHeaderLabels(
        {QStringLiteral("Materia"), QStringLiteral("ID"), QStringLiteral("Semestre asignado")});
    m_coursesTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_coursesTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_coursesTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_coursesTable->setSelectionBehavior(QAbstractItemView::SelectRows);

    auto *assignmentsLabel = new QLabel(QStringLiteral("Tareas encontradas"), central);
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
    connect(m_loginButton, &QPushButton::clicked, this, &MainWindow::onLogin);
    connect(m_loadSampleButton, &QPushButton::clicked, this, &MainWindow::onLoadSampleData);
    connect(m_loadClassroomButton, &QPushButton::clicked, this, &MainWindow::onLoadClassroom);
    connect(m_syncButton, &QPushButton::clicked, this, &MainWindow::onSyncFolders);

    connect(m_coursesTable, &QTableWidget::itemChanged, this, &MainWindow::onCourseTableItemChanged);

    connect(m_syncManager, &SyncManager::coursesChanged, this, &MainWindow::onCoursesChanged);
    connect(m_syncManager, &SyncManager::assignmentsChanged, this, &MainWindow::onAssignmentsChanged);
    connect(m_syncManager, &SyncManager::syncProgress, this, &MainWindow::onSyncProgress);
    connect(m_syncManager, &SyncManager::syncFinished, this, &MainWindow::onSyncFinished);
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
    appendLog(QStringLiteral("Ruta base guardada: %1").arg(selected));
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
        appendLog(QStringLiteral("Autorizacion cancelada por el usuario."));
        return;
    }

    auth->exchangeAuthorizationCode(code.trimmed());
}

void MainWindow::onLoadSampleData()
{
    const QString samplePath = resolveSampleDataPath();
    if (samplePath.isEmpty()) {
        appendLog(QStringLiteral("No se encontro sample_classroom_data.json local; se intentara recurso embebido."));
    } else {
        appendLog(QStringLiteral("Cargando sample desde: %1").arg(samplePath));
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

void MainWindow::onSyncProgress(int current, int total)
{
    statusBar()->showMessage(QStringLiteral("Sincronizando %1/%2...").arg(current).arg(total));
}

void MainWindow::onSyncFinished(int changedCount, int unchangedCount)
{
    statusBar()->showMessage(QStringLiteral("Sincronizacion completada."), 6000);
    appendLog(
        QStringLiteral("Resultado sync -> actualizados: %1, sin cambios: %2")
            .arg(changedCount)
            .arg(unchangedCount));
}

void MainWindow::appendLog(const QString &message)
{
    const QString line = QStringLiteral("[%1] %2")
                             .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")), message);
    m_logText->append(line);
}

void MainWindow::appendError(const QString &message)
{
    appendLog(QStringLiteral("ERROR: %1").arg(message));
}
