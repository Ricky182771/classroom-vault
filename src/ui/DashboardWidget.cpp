#include "DashboardWidget.hpp"

#include "ActivityPanelWidget.hpp"
#include "CourseGridWidget.hpp"
#include "HeroStripWidget.hpp"
#include "KpiCardWidget.hpp"
#include "StorageCardWidget.hpp"

#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QVBoxLayout>

DashboardWidget::DashboardWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);

    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    root->addWidget(scroll);

    auto *content = new QWidget(scroll);
    auto *contentLayout = new QHBoxLayout(content);
    contentLayout->setContentsMargins(16, 16, 16, 16);
    contentLayout->setSpacing(16);
    scroll->setWidget(content);

    auto *leftCol = new QVBoxLayout();
    leftCol->setSpacing(14);

    m_hero = new HeroStripWidget(content);
    leftCol->addWidget(m_hero);

    auto *kpiRow = new QHBoxLayout();
    kpiRow->setSpacing(10);
    for (int i = 0; i < 5; ++i) {
        auto *card = new KpiCardWidget(content);
        m_kpiCards.append(card);
        kpiRow->addWidget(card, 1);
    }
    leftCol->addLayout(kpiRow);

    auto *courseSection = new QFrame(content);
    courseSection->setObjectName(QStringLiteral("Section"));
    auto *courseLayout = new QVBoxLayout(courseSection);
    courseLayout->setContentsMargins(12, 12, 12, 12);
    courseLayout->setSpacing(10);

    auto *titleRow = new QHBoxLayout();
    auto *title = new QLabel(QStringLiteral("Cursos respaldados"), courseSection);
    title->setStyleSheet(QStringLiteral("font-weight:700;font-size:15px;"));
    titleRow->addWidget(title);

    m_coursesCountLabel = new QLabel(QStringLiteral("0"), courseSection);
    m_coursesCountLabel->setObjectName(QStringLiteral("Section"));
    m_coursesCountLabel->setStyleSheet(QStringLiteral("padding:2px 8px;border-radius:6px;"));
    titleRow->addWidget(m_coursesCountLabel);

    auto *subtitle = new QLabel(
        QStringLiteral("Inspeccionar, sincronizar y abrir cada materia en disco"),
        courseSection);
    subtitle->setProperty("subtle", true);
    titleRow->addWidget(subtitle, 1);

    courseLayout->addLayout(titleRow);

    auto *filterRow = new QHBoxLayout();
    filterRow->setSpacing(8);

    m_filterCombo = new QComboBox(courseSection);
    m_filterCombo->addItem(QStringLiteral("Todos"), QStringLiteral("all"));
    m_filterCombo->addItem(QStringLiteral("Completo"), QStringLiteral("complete"));
    m_filterCombo->addItem(QStringLiteral("Pendiente"), QStringLiteral("pending"));
    m_filterCombo->addItem(QStringLiteral("Error"), QStringLiteral("error"));
    m_filterCombo->addItem(QStringLiteral("Sin sync"), QStringLiteral("idle"));
    filterRow->addWidget(m_filterCombo);

    m_orderCombo = new QComboBox(courseSection);
    m_orderCombo->addItems({
        QStringLiteral("Ultima sync"),
        QStringLiteral("Nombre"),
        QStringLiteral("Semestre"),
        QStringLiteral("Pendientes"),
    });
    filterRow->addWidget(m_orderCombo);
    filterRow->addStretch(1);

    courseLayout->addLayout(filterRow);

    m_courseGrid = new CourseGridWidget(courseSection);
    courseLayout->addWidget(m_courseGrid);

    leftCol->addWidget(courseSection, 1);

    contentLayout->addLayout(leftCol, 1);

    auto *rightCol = new QVBoxLayout();
    rightCol->setSpacing(12);

    m_activityPanel = new ActivityPanelWidget(content);
    rightCol->addWidget(m_activityPanel, 1);

    m_storageCard = new StorageCardWidget(content);
    rightCol->addWidget(m_storageCard);

    contentLayout->addLayout(rightCol);
    rightCol->setAlignment(Qt::AlignTop);

    connect(m_hero, &HeroStripWidget::syncRequested, this, &DashboardWidget::syncRequested);
    connect(m_filterCombo, &QComboBox::currentIndexChanged, this, [this](int idx) {
        const QString filterId = m_filterCombo->itemData(idx).toString();
        m_courseGrid->setFilter(filterId);
    });

    connect(m_courseGrid, &CourseGridWidget::openCourseRequested, this, &DashboardWidget::openCourseRequested);
    connect(m_courseGrid, &CourseGridWidget::openFolderRequested, this, &DashboardWidget::openFolderRequested);
    connect(m_courseGrid, &CourseGridWidget::syncCourseRequested, this, &DashboardWidget::syncCourseRequested);
    connect(m_courseGrid, &CourseGridWidget::downloadAttachmentsRequested, this, &DashboardWidget::downloadAttachmentsRequested);
    connect(m_courseGrid, &CourseGridWidget::openClassroomRequested, this, &DashboardWidget::openClassroomRequested);

    connect(m_activityPanel, &ActivityPanelWidget::showHistoryRequested, this, &DashboardWidget::showHistoryRequested);
    connect(m_storageCard, &StorageCardWidget::openBasePathRequested, this, &DashboardWidget::openBasePathRequested);
}

void DashboardWidget::setKpis(const QVector<KpiData> &kpis)
{
    for (int i = 0; i < m_kpiCards.size(); ++i) {
        if (i < kpis.size()) {
            m_kpiCards.at(i)->setData(kpis.at(i));
        } else {
            m_kpiCards.at(i)->setData(KpiData{});
        }
    }
}

void DashboardWidget::setCourses(const QVector<CourseUiState> &courses)
{
    m_courses = courses;
    m_coursesCountLabel->setText(QString::number(m_courses.size()));
    m_courseGrid->setCourses(m_courses);
}

void DashboardWidget::setActivity(const QVector<ActivityItem> &items)
{
    m_activityPanel->setItems(items);
}

void DashboardWidget::setBasePath(const QString &basePath)
{
    m_storageCard->setBasePath(basePath);
}

void DashboardWidget::setStorageSummary(const QString &usedText, int attachments)
{
    m_storageCard->setUsedText(usedText);
    m_storageCard->setAttachmentsCount(attachments);
}

void DashboardWidget::setLastSyncText(const QString &text)
{
    m_hero->setApiStatusText(text);
}
