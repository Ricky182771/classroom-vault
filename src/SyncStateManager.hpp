#pragma once

#include "Models.hpp"

#include <QDateTime>
#include <QJsonObject>
#include <QString>

class SyncStateManager {
public:
    explicit SyncStateManager(const QString &statePath = QString());

    void setStatePath(const QString &statePath);
    QString statePath() const;

    bool load();
    bool save() const;

    bool hasCourse(const QString &courseId) const;
    bool hasAssignment(const QString &courseId, const QString &assignmentId) const;

    QString courseFolderPath(const QString &courseId) const;
    QString assignmentFolderPath(const QString &courseId, const QString &assignmentId) const;
    QString assignmentMetadataPath(const QString &courseId, const QString &assignmentId) const;
    QString assignmentMetadataHash(const QString &courseId, const QString &assignmentId) const;
    QJsonObject assignmentState(const QString &courseId, const QString &assignmentId) const;

    bool isAssignmentMetadataChanged(
        const QString &courseId,
        const QString &assignmentId,
        const QJsonObject &metadata) const;

    void updateCourse(const Course &course, const QString &semester, const QString &folderPath);
    void updateAssignment(
        const Course &course,
        const Assignment &assignment,
        const QString &folderName,
        const QString &folderPath,
        const QJsonObject &metadata);
    QJsonObject assignmentAttachments(const QString &courseId, const QString &assignmentId) const;
    QJsonObject assignmentAttachmentsState(const QString &courseId, const QString &assignmentId) const;
    QJsonObject attachmentRecord(const QString &courseId, const QString &assignmentId, const QString &attachmentKey) const;
    void updateAttachment(
        const QString &courseId,
        const QString &assignmentId,
        const QString &attachmentKey,
        const QJsonObject &attachmentData);

    QString lastSync() const;
    void setLastSync(const QDateTime &dateTime);

private:
    void ensureDefaultState();
    void migrateLegacyFlatAssignments();

    QJsonObject courseObject(const QString &courseId) const;
    QJsonObject assignmentObject(const QString &courseId, const QString &assignmentId) const;

    QString m_statePath;
    QJsonObject m_root;
};
