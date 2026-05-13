#include "CourseDetailWidget.hpp"

#include "AssignmentListItemWidget.hpp"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <utility>

CourseDetailWidget::CourseDetailWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(12);

    auto *headerCard = new QFrame(this);
    headerCard->setObjectName(QStringLiteral("Section"));

    auto *headerLayout = new QVBoxLayout(headerCard);
    headerLayout->setContentsMargins(12, 12, 12, 12);
    headerLayout->setSpacing(8);

    auto *topRow = new QHBoxLayout();
    topRow->setSpacing(8);

    m_backButton = new QPushButton(QStringLiteral("<- Materias"), headerCard);
    m_backButton->setProperty("variant", QStringLiteral("ghost"));
    topRow->addWidget(m_backButton);

    m_titleLabel = new QLabel(QStringLiteral("Materia"), headerCard);
    m_titleLabel->setStyleSheet(QStringLiteral("font-size:18px;font-weight:700;"));
    topRow->addWidget(m_titleLabel, 1);

    m_statusLabel = new QLabel(QStringLiteral("Sin sync"), headerCard);
    m_statusLabel->setProperty("muted", true);
    topRow->addWidget(m_statusLabel);

    headerLayout->addLayout(topRow);

    m_semesterLabel = new QLabel(QStringLiteral("Semestre: Sin semestre"), headerCard);
    m_semesterLabel->setProperty("muted", true);
    headerLayout->addWidget(m_semesterLabel);

    m_summaryLabel = new QLabel(QStringLiteral("Tareas: 0"), headerCard);
    m_summaryLabel->setProperty("muted", true);
    headerLayout->addWidget(m_summaryLabel);

    m_pathLabel = new QLabel(QStringLiteral("Ruta: —"), headerCard);
    m_pathLabel->setProperty("subtle", true);
    m_pathLabel->setWordWrap(true);
    headerLayout->addWidget(m_pathLabel);

    auto *actions = new QHBoxLayout();
    actions->setSpacing(6);

    m_syncButton = new QPushButton(QStringLiteral("Sincronizar materia"), headerCard);
    m_syncButton->setProperty("variant", QStringLiteral("primary"));
    m_downloadButton = new QPushButton(QStringLiteral("Descargar adjuntos"), headerCard);
    m_openFolderButton = new QPushButton(QStringLiteral("Abrir carpeta"), headerCard);
    m_openClassroomButton = new QPushButton(QStringLiteral("Classroom"), headerCard);

    actions->addWidget(m_syncButton);
    actions->addWidget(m_downloadButton);
    actions->addWidget(m_openFolderButton);
    actions->addWidget(m_openClassroomButton);
    actions->addStretch(1);

    headerLayout->addLayout(actions);

    root->addWidget(headerCard);

    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);

    m_listContainer = new QWidget(m_scrollArea);
    m_listLayout = new QVBoxLayout(m_listContainer);
    m_listLayout->setContentsMargins(0, 0, 0, 0);
    m_listLayout->setSpacing(8);
    m_listLayout->addStretch(1);

    m_scrollArea->setWidget(m_listContainer);
    root->addWidget(m_scrollArea, 1);

    connect(m_backButton, &QPushButton::clicked, this, &CourseDetailWidget::backRequested);
    connect(m_syncButton, &QPushButton::clicked, this, [this]() {
        emit syncCourseRequested(m_course.id);
    });
    connect(m_downloadButton, &QPushButton::clicked, this, [this]() {
        emit downloadCourseAttachmentsRequested(m_course.id);
    });
    connect(m_openFolderButton, &QPushButton::clicked, this, [this]() {
        emit openCourseFolderRequested(m_course.id);
    });
    connect(m_openClassroomButton, &QPushButton::clicked, this, [this]() {
        emit openCourseClassroomRequested(m_course.id);
    });
}

void CourseDetailWidget::setCourse(const CourseUiState &course)
{
    m_course = course;

    m_titleLabel->setText(course.name.trimmed().isEmpty() ? QStringLiteral("Materia") : course.name.trimmed());
    m_semesterLabel->setText(QStringLiteral("Semestre: %1").arg(course.semester.trimmed().isEmpty() ? QStringLiteral("Sin semestre") : course.semester.trimmed()));
    m_statusLabel->setText(QStringLiteral("Estado: %1").arg(statusLabel(course.status)));
    m_summaryLabel->setText(
        QStringLiteral("Tareas %1/%2 · Adjuntos %3 · Pendientes %4 · Errores %5 · Ultima sync %6")
            .arg(course.backedUpTasks)
            .arg(course.totalTasks)
            .arg(course.attachments)
            .arg(course.pending)
            .arg(course.errors)
            .arg(course.lastSync.trimmed().isEmpty() ? QStringLiteral("—") : course.lastSync.trimmed()));
    m_pathLabel->setText(QStringLiteral("Ruta: %1").arg(course.folderPath.trimmed().isEmpty() ? QStringLiteral("—") : course.folderPath.trimmed()));

    m_openFolderButton->setEnabled(!course.folderPath.trimmed().isEmpty());
    m_openClassroomButton->setEnabled(!course.classroomUrl.trimmed().isEmpty());
}

void CourseDetailWidget::setAssignments(const QVector<AssignmentListItemData> &assignments)
{
    m_assignments = assignments;
    refreshAssignments();
}

void CourseDetailWidget::setSearchText(const QString &text)
{
    const QString normalized = text.trimmed().toLower();
    if (m_searchText == normalized) {
        return;
    }

    m_searchText = normalized;
    refreshAssignments();
}

void CourseDetailWidget::refreshAssignments()
{
    QLayoutItem *item = nullptr;
    while ((item = m_listLayout->takeAt(0)) != nullptr) {
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }

    int visibleCount = 0;
    for (const AssignmentListItemData &entry : std::as_const(m_assignments)) {
        if (!m_searchText.isEmpty()) {
            const QString haystack =
                (entry.title + QLatin1Char(' ') + entry.descriptionPreview + QLatin1Char(' ') + entry.dueDateText).toLower();
            if (!haystack.contains(m_searchText)) {
                continue;
            }
        }

        auto *itemWidget = new AssignmentListItemWidget(m_listContainer);
        itemWidget->setData(entry);

        connect(itemWidget, &AssignmentListItemWidget::selected, this, &CourseDetailWidget::assignmentSelected);
        connect(itemWidget, &AssignmentListItemWidget::openFolderRequested, this, &CourseDetailWidget::openAssignmentFolderRequested);
        connect(itemWidget, &AssignmentListItemWidget::openClassroomRequested, this, &CourseDetailWidget::openAssignmentClassroomRequested);
        connect(itemWidget, &AssignmentListItemWidget::downloadAttachmentsRequested, this, &CourseDetailWidget::downloadAssignmentAttachmentsRequested);

        m_listLayout->addWidget(itemWidget);
        ++visibleCount;
    }

    if (visibleCount == 0) {
        auto *empty = new QLabel(QStringLiteral("No hay tareas para este filtro."), m_listContainer);
        empty->setProperty("subtle", true);
        m_listLayout->addWidget(empty);
    }

    m_listLayout->addStretch(1);
}
