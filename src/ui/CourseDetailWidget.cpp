#include "CourseDetailWidget.hpp"

#include "AssignmentListItemWidget.hpp"
#include "PublicationListItemWidget.hpp"

#include <QButtonGroup>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QComboBox>
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
    m_titleLabel->setStyleSheet(QStringLiteral("font-size:18px;font-weight:700;background:transparent;border:none;"));
    topRow->addWidget(m_titleLabel, 1);

    m_statusLabel = new QLabel(QStringLiteral("Sin sync"), headerCard);
    m_statusLabel->setProperty("muted", true);
    topRow->addWidget(m_statusLabel);

    headerLayout->addLayout(topRow);

    m_semesterLabel = new QLabel(QStringLiteral("Semestre: Sin semestre"), headerCard);
    m_semesterLabel->setProperty("muted", true);
    headerLayout->addWidget(m_semesterLabel);

    auto *semesterRow = new QHBoxLayout();
    auto *semesterEditLabel = new QLabel(QStringLiteral("Semestre asignado:"), headerCard);
    semesterEditLabel->setProperty("subtle", true);
    semesterRow->addWidget(semesterEditLabel);
    m_semesterCombo = new QComboBox(headerCard);
    m_semesterCombo->addItem(QStringLiteral("Sin semestre"));
    m_semesterCombo->addItem(QStringLiteral("Semestre 1"));
    m_semesterCombo->addItem(QStringLiteral("Semestre 2"));
    m_semesterCombo->addItem(QStringLiteral("Semestre 3"));
    m_semesterCombo->addItem(QStringLiteral("Semestre 4"));
    m_semesterCombo->addItem(QStringLiteral("Semestre 5"));
    m_semesterCombo->addItem(QStringLiteral("Semestre 6"));
    semesterRow->addWidget(m_semesterCombo);
    semesterRow->addStretch(1);
    headerLayout->addLayout(semesterRow);

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
    m_openFolderButton = new QPushButton(QStringLiteral("Abrir carpeta"), headerCard);
    m_openClassroomButton = new QPushButton(QStringLiteral("Classroom"), headerCard);

    actions->addWidget(m_syncButton);
    actions->addWidget(m_openFolderButton);
    actions->addWidget(m_openClassroomButton);
    actions->addStretch(1);

    headerLayout->addLayout(actions);

    // Section toggle — centered segmented control
    auto *toggleContainer = new QFrame(headerCard);
    toggleContainer->setObjectName(QStringLiteral("SectionToggle"));
    auto *toggleLayout = new QHBoxLayout(toggleContainer);
    toggleLayout->setContentsMargins(3, 3, 3, 3);
    toggleLayout->setSpacing(2);

    m_tasksTabButton = new QPushButton(QStringLiteral("Tareas"), toggleContainer);
    m_tasksTabButton->setCheckable(true);
    m_tasksTabButton->setChecked(true);
    m_tasksTabButton->setProperty("sectionTab", QStringLiteral("left"));

    m_publicationsTabButton = new QPushButton(QStringLiteral("Trabajos y publicaciones"), toggleContainer);
    m_publicationsTabButton->setCheckable(true);
    m_publicationsTabButton->setChecked(false);
    m_publicationsTabButton->setProperty("sectionTab", QStringLiteral("right"));

    m_sectionGroup = new QButtonGroup(toggleContainer);
    m_sectionGroup->setExclusive(true);
    m_sectionGroup->addButton(m_tasksTabButton, 0);
    m_sectionGroup->addButton(m_publicationsTabButton, 1);

    toggleLayout->addWidget(m_tasksTabButton);
    toggleLayout->addWidget(m_publicationsTabButton);

    auto *sectionRow = new QHBoxLayout();
    sectionRow->addStretch(1);
    sectionRow->addWidget(toggleContainer);
    sectionRow->addStretch(1);
    headerLayout->addLayout(sectionRow);

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
    connect(m_openFolderButton, &QPushButton::clicked, this, [this]() {
        emit openCourseFolderRequested(m_course.id);
    });
    connect(m_openClassroomButton, &QPushButton::clicked, this, [this]() {
        emit openCourseClassroomRequested(m_course.id);
    });
    connect(m_semesterCombo, &QComboBox::currentTextChanged, this, [this](const QString &text) {
        if (m_course.id.trimmed().isEmpty()) {
            return;
        }
        emit semesterChanged(m_course.id, text.trimmed());
    });
    connect(m_sectionGroup, &QButtonGroup::idClicked, this, [this](int id) {
        m_currentSection = (id == 0) ? CourseSection::Tasks : CourseSection::Publications;
        updateSummaryLabel();
        refreshList();
    });
}

