#pragma once

#include "Models.hpp"

#include <QMainWindow>

class QLineEdit;
class QPushButton;
class QLabel;
class QProgressBar;
class QTableWidget;
class QTableWidgetItem;
class QTextEdit;
class SyncManager;

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
    void onLoadSampleData();
    void onLoadClassroom();
    void onSyncFolders();

    void onCoursesChanged(const QList<Course> &courses);
    void onAssignmentsChanged(const QList<Assignment> &assignments);
    void onCourseTableItemChanged(QTableWidgetItem *item);
    void onAssignmentDoubleClicked(int row, int column);

    void onSyncProgress(int current, int total);
    void onSyncFinished(int newCount, int updatedCount, int unchangedCount, int errorCount);
    void onCountersChanged(
        int courses,
        int assignments,
        int newCount,
        int updatedCount,
        int unchangedCount,
        int errorCount);

    void appendLog(const QString &message);
    void appendError(const QString &message);

private:
    void setupUi();
    void connectSignals();
    QString resolveSampleDataPath() const;

    SyncManager *m_syncManager = nullptr;

    QLineEdit *m_basePathEdit = nullptr;
    QPushButton *m_browseButton = nullptr;
    QPushButton *m_openBaseFolderButton = nullptr;
    QPushButton *m_clearLogsButton = nullptr;
    QPushButton *m_loginButton = nullptr;
    QPushButton *m_loadSampleButton = nullptr;
    QPushButton *m_loadClassroomButton = nullptr;
    QPushButton *m_syncButton = nullptr;

    QTableWidget *m_coursesTable = nullptr;
    QTableWidget *m_assignmentsTable = nullptr;
    QTextEdit *m_logText = nullptr;
    QProgressBar *m_progressBar = nullptr;

    QLabel *m_coursesCountLabel = nullptr;
    QLabel *m_assignmentsCountLabel = nullptr;
    QLabel *m_newCountLabel = nullptr;
    QLabel *m_updatedCountLabel = nullptr;
    QLabel *m_unchangedCountLabel = nullptr;
    QLabel *m_errorCountLabel = nullptr;

    bool m_updatingCourseTable = false;
    QList<Course> m_currentCourses;
};
