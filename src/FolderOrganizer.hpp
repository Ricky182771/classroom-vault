#pragma once

#include "Models.hpp"

#include <QJsonObject>
#include <QString>

class FolderOrganizer {
public:
    FolderOrganizer() = default;

    void setBasePath(const QString &basePath);
    QString basePath() const;

    QString tasksRootPath() const;

    static QString sanitizeName(const QString &name);

    QString createSemesterFolder(const QString &semester) const;
    QString createCourseFolder(const QString &semester, const QString &courseName) const;
    QString createAssignmentFolder(const QString &semester, const QString &courseName, const Assignment &assignment) const;

    bool writeMetadata(const QString &filePath, const QJsonObject &metadata) const;

private:
    bool ensureDir(const QString &path) const;
    QString metadataAssignmentId(const QString &assignmentDir) const;
    QString makeUniqueAssignmentName(const QString &coursePath, const QString &preferredName, const Assignment &assignment) const;

    QString m_basePath;
};
