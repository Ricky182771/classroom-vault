#include "../Semester.hpp"
#include "CourseCardWidget.hpp"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>

CourseCardWidget::CourseCardWidget(QWidget *parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("CourseCard"));

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    m_banner = new QFrame(this);
    m_banner->setObjectName(QStringLiteral("CourseBanner"));
    m_banner->setMinimumHeight(56);
    m_banner->setStyleSheet(QStringLiteral("background: qlineargradient(x1:0,y1:0,x2:1,y2:1, stop:0 #3F4FB8, stop:1 #2C3478);")
                          + QStringLiteral("border-top-left-radius:11px;border-top-right-radius:11px;"));

    auto *bannerLayout = new QHBoxLayout(m_banner);
    bannerLayout->setContentsMargins(10, 8, 10, 8);
    bannerLayout->addStretch(1);

    m_archivedBadge = new QLabel(QStringLiteral("\U0001F512 Archivado"), m_banner);
    m_archivedBadge->setToolTip(QStringLiteral("Semestre archivado: solo lectura"));
    m_archivedBadge->setStyleSheet(
        QStringLiteral("padding:4px 8px;border-radius:8px;font-size:11px;color:#E6C26A;"
                       "background:rgba(230,194,106,0.20);border:1px solid rgba(255,255,255,0.12);"));
    m_archivedBadge->setVisible(false);
    bannerLayout->addWidget(m_archivedBadge);

    m_statusBadge = new QLabel(QStringLiteral("Sin sync"), m_banner);
    m_statusBadge->setObjectName(QStringLiteral("Section"));
    m_statusBadge->setStyleSheet(
        QStringLiteral("padding:4px 8px;border-radius:8px;font-size:11px;background:#30323A;border:1px solid #3A3D46;"));
    bannerLayout->addWidget(m_statusBadge);

    root->addWidget(m_banner);

    auto *content = new QFrame(this);
    auto *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(12, 10, 12, 12);
    contentLayout->setSpacing(6);

    m_nameLabel = new QLabel(QStringLiteral("Materia"), content);
    m_nameLabel->setWordWrap(true);
    m_nameLabel->setStyleSheet(QStringLiteral("font-weight:700;font-size:14px;background:transparent;border:none;"));
    contentLayout->addWidget(m_nameLabel);

    m_codeLabel = new QLabel(QStringLiteral("Codigo"), content);
    m_codeLabel->setProperty("muted", true);
    contentLayout->addWidget(m_codeLabel);

    m_semesterLabel = new QLabel(QStringLiteral("Semestre"), content);
    m_semesterLabel->setProperty("subtle", true);
    contentLayout->addWidget(m_semesterLabel);

    m_statsLabel = new QLabel(QStringLiteral("Tareas: 0 · Adjuntos: 0 · Pendientes: 0"), content);
    m_statsLabel->setProperty("muted", true);
    contentLayout->addWidget(m_statsLabel);

    m_progress = new QProgressBar(content);
    m_progress->setRange(0, 100);
    m_progress->setValue(0);
    m_progress->setFormat(QStringLiteral("0%"));
    contentLayout->addWidget(m_progress);

    m_lastSyncLabel = new QLabel(QStringLiteral("Ultima sync: —"), content);
    m_lastSyncLabel->setProperty("subtle", true);
    contentLayout->addWidget(m_lastSyncLabel);

    auto *actions = new QGridLayout();
    actions->setHorizontalSpacing(6);
    actions->setVerticalSpacing(6);

    m_openTasksButton = new QPushButton(QStringLiteral("Ver tareas"), content);
    m_openFolderButton = new QPushButton(QStringLiteral("Abrir carpeta"), content);
    m_syncButton = new QPushButton(QStringLiteral("Sincronizar"), content);
    m_classroomButton = new QPushButton(QStringLiteral("Classroom"), content);

    m_syncButton->setProperty("variant", QStringLiteral("primary"));

    actions->addWidget(m_openTasksButton, 0, 0);
    actions->addWidget(m_openFolderButton, 0, 1);
    actions->addWidget(m_syncButton, 1, 0);
    actions->addWidget(m_classroomButton, 1, 1);

    contentLayout->addLayout(actions);

    root->addWidget(content);

    connect(m_openTasksButton, &QPushButton::clicked, this, [this]() {
        emit openCourseRequested(m_course.id);
    });
    connect(m_openFolderButton, &QPushButton::clicked, this, [this]() {
        emit openFolderRequested(m_course.id);
    });
    connect(m_syncButton, &QPushButton::clicked, this, [this]() {
        emit syncCourseRequested(m_course.id);
    });
    connect(m_classroomButton, &QPushButton::clicked, this, [this]() {
        emit openClassroomRequested(m_course.id);
    });
}

