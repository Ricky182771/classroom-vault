#pragma once

#include "Models.hpp"

#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QStringList>

class SyncStagingManager : public QObject {
    Q_OBJECT

public:
    explicit SyncStagingManager(QObject *parent = nullptr);

    bool beginSession(const QString &mode);
    QString sessionPath() const;

    bool writeCourse(const Course &course, const QJsonObject &rawCourseJson = QJsonObject());
    bool writeAssignment(
        const QString &courseId,
        const QString &assignmentId,
        const QJsonObject &metadata);
    bool writePublication(
        const QString &courseId,
        const QString &publicationId,
        const QJsonObject &metadata);

    bool writeCourseManifest(
        const QString &courseId,
        const QString &courseName,
        const QStringList &assignmentIds,
        bool fetchComplete,
        const QString &errorMessage = QString());
    bool writeCoursePublicationsManifest(
        const QString &courseId,
        const QStringList &publicationIds,
        bool fetchComplete);

    bool writeGlobalManifest(
        const QStringList &courseIds,
        bool fetchComplete,
        const QString &errorMessage = QString());

    QJsonObject readAssignmentMetadata(
        const QString &courseId,
        const QString &assignmentId) const;
    QJsonObject readPublicationMetadata(
        const QString &courseId,
        const QString &publicationId) const;

    QStringList stagedAssignmentIds(const QString &courseId) const;
    QStringList stagedPublicationIds(const QString &courseId) const;
    bool courseFetchComplete(const QString &courseId) const;
    bool coursePublicationsFetchComplete(const QString &courseId) const;

    void cleanup();
    void preserveForDebug();

private:
    bool writeJsonFile(const QString &path, const QJsonObject &json) const;
    QJsonObject readJsonFile(const QString &path) const;
    QString baseStagingPath() const;
    QString courseDirPath(const QString &courseId) const;

    QString m_sessionPath;
    bool m_preserveOnCleanup = false;
};
