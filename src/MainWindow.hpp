#pragma once

#include "Models.hpp"

#include <QMainWindow>

class QLineEdit;
class QPushButton;
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
    void onLogin();
    void onLoadSampleData();
    void onLoadClassroom();
    void onSyncFolders();

    void onCoursesChanged(const QList<Course> &courses);
    void onAssignmentsChanged(const QList<Assignment> &assignments);
    void onCourseTableItemChanged(QTableWidgetItem *item);

    void onSyncProgress(int current, int total);
    void onSyncFinished(int changedCount, int unchangedCount);

    void appendLog(const QString &message);
    void appendError(const QString &message);

private:
    void setupUi();
    void connectSignals();
    QString resolveSampleDataPath() const;

    SyncManager *m_syncManager = nullptr;

    QLineEdit *m_basePathEdit = nullptr;
    QPushButton *m_browseButton = nullptr;
    QPushButton *m_loginButton = nullptr;
    QPushButton *m_loadSampleButton = nullptr;
    QPushButton *m_loadClassroomButton = nullptr;
    QPushButton *m_syncButton = nullptr;

    QTableWidget *m_coursesTable = nullptr;
    QTableWidget *m_assignmentsTable = nullptr;
    QTextEdit *m_logText = nullptr;

    bool m_updatingCourseTable = false;
    QList<Course> m_currentCourses;
};