void CourseCardWidget::setCourse(const CourseUiState &course)
{
    m_course = course;

    m_nameLabel->setText(course.name.isEmpty() ? QStringLiteral("Materia sin nombre") : course.name);
    m_codeLabel->setText(course.code.isEmpty() ? QStringLiteral("—") : course.code);
    m_semesterLabel->setText(QStringLiteral("Semestre: %1").arg(course.semester.isEmpty() ? Semester::none() : course.semester));
    QString stats = QStringLiteral("Tareas %1/%2 · Adjuntos %3 · Pendientes %4 · Errores %5")
                        .arg(course.backedUpTasks)
                        .arg(course.totalTasks)
                        .arg(course.attachments)
                        .arg(course.pending)
                        .arg(course.errors);
    if (course.missingLocal > 0) {
        stats += QStringLiteral(" · Sin localizar %1").arg(course.missingLocal);
    }
    m_statsLabel->setText(stats);

    const int total = course.totalTasks <= 0 ? 1 : course.totalTasks;
    int progress = static_cast<int>((100.0 * static_cast<double>(course.backedUpTasks)) / static_cast<double>(total));
    if (progress < 0) {
        progress = 0;
    }
    if (progress > 100) {
        progress = 100;
    }

    m_progress->setValue(progress);
    if (course.archived) {
        // Un semestre archivado no se sincroniza: el porcentaje no mide progreso,
        // asi que anunciar "0% respaldado" lo hacia parecer un respaldo fallido.
        m_progress->setFormat(QStringLiteral("Archivado · solo lectura"));
    } else if (course.missingLocal > 0 && course.errors == 0) {
        m_progress->setFormat(QStringLiteral("%1% · respaldo no encontrado en la ruta base actual").arg(progress));
    } else {
        m_progress->setFormat(QStringLiteral("%1% respaldado").arg(progress));
    }

    m_lastSyncLabel->setText(QStringLiteral("Ultima sync: %1").arg(course.lastSync.isEmpty() ? QStringLiteral("—") : course.lastSync));

    applyStatusUi(course.status);

    m_openFolderButton->setEnabled(!course.folderPath.trimmed().isEmpty());
    m_classroomButton->setEnabled(!course.classroomUrl.trimmed().isEmpty());

    // Semestre archivado: la materia sigue siendo navegable (ver tareas, abrir
    // carpeta, abrir Classroom) pero la recarga manual queda deshabilitada.
    m_archivedBadge->setVisible(course.archived);
    m_syncButton->setEnabled(!course.archived);
    m_syncButton->setToolTip(course.archived
        ? QStringLiteral("Semestre archivado: solo lectura")
        : QStringLiteral("Sincronizar esta materia con Classroom"));
    m_semesterLabel->setText(
        course.archived
            ? QStringLiteral("Semestre: %1 \u00b7 \U0001F512 Archivado (solo lectura)")
                  .arg(course.semester.isEmpty() ? Semester::none() : course.semester)
            : m_semesterLabel->text());
}

CourseUiState CourseCardWidget::course() const
{
    return m_course;
}

void CourseCardWidget::mousePressEvent(QMouseEvent *event)
{
    if (event && event->button() == Qt::LeftButton) {
        emit openCourseRequested(m_course.id);
    }
    QFrame::mousePressEvent(event);
}

void CourseCardWidget::applyStatusUi(const QString &status)
{
    QString badgeText = QStringLiteral("Sin sync");
    QString badgeColor = QStringLiteral("#9AA0AE");
    QString badgeBg = QStringLiteral("rgba(154,160,174,0.18)");
    QString bannerA = QStringLiteral("#4A5060");
    QString bannerB = QStringLiteral("#2F3340");

    if (status == QStringLiteral("complete")) {
        badgeText = QStringLiteral("Completo");
        badgeColor = QStringLiteral("#8FD19E");
        badgeBg = QStringLiteral("rgba(143,209,158,0.16)");
        bannerA = QStringLiteral("#3A7A55");
        bannerB = QStringLiteral("#235038");
    } else if (status == QStringLiteral("pending")) {
        badgeText = QStringLiteral("Pendiente");
        badgeColor = QStringLiteral("#E6C26A");
        badgeBg = QStringLiteral("rgba(230,194,106,0.16)");
        bannerA = QStringLiteral("#9A6E3A");
        bannerB = QStringLiteral("#5E4225");
    } else if (status == QStringLiteral("archived")) {
        badgeText = QStringLiteral("Archivado");
        badgeColor = QStringLiteral("#9AA0AE");
        badgeBg = QStringLiteral("rgba(154,160,174,0.18)");
        bannerA = QStringLiteral("#4A5060");
        bannerB = QStringLiteral("#2F3340");
    } else if (status == QStringLiteral("error")) {
        badgeText = QStringLiteral("Error");
        badgeColor = QStringLiteral("#E07C7C");
        badgeBg = QStringLiteral("rgba(224,124,124,0.18)");
        bannerA = QStringLiteral("#9A4A5C");
        bannerB = QStringLiteral("#5E2D38");
    }

    m_statusBadge->setText(badgeText);
    m_statusBadge->setStyleSheet(
        QStringLiteral("padding:4px 8px;border-radius:8px;font-size:11px;color:%1;background:%2;border:1px solid rgba(255,255,255,0.12);")
            .arg(badgeColor, badgeBg));

    m_banner->setStyleSheet(
        QStringLiteral("background: qlineargradient(x1:0,y1:0,x2:1,y2:1, stop:0 %1, stop:1 %2);"
                       "border-top-left-radius:11px;border-top-right-radius:11px;")
            .arg(bannerA, bannerB));
}
