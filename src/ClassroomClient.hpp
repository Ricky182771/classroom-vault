#pragma once

#include "Models.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QHash>
#include <QNetworkAccessManager>
#include <QObject>
#include <QString>

class QNetworkReply;

class ClassroomClient : public QObject {
    Q_OBJECT

public:
    explicit ClassroomClient(QObject *parent = nullptr);

    void setAccessToken(const QString &accessToken);

    void setUseSampleData(bool enabled);
    bool useSampleData() const;

    void setSampleDataPath(const QString &path);

    void fetchCourses();
    void fetchCourseWork(const QString &courseId);

signals:
    void coursesFetched(const QList<Course> &courses);
    void courseWorkFetched(const QString &courseId, const QList<Assignment> &courseWork);
    void courseWorkSnapshotFetched(const QString &courseId, const QList<Assignment> &courseWork, bool fetchComplete);
    void errorOccurred(const QString &operation, const QString &message);
    void requestFailed(const QString &context, int httpStatus, const QString &errorMessage);
    void tokenInvalid();
    void logMessage(const QString &message);

private slots:
    void onReplyFinished();

private:
    QJsonDocument loadSampleDocument() const;

    QList<Course> parseCourses(const QJsonArray &array) const;
    QList<Assignment> parseCourseWork(const QString &courseId, const QJsonArray &array) const;
    QVector<AssignmentMaterial> parseMaterials(const QJsonArray &array) const;
    QList<QJsonObject> parseStudentSubmissions(const QJsonArray &array) const;

    Course parseCourseObject(const QJsonObject &json) const;
    Assignment parseAssignmentObject(const QString &courseId, const QJsonObject &json) const;
    AssignmentMaterial parseMaterialObject(const QJsonObject &json) const;
    void applyStudentSubmissionsToAssignments(
        const QString &courseId,
        const QList<QJsonObject> &submissionObjects,
        bool reliable);
    void finalizeCourseWorkSnapshot(const QString &courseId, bool fetchComplete, bool submissionsReliable);

    void fetchCoursesFromSample();
    void fetchCourseWorkFromSample(const QString &courseId);

    void fetchCoursesFromApi();
    void fetchCourseWorkFromApi(const QString &courseId);
    void fetchCourseWorkPageFromApi(const QString &courseId, const QString &pageToken);
    void fetchStudentSubmissionsFromApi(const QString &courseId);
    void fetchStudentSubmissionsPageFromApi(const QString &courseId, const QString &pageToken);

    QString m_accessToken;
    bool m_useSampleData = true;
    QString m_sampleDataPath;

    QNetworkAccessManager m_networkManager;
    QHash<QString, QList<Assignment>> m_courseWorkAccumulator;
    QHash<QString, int> m_courseWorkPageCount;
    QHash<QString, QList<QJsonObject>> m_studentSubmissionsAccumulator;
    QHash<QString, int> m_studentSubmissionsPageCount;
};
