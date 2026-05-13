#pragma once

#include "Models.hpp"

#include <QJsonArray>
#include <QJsonDocument>
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
    void errorOccurred(const QString &operation, const QString &message);
    void logMessage(const QString &message);

private slots:
    void onReplyFinished();

private:
    QJsonDocument loadSampleDocument() const;

    QList<Course> parseCourses(const QJsonArray &array) const;
    QList<Assignment> parseCourseWork(const QString &courseId, const QJsonArray &array) const;

    Course parseCourseObject(const QJsonObject &json) const;
    Assignment parseAssignmentObject(const QString &courseId, const QJsonObject &json) const;

    void fetchCoursesFromSample();
    void fetchCourseWorkFromSample(const QString &courseId);

    void fetchCoursesFromApi();
    void fetchCourseWorkFromApi(const QString &courseId);

    QString m_accessToken;
    bool m_useSampleData = true;
    QString m_sampleDataPath;

    QNetworkAccessManager m_networkManager;
};
