#include "CourseGridWidget.hpp"

#include "CourseCardWidget.hpp"

#include <QGridLayout>
#include <QResizeEvent>
#include <QTimer>
#include <QVBoxLayout>

CourseGridWidget::CourseGridWidget(QWidget *parent)
    : QWidget(parent)
    , m_gridContainer(new QWidget(this))
    , m_grid(new QGridLayout(m_gridContainer))
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    m_grid->setContentsMargins(0, 0, 0, 0);
    m_grid->setHorizontalSpacing(14);
    m_grid->setVerticalSpacing(14);

    root->addWidget(m_gridContainer);
}

void CourseGridWidget::setCourses(const QVector<CourseUiState> &courses)
{
    m_courses = courses;
    scheduleRefresh();
}

void CourseGridWidget::setFilter(const QString &filter)
{
    const QString normalized = filter.trimmed().toLower();
    if (m_filter == normalized) {
        return;
    }
    m_filter = normalized;
    scheduleRefresh();
}

void CourseGridWidget::scheduleRefresh()
{
    if (m_refreshScheduled) {
        return;
    }

    m_refreshScheduled = true;
    QTimer::singleShot(0, this, [this]() {
        m_refreshScheduled = false;
        refreshGrid();
    });
}

QVector<CourseUiState> CourseGridWidget::filteredCourses() const
{
    if (m_filter.isEmpty() || m_filter == QStringLiteral("all")) {
        return m_courses;
    }

    QVector<CourseUiState> result;
    for (const CourseUiState &course : m_courses) {
        if (course.status.toLower() == m_filter) {
            result.append(course);
        }
    }
    return result;
}

void CourseGridWidget::refreshGrid()
{
    QLayoutItem *item;
    while ((item = m_grid->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }

    const QVector<CourseUiState> list = filteredCourses();
    const int available = qMax(320, width());
    const int columns = qMax(1, available / 300);

    int row = 0;
    int col = 0;

    for (const CourseUiState &course : list) {
        auto *card = new CourseCardWidget(m_gridContainer);
        card->setCourse(course);

        connect(card, &CourseCardWidget::openCourseRequested, this, &CourseGridWidget::openCourseRequested);
        connect(card, &CourseCardWidget::openFolderRequested, this, &CourseGridWidget::openFolderRequested);
        connect(card, &CourseCardWidget::syncCourseRequested, this, &CourseGridWidget::syncCourseRequested);
        connect(card, &CourseCardWidget::downloadAttachmentsRequested, this, &CourseGridWidget::downloadAttachmentsRequested);
        connect(card, &CourseCardWidget::openClassroomRequested, this, &CourseGridWidget::openClassroomRequested);

        m_grid->addWidget(card, row, col);
        ++col;
        if (col >= columns) {
            col = 0;
            ++row;
        }
    }

    if (col != 0) {
        ++row;
    }

    m_grid->setRowStretch(row, 1);
}

void CourseGridWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    scheduleRefresh();
}
