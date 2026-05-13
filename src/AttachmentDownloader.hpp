#pragma once

#include "Models.hpp"

#include <QJsonObject>
#include <QObject>
#include <QVector>

class DriveClient;
class SyncStateManager;

class AttachmentDownloader : public QObject {
    Q_OBJECT

public:
    explicit AttachmentDownloader(QObject *parent = nullptr);

    void setDriveClient(DriveClient *driveClient);
    void setSyncStateManager(SyncStateManager *syncStateManager);
    void setDriveDownloadsEnabled(bool enabled);

    void downloadAttachmentsForAssignments(const QVector<Assignment> &assignments);

signals:
    void attachmentProgress(int current, int total);
    void attachmentLog(const QString &message);
    void attachmentFinished(int downloaded, int skipped, int errors);
    void attachmentCountersChanged(int blobDownloaded, int exported, int linksSaved, int skipped, int errors);

private slots:
    void onFileMetadataLoaded(const QString &fileId, const QJsonObject &metadata);
    void onDownloadFinished(const QString &fileId, const QString &savedPath);
    void onDownloadSkipped(const QString &fileId, const QString &reason);
    void onRequestFailed(const QString &context, int httpStatus, const QString &errorMessage);

private:
    enum class ResultKind {
        BlobDownloaded,
        WorkspaceExported,
        LinkSaved,
        Skipped,
        Error
    };

    struct WorkItem {
        Assignment assignment;
        AssignmentMaterial material;
    };

    void processNext();
    void processCurrentDriveMaterial();

    void finalizeCurrentItem(ResultKind result);
    void syncAssignmentMetadataAttachments(const Assignment &assignment) const;

    QString assignmentFolderPath(const Assignment &assignment) const;
    QString attachmentsDirPath(const Assignment &assignment) const;
    QString ensureAttachmentsDir(const Assignment &assignment, bool *ok = nullptr) const;

    QString materialDisplayName(const AssignmentMaterial &material) const;
    QString linkKey(const QString &url) const;
    QString localFileMd5(const QString &path) const;
    QString preferredUrlForMaterial(const AssignmentMaterial &material, const QJsonObject &driveMetadata = QJsonObject()) const;
    QString workspaceLogTag(const QString &sourceMimeType) const;

    bool createUrlFile(
        const QString &destinationDir,
        const QString &title,
        const QString &url,
        QString *savedPath = nullptr,
        bool *unchanged = nullptr);

    bool saveLinkForCurrentMaterial(
        const QString &prefix,
        const QString &status,
        const QString &url,
        const QString &key,
        bool *unchanged = nullptr,
        const QString &sourceMimeType = QString());

    bool isUnchangedByState(
        const QJsonObject &existingRecord,
        const QString &localPath,
        const QString &remoteMd5,
        qint64 remoteSize,
        const QString &remoteModifiedTime) const;

    QJsonObject buildDriveAttachmentEntry(
        const QString &status,
        const QString &fileId,
        const QString &localPath,
        const QJsonObject &metadata,
        const QString &exportMimeType = QString()) const;

    DriveClient *m_driveClient = nullptr;
    SyncStateManager *m_syncStateManager = nullptr;

    bool m_driveDownloadsEnabled = true;

    QVector<WorkItem> m_queue;
    WorkItem m_current;
    bool m_hasCurrent = false;
    QJsonObject m_currentDriveMetadata;
    QString m_currentDriveAction;
    QString m_currentDriveExportMimeType;

    int m_currentIndex = 0;
    int m_total = 0;

    int m_blobDownloaded = 0;
    int m_workspaceExported = 0;
    int m_linksSaved = 0;
    int m_skipped = 0;
    int m_errors = 0;
};
