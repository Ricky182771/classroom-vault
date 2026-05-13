#pragma once

#include "UiModels.hpp"

#include <QWidget>

class QGridLayout;
class CourseCardWidget;

class CourseGridWidget : public QWidget {
    Q_OBJECT

public:
    explicit CourseGridWidget(QWidget *parent = nullptr);

    void setCourses(const QVector<CourseUiState> &courses);
    void setFilter(const QString &filter);

signals:
    void openCourseRequested(const QString &courseId);
    void openFolderRequested(const QString &courseId);
    void syncCourseRequested(const QString &courseId);
    void downloadAttachmentsRequested(const QString &courseId);
    void openClassroomRequested(const QString &courseId);

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    int columnsForWidth(int width) const;
    void scheduleRefresh(bool force = false);
    QVector<CourseUiState> filteredCourses() const;
    void refreshGrid(bool force = false);

    QVector<CourseUiState> m_courses;
    QString m_filter = QStringLiteral("all");

    QWidget *m_gridContainer = nullptr;
    QGridLayout *m_grid = nullptr;
    bool m_refreshScheduled = false;
    bool m_forceRefreshPending = false;
    int m_lastColumnCount = -1;
};
