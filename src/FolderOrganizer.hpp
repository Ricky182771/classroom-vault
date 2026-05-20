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
    static QString sanitizeFileName(QString name);
    static QString buildAssignmentFolderName(const Assignment &assignment);

    QString createSemesterFolder(const QString &semester) const;
    QString createCourseFolder(const QString &semester, const QString &courseName) const;
    QString createAssignmentFolder(const QString &semester, const QString &courseName, const Assignment &assignment) const;

    QString resolveFolderConflict(const QString &desiredPath, const QString &assignmentId) const;

    bool writeMetadata(const QString &filePath, const QJsonObject &metadata) const;
    bool writeMetadataIfChanged(
        const QString &metadataPath,
        const QJsonObject &metadata,
        const QString &newHash,
        const QString &oldHash) const;
    bool writeDescriptionMarkdown(const QString &assignmentFolder, const QJsonObject &metadata) const;

private:
    bool ensureDir(const QString &path) const;
    QString metadataAssignmentId(const QString &assignmentDir) const;

    QString m_basePath;
};
