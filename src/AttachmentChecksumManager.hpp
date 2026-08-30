#pragma once

#include <QJsonObject>
#include <QObject>
#include <QQueue>
#include <QSet>
#include <QString>
#include <QStringList>

class SyncStateManager;

class AttachmentChecksumManager : public QObject {
    Q_OBJECT

public:
    explicit AttachmentChecksumManager(QObject *parent = nullptr);

    void setSyncStateManager(SyncStateManager *syncStateManager);
    // Cursos de semestres archivados: el worker de verificacion los ignora por completo
    // (no lee, no verifica y no regenera .checksum bajo un semestre en solo lectura).
    void setArchivedCourseIds(const QSet<QString> &archivedCourseIds);
    // Raices de los semestres archivados. Complementa el filtro por courseId:
    // atrapa carpetas que siguen dentro del arbol archivado aunque su materia
    // ya no se resuelva como archivada.
    void setArchivedPathRoots(const QStringList &archivedPathRoots);

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
    QSet<QString> m_archivedCourseIds;
    QStringList m_archivedPathRoots;
    QQueue<VerifyTask> m_queue;
    bool m_running = false;
};
