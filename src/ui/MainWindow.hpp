#pragma once

#include "../Models.hpp"
#include "UiModels.hpp"

#include <QMainWindow>
#include <QHash>
#include <QJsonObject>
#include <QStringList>

class SyncManager;
class TopBarWidget;
class PathBarWidget;
class BreadcrumbWidget;
class HomeDashboardWidget;
class CourseDetailWidget;
class AssignmentDetailWidget;
class ActivityDrawerWidget;
class StatusBarWidget;
class QStackedWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override = default;

private:
    enum class ViewPage {
        Home,
        CourseDetail,
        AssignmentDetail,
    };

private slots:
    void onBrowseBasePath();
    void onOpenBaseFolder();
    void onClearLogs();
    void onLogin();
    void onLogout();
    void onLoadSampleData();
    void onSyncFolders();

    void onAuthStatusChanged(const QString &status);
    void onAuthFailed(const QString &errorMessage);

    void onCoursesChanged(const QList<Course> &courses);
    void onAssignmentsChanged(const QList<Assignment> &assignments);

    void onSyncProgress(int current, int total);
    void onSyncFinished(int newCount, int updatedCount, int unchangedCount, int errorCount);
    void onCountersChanged(
        int courses,
        int assignments,
        int newCount,
        int updatedCount,
        int unchangedCount,
        int errorCount);
    void onAttachmentProgress(int current, int total);
    void onAttachmentFinished(int downloaded, int skipped, int errors);
    void onAttachmentCountersChanged(int blobDownloaded, int exported, int linksSaved, int skipped, int errors);

    void onHomeCourseSelected(const QString &courseId);
    void onOpenCourseFolder(const QString &courseId);
    void onSyncCourseRequested(const QString &courseId);
    void onOpenCourseClassroom(const QString &courseId);

    void onCourseBackRequested();
    void onAssignmentSelected(const QString &courseId, const QString &assignmentId);
    void onOpenAssignmentFolder(const QString &courseId, const QString &assignmentId);
    void onOpenAssignmentClassroom(const QString &courseId, const QString &assignmentId);
    void onCourseSemesterChanged(const QString &courseId, const QString &semester);

    void onAssignmentBackRequested();
    void onResyncAssignmentMetadata(const QString &courseId, const QString &assignmentId);

    void onBreadcrumbHomeRequested();
    void onBreadcrumbCourseRequested(const QString &courseId);

    void onTopBarSearchChanged(const QString &text);
    void onTopBarAccountRequested();
    void onGlobalSemesterFilterChanged(const QString &semester);

    void appendLog(const QString &message);
    void appendError(const QString &message);

private:
    void attemptAutoLoadClassroom();

    void setupUi();
    void connectSignals();

    QString resolveSampleDataPath() const;
    QString formatIsoDateTime(const QString &isoText) const;

    void showHome();
    void showCourseDetail(const QString &courseId);
    void showAssignmentDetail(const QString &courseId, const QString &assignmentId);

    QVector<CourseUiState> buildCourseUiStates() const;
    QVector<AssignmentListItemData> buildCourseAssignments(const QString &courseId) const;
    AssignmentPreviewData buildAssignmentPreview(const QString &courseId, const QString &assignmentId) const;
    QVector<ActivityItem> recentActivityItems(int maxItems = 40) const;

    const Course *findCourse(const QString &courseId) const;
    const Assignment *findAssignment(const QString &courseId, const QString &assignmentId) const;
    CourseUiState courseUiById(const QString &courseId) const;

    int attachmentObjectCount(const QJsonObject &attachmentsObject) const;

    void refreshAuthUi();
    void refreshPathUi();
    void refreshStatusUi();
    void refreshActivityUi();
    void refreshHomeUi();
    void refreshCourseUi();
    void refreshAssignmentUi();
    void refreshAllViews();

    SyncManager *m_syncManager = nullptr;

    TopBarWidget *m_topBar = nullptr;
    PathBarWidget *m_pathBar = nullptr;
    BreadcrumbWidget *m_breadcrumb = nullptr;
    QStackedWidget *m_stack = nullptr;

    HomeDashboardWidget *m_home = nullptr;
    CourseDetailWidget *m_courseDetail = nullptr;
    AssignmentDetailWidget *m_assignmentDetail = nullptr;

    ActivityDrawerWidget *m_activityDrawer = nullptr;
    StatusBarWidget *m_statusBarWidget = nullptr;

    QList<Course> m_currentCourses;
    QList<Assignment> m_currentAssignments;

    QString m_currentCourseId;
    QString m_currentAssignmentId;
    ViewPage m_currentPage = ViewPage::Home;

    QString m_homeSearchText;
    QString m_courseSearchText;
    QString m_globalSemesterFilter = QStringLiteral("Todos los semestres");

    QStringList m_logLines;

    int m_coursesCount = 0;
    int m_assignmentsCount = 0;
    int m_newCount = 0;
    int m_updatedCount = 0;
    int m_unchangedCount = 0;
    int m_errorCount = 0;

    int m_attachmentBlobDownloaded = 0;
    int m_attachmentExported = 0;
    int m_attachmentLinksSaved = 0;
    int m_attachmentSkipped = 0;
    int m_attachmentErrors = 0;

    int m_syncProgressCurrent = 0;
    int m_syncProgressTotal = 0;
    QString m_runtimeStatus = QStringLiteral("Listo");
};
