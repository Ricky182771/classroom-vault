#pragma once

#include "../Models.hpp"

#include <QMainWindow>
#include <QHash>
#include <QJsonObject>
#include <QStringList>

class SyncManager;
class SidebarWidget;
class TopBarWidget;
class PathBarWidget;
class DashboardWidget;
class StatusBarWidget;
class TasksViewWidget;
class AttachmentsViewWidget;
class HistoryViewWidget;
class SettingsViewWidget;
class QStackedWidget;
class QTableWidget;
class QTableWidgetItem;
class QPushButton;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override = default;

private slots:
    void onBrowseBasePath();
    void onOpenBaseFolder();
    void onClearLogs();
    void onLogin();
    void onLogout();
    void onLoadSampleData();
    void onLoadClassroom();
    void onSyncFolders();
    void onDownloadAttachments();

    void onAuthStatusChanged(const QString &status);
    void onAuthFailed(const QString &errorMessage);

    void onCoursesChanged(const QList<Course> &courses);
    void onAssignmentsChanged(const QList<Assignment> &assignments);
    void onCourseTableItemChanged(QTableWidgetItem *item);
    void onTaskActivated(const QString &courseId, const QString &assignmentId);

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

    void onNavigationRequested(const QString &pageId);
    void onToggleSidebar();
    void onDashboardOpenCourse(const QString &courseId);
    void onDashboardOpenFolder(const QString &courseId);
    void onDashboardSyncCourse(const QString &courseId);
    void onDashboardDownloadAttachments(const QString &courseId);
    void onDashboardOpenClassroom(const QString &courseId);

    void appendLog(const QString &message);
    void appendError(const QString &message);

private:
    void setupUi();
    void connectSignals();

    QString resolveSampleDataPath() const;
    void setCurrentPage(const QString &pageId);

    QJsonObject readSyncState() const;
    QString formatIsoDateTime(const QString &isoText) const;

    void refreshAuthUi();
    void refreshPathUi();
    void refreshStatusUi();
    void refreshCourseTable();
    void refreshTasksView();
    void refreshAttachmentsView();
    void refreshDashboard();
    void refreshHistoryView();
    void refreshAllViews();

    SyncManager *m_syncManager = nullptr;

    SidebarWidget *m_sidebar = nullptr;
    TopBarWidget *m_topBar = nullptr;
    PathBarWidget *m_pathBar = nullptr;
    QStackedWidget *m_stack = nullptr;
    DashboardWidget *m_dashboard = nullptr;
    StatusBarWidget *m_statusBarWidget = nullptr;

    QWidget *m_coursesPage = nullptr;
    QTableWidget *m_coursesTable = nullptr;
    QPushButton *m_loginButton = nullptr;
    QPushButton *m_logoutButton = nullptr;
    QPushButton *m_loadSampleButton = nullptr;
    QPushButton *m_loadClassroomButton = nullptr;

    TasksViewWidget *m_tasksView = nullptr;
    AttachmentsViewWidget *m_attachmentsView = nullptr;
    HistoryViewWidget *m_historyView = nullptr;
    SettingsViewWidget *m_settingsView = nullptr;

    QHash<QString, int> m_pageIndexById;

    QList<Course> m_currentCourses;
    QList<Assignment> m_currentAssignments;

    QStringList m_logLines;
    bool m_updatingCourseTable = false;

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
