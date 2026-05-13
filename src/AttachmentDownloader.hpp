#pragma once

#include "Models.hpp"

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

private slots:
    void onFileMetadataLoaded(const QString &fileId, const QJsonObject &metadata);
    void onDownloadFinished(const QString &fileId, const QString &savedPath);
    void onDownloadSkipped(const QString &fileId, const QString &reason);
    void onRequestFailed(const QString &context, int httpStatus, const QString &errorMessage);

private:
    struct WorkItem {
        Assignment assignment;
        AssignmentMaterial material;
    };

    void processNext();
    void processCurrentDriveMaterial();
    void finalizeCurrentItem(bool downloaded, bool skipped, bool errorOccurred);

    bool createUrlFile(const QString &destinationDir, const QString &title, const QString &url, bool *unchanged = nullptr);
    QString buildAttachmentsDir(const Assignment &assignment, bool *ok = nullptr) const;
    QString materialDisplayName(const AssignmentMaterial &material) const;
    QString linkKey(const QString &url) const;
    QString localFileMd5(const QString &path) const;

    DriveClient *m_driveClient = nullptr;
    SyncStateManager *m_syncStateManager = nullptr;

    bool m_driveDownloadsEnabled = true;

    QVector<WorkItem> m_queue;
    WorkItem m_current;
    bool m_hasCurrent = false;
    QJsonObject m_currentDriveMetadata;

    int m_currentIndex = 0;
    int m_total = 0;

    int m_downloaded = 0;
    int m_skipped = 0;
    int m_errors = 0;
};