void CourseDetailWidget::setCourse(const CourseUiState &course)
{
    m_course = course;

    m_titleLabel->setText(course.name.trimmed().isEmpty() ? QStringLiteral("Materia") : course.name.trimmed());
    m_semesterLabel->setText(QStringLiteral("Semestre: %1").arg(course.semester.trimmed().isEmpty() ? QStringLiteral("Sin semestre") : course.semester.trimmed()));
    const QString targetSemester = course.semester.trimmed().isEmpty() ? QStringLiteral("Sin semestre") : course.semester.trimmed();
    const int semesterIndex = m_semesterCombo->findText(targetSemester);
    if (semesterIndex >= 0 && m_semesterCombo->currentIndex() != semesterIndex) {
        m_semesterCombo->blockSignals(true);
        m_semesterCombo->setCurrentIndex(semesterIndex);
        m_semesterCombo->blockSignals(false);
    }
    m_statusLabel->setText(QStringLiteral("Estado: %1").arg(statusLabel(course.status)));
    updateSummaryLabel();
    m_pathLabel->setText(QStringLiteral("Ruta: %1").arg(course.folderPath.trimmed().isEmpty() ? QStringLiteral("—") : course.folderPath.trimmed()));

    m_openFolderButton->setEnabled(!course.folderPath.trimmed().isEmpty());
    m_openClassroomButton->setEnabled(!course.classroomUrl.trimmed().isEmpty());
}

void CourseDetailWidget::setAssignments(const QVector<AssignmentListItemData> &assignments)
{
    m_assignments = assignments;
    if (m_currentSection == CourseSection::Tasks) {
        refreshList();
    }
    updateSummaryLabel();
}

void CourseDetailWidget::setPublications(const QVector<PublicationListItemData> &publications)
{
    m_publications = publications;
    if (m_currentSection == CourseSection::Publications) {
        refreshList();
    }
    updateSummaryLabel();
}

void CourseDetailWidget::setSearchText(const QString &text)
{
    const QString normalized = text.trimmed().toLower();
    if (m_searchText == normalized) {
        return;
    }

    m_searchText = normalized;
    refreshList();
}

void CourseDetailWidget::updateSummaryLabel()
{
    if (m_currentSection == CourseSection::Tasks) {
        m_summaryLabel->setText(
            QStringLiteral("Tareas %1/%2 · Adjuntos %3 · Pendientes %4 · Errores %5 · Ultima sync %6")
                .arg(m_course.backedUpTasks)
                .arg(m_course.totalTasks)
                .arg(m_course.attachments)
                .arg(m_course.pending)
                .arg(m_course.errors)
                .arg(m_course.lastSync.trimmed().isEmpty() ? QStringLiteral("—") : m_course.lastSync.trimmed()));
    } else {
        m_summaryLabel->setText(
            QStringLiteral("Publicaciones: %1 · Ultima sync %2")
                .arg(m_publications.size())
                .arg(m_course.lastSync.trimmed().isEmpty() ? QStringLiteral("—") : m_course.lastSync.trimmed()));
    }
}

void CourseDetailWidget::refreshList()
{
    QLayoutItem *item = nullptr;
    while ((item = m_listLayout->takeAt(0)) != nullptr) {
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }

    int visibleCount = 0;

    if (m_currentSection == CourseSection::Tasks) {
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

            m_listLayout->addWidget(itemWidget);
            ++visibleCount;
        }

        if (visibleCount == 0) {
            auto *empty = new QLabel(QStringLiteral("No hay tareas para este filtro."), m_listContainer);
            empty->setProperty("subtle", true);
            m_listLayout->addWidget(empty);
        }
    } else {
        for (const PublicationListItemData &entry : std::as_const(m_publications)) {
            if (!m_searchText.isEmpty()) {
                const QString haystack = (entry.title + QLatin1Char(' ') + entry.textPreview).toLower();
                if (!haystack.contains(m_searchText)) {
                    continue;
                }
            }

            auto *itemWidget = new PublicationListItemWidget(m_listContainer);
            itemWidget->setData(entry);

            connect(itemWidget, &PublicationListItemWidget::openFolderRequested,
                    this, &CourseDetailWidget::openPublicationFolderRequested);
            connect(itemWidget, &PublicationListItemWidget::openClassroomRequested,
                    this, &CourseDetailWidget::openPublicationClassroomRequested);
            connect(itemWidget, &PublicationListItemWidget::downloadAttachmentRequested,
                    this, &CourseDetailWidget::downloadPublicationAttachmentRequested);

            m_listLayout->addWidget(itemWidget);
            ++visibleCount;
        }

        if (visibleCount == 0) {
            auto *empty = new QLabel(QStringLiteral("No hay publicaciones para esta materia."), m_listContainer);
            empty->setProperty("subtle", true);
            m_listLayout->addWidget(empty);
        }
    }

    m_listLayout->addStretch(1);
}
