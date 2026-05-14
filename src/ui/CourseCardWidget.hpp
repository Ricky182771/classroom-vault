#pragma once

#include "UiModels.hpp"

#include <QFrame>

class QLabel;
class QProgressBar;
class QPushButton;

class CourseCardWidget : public QFrame {
    Q_OBJECT

public:
    explicit CourseCardWidget(QWidget *parent = nullptr);

    void setCourse(const CourseUiState &course);
    CourseUiState course() const;

signals:
    void openCourseRequested(const QString &courseId);
    void openFolderRequested(const QString &courseId);
    void syncCourseRequested(const QString &courseId);
    void openClassroomRequested(const QString &courseId);

private:
    void mousePressEvent(QMouseEvent *event) override;
    void applyStatusUi(const QString &status);

    CourseUiState m_course;

    QFrame *m_banner = nullptr;
    QLabel *m_statusBadge = nullptr;
    QLabel *m_nameLabel = nullptr;
    QLabel *m_codeLabel = nullptr;
    QLabel *m_semesterLabel = nullptr;
    QLabel *m_statsLabel = nullptr;
    QLabel *m_lastSyncLabel = nullptr;
    QProgressBar *m_progress = nullptr;

    QPushButton *m_openTasksButton = nullptr;
    QPushButton *m_openFolderButton = nullptr;
    QPushButton *m_syncButton = nullptr;
    QPushButton *m_classroomButton = nullptr;
};
