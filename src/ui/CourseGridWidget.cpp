#include "../Semester.hpp"

#include <QLabel>
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
    scheduleRefresh(true);
}

void CourseGridWidget::setFilter(const QString &filter)
{
    const QString normalized = filter.trimmed().toLower();
    if (m_filter == normalized) {
        return;
    }
    m_filter = normalized;
    scheduleRefresh(true);
}

void CourseGridWidget::setSearchText(const QString &text)
{
    const QString normalized = text.trimmed().toLower();
    if (m_searchText == normalized) {
        return;
    }

    m_searchText = normalized;
    scheduleRefresh(true);
}

int CourseGridWidget::columnsForWidth(int width) const
{
    constexpr int preferredCardWidth = 340;
    const int available = qMax(preferredCardWidth, width);
    const int computed = qMax(1, available / preferredCardWidth);
    return qMin(4, computed);
}

void CourseGridWidget::scheduleRefresh(bool force)
{
    if (force) {
        m_forceRefreshPending = true;
    }

    if (m_refreshScheduled) {
        return;
    }

    m_refreshScheduled = true;
    QTimer::singleShot(0, this, [this]() {
        m_refreshScheduled = false;
        const bool forceNow = m_forceRefreshPending;
        m_forceRefreshPending = false;
        refreshGrid(forceNow);
    });
}

QVector<CourseUiState> CourseGridWidget::filteredCourses() const
{
    // El filtro "all" solo desactiva la comparacion de estado. Antes hacia un
    // return temprano que se saltaba tambien la busqueda por texto: escribir en
    // el buscador con el filtro en "Todos" no filtraba nada.
    const bool filterByStatus = !m_filter.isEmpty() && m_filter != QStringLiteral("all");

    QVector<CourseUiState> result;
    for (const CourseUiState &course : m_courses) {
        if (filterByStatus && course.status.toLower() != m_filter) {
            continue;
        }

        if (!m_searchText.isEmpty()) {
            const QString haystack =
                (course.name + QLatin1Char(' ') + course.semester + QLatin1Char(' ') + course.code).toLower();
            if (!haystack.contains(m_searchText)) {
                continue;
            }
        }

        result.append(course);
    }
    return result;
}

void CourseGridWidget::setSemesterContext(const QString &semester)
{
    const QString clean = semester.trimmed();
    if (clean == m_semesterContext) {
        return;
    }
    m_semesterContext = clean;
    scheduleRefresh(true);
}

QString CourseGridWidget::emptyStateMessage() const
{
    if (!m_courses.isEmpty()) {
        return QStringLiteral("Ninguna materia coincide con el filtro o la busqueda actual.");
    }

    if (m_semesterContext.isEmpty() || m_semesterContext == Semester::all()) {
        return QStringLiteral("Todavia no hay materias respaldadas. Inicia sesion y sincroniza con Classroom.");
    }

    if (m_semesterContext == Semester::none()) {
        return QStringLiteral("No hay materias sin semestre asignado.");
    }

    return QStringLiteral("«%1» no tiene materias todavia.\n"
                          "Las materias nuevas se guardan en el semestre elegido en «Materias nuevas →».")
        .arg(m_semesterContext);
}

void CourseGridWidget::refreshGrid(bool force)
{
    const int columns = columnsForWidth(width());
    if (!force && columns == m_lastColumnCount) {
        return;
    }

    m_lastColumnCount = columns;

    QLayoutItem *item;
    while ((item = m_grid->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }

    const QVector<CourseUiState> list = filteredCourses();

    if (list.isEmpty()) {
        auto *empty = new QLabel(emptyStateMessage(), m_gridContainer);
        empty->setAlignment(Qt::AlignCenter);
        empty->setWordWrap(true);
        empty->setProperty("muted", true);
        empty->setStyleSheet(QStringLiteral("padding:32px;background:transparent;border:none;"));
        m_grid->addWidget(empty, 0, 0, 1, qMax(1, columns));
        m_grid->setRowStretch(1, 1);
        return;
    }

    int row = 0;
    int col = 0;

    for (const CourseUiState &course : list) {
        auto *card = new CourseCardWidget(m_gridContainer);
        card->setCourse(course);

        connect(card, &CourseCardWidget::openCourseRequested, this, &CourseGridWidget::openCourseRequested);
        connect(card, &CourseCardWidget::openFolderRequested, this, &CourseGridWidget::openFolderRequested);
        connect(card, &CourseCardWidget::syncCourseRequested, this, &CourseGridWidget::syncCourseRequested);
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
    const int oldColumns = columnsForWidth(event->oldSize().width());
    const int newColumns = columnsForWidth(event->size().width());
    if (oldColumns != newColumns) {
        scheduleRefresh(false);
    }
}
