#pragma once

#include <QJsonObject>
#include <QObject>
#include <QQueue>
#include <QString>
#include <QStringList>

class SyncStateManager;

class AttachmentChecksumManager : public QObject {
    Q_OBJECT

public:
    explicit AttachmentChecksumManager(QObject *parent = nullptr);

    void setSyncStateManager(SyncStateManager *syncStateManager);

    void verifyAllKnownAttachments();
    void verifyForAssignment(const QString &courseId, const QString &assignmentId);

signals:
    void checksumLog(const QString &message);
    void checksumVerificationFinished(const QString &courseId, const QString &assignmentId, int okCount, int failedCount);
    void checksumFailed(const QString &courseId, const QString &assignmentId, const QStringList &attachmentKeys);

private:
    struct VerifyTask {
        QString courseId;
        QString assignmentId;
        QString assignmentFolderPath;
        QJsonObject attachmentsState;
    };

    struct VerifyResult {
        QString courseId;
        QString assignmentId;
        QString assignmentFolderPath;
        QString checksumPath;
        int okCount = 0;
        QStringList failedRelativePaths;
        QStringList failedAttachmentKeys;
        bool generated = false;
        bool missingAttachmentsDir = false;
        bool error = false;
        QString errorMessage;
    };

    void enqueueTask(const VerifyTask &task);
    void startNext();
    void handleResult(const VerifyResult &result);

    SyncStateManager *m_syncStateManager = nullptr;
    QQueue<VerifyTask> m_queue;
    bool m_running = false;
};
