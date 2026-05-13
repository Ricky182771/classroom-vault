#pragma once

#include "ClassroomClient.hpp"
#include "ConfigManager.hpp"
#include "FolderOrganizer.hpp"
#include "GoogleAuth.hpp"
#include "Models.hpp"

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

    void loadSampleData(const QString &samplePath = QString());
    void fetchFromClassroom();

    void syncFolders();

signals:
    void coursesChanged(const QList<Course> &courses);
    void assignmentsChanged(const QList<Assignment> &assignments);
    void syncProgress(int current, int total);
    void syncFinished(int changedCount, int unchangedCount);
    void logMessage(const QString &message);
    void errorOccurred(const QString &message);

private slots:
    void onCoursesFetched(const QList<Course> &courses);
    void onCourseWorkFetched(const QString &courseId, const QList<Assignment> &courseWork);
    void onClientError(const QString &operation, const QString &message);

    void onTokenUpdated();
    void onAuthSucceeded();

private:
    void refreshAuthConfig();
    void startFetchingCourses();
    QJsonObject buildMetadata(const Course &course, const Assignment &assignment) const;
    QString metadataHash(const QJsonObject &metadata) const;

    ConfigManager m_configManager;
    FolderOrganizer m_folderOrganizer;
    ClassroomClient m_classroomClient;
    GoogleAuth m_googleAuth;

    QList<Course> m_courses;
    QHash<QString, QList<Assignment>> m_assignmentsByCourse;
    QHash<QString, QString> m_semesterByCourse;

    int m_pendingCourseWorkRequests = 0;
    bool m_waitingForTokenRefresh = false;
};
