#pragma once

#include "AttachmentDownloader.hpp"
#include "ClassroomClient.hpp"
#include "ConfigManager.hpp"
#include "DriveClient.hpp"
#include "FolderOrganizer.hpp"
#include "GoogleAuth.hpp"
#include "Models.hpp"
#include "SyncStateManager.hpp"

#include <QHash>
#include <QObject>

class SyncManager : public QObject {
    Q_OBJECT

public:
    explicit SyncManager(QObject *parent = nullptr);

    ConfigManager &configManager();
    ClassroomClient *classroomClient();
    GoogleAuth *googleAuth();

    QList<Course> courses() const;
    QList<Assignment> allAssignments() const;

    QString basePath() const;
    void setBasePath(const QString &basePath);

    QString semesterForCourse(const QString &courseId) const;
    void setSemesterForCourse(const QString &courseId, const QString &semester);
    void setSemesterMapping(const QHash<QString, QString> &mapping);

    QString assignmentFolderPath(const QString &courseId, const QString &assignmentId) const;

    void loadSampleData(const QString &samplePath = QString());
    void fetchFromClassroom();
    void setAutoDownloadAttachments(bool enabled);
    bool autoDownloadAttachments() const;

    void syncFolders();
    void downloadAttachments();

signals:
    void coursesChanged(const QList<Course> &courses);
    void assignmentsChanged(const QList<Assignment> &assignments);
    void syncProgress(int current, int total);
    void syncFinished(int newCount, int updatedCount, int unchangedCount, int errorCount);
    void countersChanged(
        int courses,
        int assignments,
        int newCount,
        int updatedCount,
        int unchangedCount,
        int errorCount);
    void attachmentProgress(int current, int total);
    void attachmentFinished(int downloaded, int skipped, int errors);
    void attachmentCountersChanged(int downloaded, int skipped, int errors);
    void logMessage(const QString &message);
    void errorOccurred(const QString &message);

private slots:
    void onCoursesFetched(const QList<Course> &courses);
    void onCourseWorkFetched(const QString &courseId, const QList<Assignment> &courseWork);
    void onClientRequestFailed(const QString &context, int httpStatus, const QString &message);
    void onAttachmentProgress(int current, int total);
    void onAttachmentFinished(int downloaded, int skipped, int errors);
    void onAttachmentLog(const QString &message);

    void onTokenUpdated();
    void onAuthSucceeded();

private:
    void refreshAuthConfig();
    void startFetchingCourses();
    QJsonObject buildMetadata(const Course &course, const Assignment &assignment) const;

    void logInfo(const QString &message);
    void logNew(const QString &message);
    void logSame(const QString &message);
    void logUpdated(const QString &message);
    void logErr(const QString &message);

    void emitCounters();

    ConfigManager m_configManager;
    SyncStateManager m_syncStateManager;
    FolderOrganizer m_folderOrganizer;
    ClassroomClient m_classroomClient;
    DriveClient m_driveClient;
    AttachmentDownloader m_attachmentDownloader;
    GoogleAuth m_googleAuth;

    QList<Course> m_courses;
    QHash<QString, QList<Assignment>> m_assignmentsByCourse;
    QHash<QString, QString> m_semesterByCourse;

    int m_pendingCourseWorkRequests = 0;
    bool m_waitingForTokenRefresh = false;

    int m_newCount = 0;
    int m_updatedCount = 0;
    int m_unchangedCount = 0;
    int m_errorCount = 0;
    int m_attachmentDownloaded = 0;
    int m_attachmentSkipped = 0;
    int m_attachmentErrors = 0;
    bool m_autoDownloadAttachments = false;
};
