#pragma once

#include "UiModels.hpp"

#include <QWidget>
#include <QVector>

class QLabel;
class QPushButton;
class QLineEdit;
class QVBoxLayout;
class QScrollArea;
class AssignmentListItemWidget;
class QComboBox;

class CourseDetailWidget : public QWidget {
    Q_OBJECT

public:
    explicit CourseDetailWidget(QWidget *parent = nullptr);

    void setCourse(const CourseUiState &course);
    void setAssignments(const QVector<AssignmentListItemData> &assignments);
    void setSearchText(const QString &text);

signals:
    void backRequested();
    void assignmentSelected(const QString &courseId, const QString &assignmentId);
    void openAssignmentFolderRequested(const QString &courseId, const QString &assignmentId);
    void openAssignmentClassroomRequested(const QString &courseId, const QString &assignmentId);

    void syncCourseRequested(const QString &courseId);
    void openCourseFolderRequested(const QString &courseId);
    void openCourseClassroomRequested(const QString &courseId);
    void semesterChanged(const QString &courseId, const QString &semester);

private:
    void refreshAssignments();

    CourseUiState m_course;
    QVector<AssignmentListItemData> m_assignments;
    QString m_searchText;

    QLabel *m_titleLabel = nullptr;
    QLabel *m_semesterLabel = nullptr;
    QLabel *m_statusLabel = nullptr;
    QLabel *m_summaryLabel = nullptr;
    QLabel *m_pathLabel = nullptr;
    QComboBox *m_semesterCombo = nullptr;

    QPushButton *m_backButton = nullptr;
    QPushButton *m_syncButton = nullptr;
    QPushButton *m_openFolderButton = nullptr;
    QPushButton *m_openClassroomButton = nullptr;

    QScrollArea *m_scrollArea = nullptr;
    QWidget *m_listContainer = nullptr;
    QVBoxLayout *m_listLayout = nullptr;
};
