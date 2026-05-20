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
class QButtonGroup;

class CourseDetailWidget : public QWidget {
    Q_OBJECT

public:
    explicit CourseDetailWidget(QWidget *parent = nullptr);

    void setCourse(const CourseUiState &course);
    void setAssignments(const QVector<AssignmentListItemData> &assignments);
    void setPublications(const QVector<PublicationListItemData> &publications);
    void setSearchText(const QString &text);

signals:
    void backRequested();
    void assignmentSelected(const QString &courseId, const QString &assignmentId);
    void openAssignmentFolderRequested(const QString &courseId, const QString &assignmentId);
    void openAssignmentClassroomRequested(const QString &courseId, const QString &assignmentId);

    void openPublicationFolderRequested(const QString &courseId, const QString &publicationId);
    void openPublicationClassroomRequested(const QString &courseId, const QString &publicationId);
    void downloadPublicationAttachmentRequested(const QString &courseId, const QString &publicationId,
                                                const QString &url, const QString &title);

    void syncCourseRequested(const QString &courseId);
    void openCourseFolderRequested(const QString &courseId);
    void openCourseClassroomRequested(const QString &courseId);
    void semesterChanged(const QString &courseId, const QString &semester);

private:
    void refreshList();
    void updateSummaryLabel();

    CourseSection m_currentSection = CourseSection::Tasks;
    CourseUiState m_course;
    QVector<AssignmentListItemData> m_assignments;
    QVector<PublicationListItemData> m_publications;
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

    QPushButton *m_tasksTabButton = nullptr;
    QPushButton *m_publicationsTabButton = nullptr;
    QButtonGroup *m_sectionGroup = nullptr;

    QScrollArea *m_scrollArea = nullptr;
    QWidget *m_listContainer = nullptr;
    QVBoxLayout *m_listLayout = nullptr;
};
