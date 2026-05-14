#include "AttachmentChecksumManager.hpp"

#include "FolderOrganizer.hpp"
#include "SyncStateManager.hpp"
#include "Utils.hpp"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMetaObject>
#include <QPointer>
#include <QThreadPool>

namespace {

QString sha256File(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return QString();
    }

    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&file)) {
        file.close();
        return QString();
    }

    file.close();
    return QString::fromUtf8(hash.result().toHex());
}

QJsonObject buildChecksumJson(const QString &attachmentsDirPath, const QFileInfoList &files)
{
    QJsonObject filesObject;

    for (const QFileInfo &info : files) {
        if (!info.isFile()) {
            continue;
        }
        if (info.fileName() == QStringLiteral(".checksum")) {
            continue;
        }

        const QString sha = sha256File(info.absoluteFilePath());
        if (sha.isEmpty()) {
            continue;
        }

        QJsonObject entry;
        entry.insert(QStringLiteral("sha256"), sha);
        entry.insert(QStringLiteral("size"), static_cast<qint64>(info.size()));
        entry.insert(QStringLiteral("lastModified"), info.lastModified().toUTC().toString(Qt::ISODate));
        filesObject.insert(info.fileName(), entry);
    }

    QJsonObject root;
    root.insert(QStringLiteral("version"), 1);
    root.insert(QStringLiteral("algorithm"), QStringLiteral("SHA256"));
    root.insert(QStringLiteral("generatedAt"), Utils::nowIsoStringUtc());
    root.insert(QStringLiteral("files"), filesObject);
    return root;
}

bool writeChecksumFile(const QString &checksumPath, const QJsonObject &checksum)
{
    const QString tmpPath = checksumPath + QStringLiteral(".tmp");

    QFile tmpFile(tmpPath);
    if (!tmpFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }

    tmpFile.write(QJsonDocument(checksum).toJson(QJsonDocument::Indented));
    tmpFile.close();

    QFile::remove(checksumPath);
    return QFile::rename(tmpPath, checksumPath);
}

QStringList mapFailedRelativePathsToAttachmentKeys(const QJsonObject &attachmentsState, const QStringList &failedRelativePaths)
{
    QStringList keys;
    for (const QString &failedRelativePath : failedRelativePaths) {
        const QString targetName = QFileInfo(failedRelativePath).fileName();
        if (targetName.isEmpty()) {
            continue;
        }

        for (auto it = attachmentsState.begin(); it != attachmentsState.end(); ++it) {
            if (!it.value().isObject()) {
                continue;
            }
            const QJsonObject entry = it.value().toObject();
            const QString localPath = entry.value(QStringLiteral("localPath")).toString();
            const QString localFileName = entry.value(QStringLiteral("localFileName")).toString();
            if (QFileInfo(localPath).fileName() == targetName || localFileName == targetName) {
                if (!keys.contains(it.key())) {
                    keys.append(it.key());
                }
            }
        }
    }
    return keys;
}

} // namespace

AttachmentChecksumManager::AttachmentChecksumManager(QObject *parent)
    : QObject(parent)
{}

void AttachmentChecksumManager::setSyncStateManager(SyncStateManager *syncStateManager)
{
    m_syncStateManager = syncStateManager;
}

void AttachmentChecksumManager::verifyAllKnownAttachments()
{
    if (!m_syncStateManager) {
        return;
    }

    if (!m_syncStateManager->load()) {
        emit checksumLog(QStringLiteral("ERR   No se pudo leer sync_state.json para verificar checksums."));
        return;
    }

    emit checksumLog(QStringLiteral("HASH  Verificando checksums en segundo plano..."));

    const QStringList courseIds = m_syncStateManager->courseIds();
    for (const QString &courseId : courseIds) {
        const QStringList assignmentIds = m_syncStateManager->assignmentIds(courseId);
        for (const QString &assignmentId : assignmentIds) {
            verifyForAssignment(courseId, assignmentId);
        }
    }
}

void AttachmentChecksumManager::verifyForAssignment(const QString &courseId, const QString &assignmentId)
{
    if (!m_syncStateManager) {
        return;
    }

    const QString assignmentFolderPath = m_syncStateManager->assignmentFolderPath(courseId, assignmentId).trimmed();
    if (assignmentFolderPath.isEmpty()) {
        return;
    }

    VerifyTask task;
    task.courseId = courseId;
    task.assignmentId = assignmentId;
    task.assignmentFolderPath = assignmentFolderPath;
    task.attachmentsState = m_syncStateManager->assignmentAttachmentsState(courseId, assignmentId);
    enqueueTask(task);
}

void AttachmentChecksumManager::enqueueTask(const VerifyTask &task)
{
    m_queue.enqueue(task);
    startNext();
}

void AttachmentChecksumManager::startNext()
{
    if (m_running || m_queue.isEmpty()) {
        return;
    }

    m_running = true;
    const VerifyTask task = m_queue.dequeue();
    QPointer<AttachmentChecksumManager> guard(this);

    QThreadPool::globalInstance()->start([guard, task]() {
        if (!guard) {
            return;
        }

        VerifyResult result;
        result.courseId = task.courseId;
        result.assignmentId = task.assignmentId;
        result.assignmentFolderPath = task.assignmentFolderPath;

        const QString attachmentsDirPath = QDir(task.assignmentFolderPath).filePath(QStringLiteral("Adjuntos"));
        const QString checksumPath = QDir(attachmentsDirPath).filePath(QStringLiteral(".checksum"));
        result.checksumPath = checksumPath;

        const QFileInfo attachmentsDirInfo(attachmentsDirPath);
        if (!attachmentsDirInfo.exists() || !attachmentsDirInfo.isDir()) {
            result.missingAttachmentsDir = true;
            QMetaObject::invokeMethod(
                guard,
                [guard, result]() {
                    if (!guard) {
                        return;
                    }
                    guard->handleResult(result);
                },
                Qt::QueuedConnection);
            return;
        }

        const QDir attachmentsDir(attachmentsDirPath);
        const QFileInfoList files = attachmentsDir.entryInfoList(
            QDir::Files | QDir::NoDotAndDotDot,
            QDir::Name | QDir::IgnoreCase);

        QJsonObject checksumRoot;
        bool checksumValid = false;

        QFile checksumFile(checksumPath);
        if (checksumFile.exists() && checksumFile.open(QIODevice::ReadOnly)) {
            const QJsonDocument checksumDoc = QJsonDocument::fromJson(checksumFile.readAll());
            checksumFile.close();
            if (checksumDoc.isObject()) {
                checksumRoot = checksumDoc.object();
                checksumValid = checksumRoot.value(QStringLiteral("files")).isObject();
            }
        }

        if (!checksumValid) {
            if (files.isEmpty()) {
                QMetaObject::invokeMethod(
                    guard,
                    [guard, result]() {
                        if (!guard) {
                            return;
                        }
                        guard->handleResult(result);
                    },
                    Qt::QueuedConnection);
                return;
            }

            const QJsonObject generated = buildChecksumJson(attachmentsDirPath, files);
            if (!writeChecksumFile(checksumPath, generated)) {
                result.error = true;
                result.errorMessage = QStringLiteral("No se pudo escribir .checksum");
            } else {
                result.generated = true;
                result.okCount = generated.value(QStringLiteral("files")).toObject().size();
            }

            QMetaObject::invokeMethod(
                guard,
                [guard, result]() {
                    if (!guard) {
                        return;
                    }
                    guard->handleResult(result);
                },
                Qt::QueuedConnection);
            return;
        }

        const QJsonObject checksumFiles = checksumRoot.value(QStringLiteral("files")).toObject();
        int okCount = 0;
        QStringList failedRelativePaths;

        for (auto it = checksumFiles.begin(); it != checksumFiles.end(); ++it) {
            if (!it.value().isObject()) {
                continue;
            }

            const QString fileName = it.key();
            const QString filePath = QDir(attachmentsDirPath).filePath(fileName);
            const QFileInfo info(filePath);
            if (!info.exists() || !info.isFile()) {
                failedRelativePaths.append(fileName);
                continue;
            }

            const QJsonObject checksumEntry = it.value().toObject();
            const QString expected = checksumEntry.value(QStringLiteral("sha256")).toString();
            const QString actual = sha256File(filePath);
            if (expected.isEmpty() || actual.isEmpty() || expected.compare(actual, Qt::CaseInsensitive) != 0) {
                failedRelativePaths.append(fileName);
                continue;
            }

            ++okCount;
        }

        result.okCount = okCount;
        result.failedRelativePaths = failedRelativePaths;
        result.failedAttachmentKeys = mapFailedRelativePathsToAttachmentKeys(task.attachmentsState, failedRelativePaths);

        QMetaObject::invokeMethod(
            guard,
            [guard, result]() {
                if (!guard) {
                    return;
                }
                guard->handleResult(result);
            },
            Qt::QueuedConnection);
    });
}

void AttachmentChecksumManager::handleResult(const VerifyResult &result)
{
    if (!m_syncStateManager) {
        m_running = false;
        startNext();
        return;
    }

    QJsonObject checksumState;
    checksumState.insert(QStringLiteral("algorithm"), QStringLiteral("SHA256"));
    checksumState.insert(QStringLiteral("path"), result.checksumPath);
    checksumState.insert(QStringLiteral("lastVerified"), Utils::nowIsoStringUtc());

    if (result.missingAttachmentsDir) {
        checksumState.insert(QStringLiteral("status"), QStringLiteral("missing"));
        m_syncStateManager->updateAssignmentChecksumState(result.courseId, result.assignmentId, checksumState);
        m_syncStateManager->save();
        m_running = false;
        startNext();
        return;
    }

    if (result.error) {
        checksumState.insert(QStringLiteral("status"), QStringLiteral("error"));
        checksumState.insert(QStringLiteral("error"), result.errorMessage);
        m_syncStateManager->updateAssignmentChecksumState(result.courseId, result.assignmentId, checksumState);
        m_syncStateManager->save();

        emit checksumLog(QStringLiteral("ERR   Checksum: %1").arg(result.errorMessage));
        m_running = false;
        startNext();
        return;
    }

    if (result.generated) {
        checksumState.insert(QStringLiteral("status"), QStringLiteral("ok"));
        checksumState.insert(QStringLiteral("lastGenerated"), Utils::nowIsoStringUtc());
        m_syncStateManager->updateAssignmentChecksumState(result.courseId, result.assignmentId, checksumState);
        m_syncStateManager->save();
        emit checksumLog(
            QStringLiteral("HASH  Checksum creado para %1 archivos: %2")
                .arg(result.okCount)
                .arg(result.assignmentId));
    }

    if (!result.failedRelativePaths.isEmpty()) {
        checksumState.insert(QStringLiteral("status"), QStringLiteral("failed"));
        checksumState.insert(QStringLiteral("lastGenerated"), Utils::nowIsoStringUtc());

        QJsonArray failedArray;
        for (const QString &failed : result.failedRelativePaths) {
            failedArray.append(failed);
        }
        checksumState.insert(QStringLiteral("failedFiles"), failedArray);

        m_syncStateManager->updateAssignmentChecksumState(result.courseId, result.assignmentId, checksumState);
        m_syncStateManager->save();

        emit checksumLog(
            QStringLiteral("HASH  Fallo verificacion (%1): %2")
                .arg(result.assignmentId)
                .arg(result.failedRelativePaths.join(QStringLiteral(", "))));

        if (!result.failedAttachmentKeys.isEmpty()) {
            emit checksumFailed(result.courseId, result.assignmentId, result.failedAttachmentKeys);
        } else {
            emit checksumLog(QStringLiteral("ERR   No se pudo asociar archivo fallido con un adjunto remoto."));
        }
    } else if (!result.generated) {
        checksumState.insert(QStringLiteral("status"), QStringLiteral("ok"));
        m_syncStateManager->updateAssignmentChecksumState(result.courseId, result.assignmentId, checksumState);
        m_syncStateManager->save();
    }

    emit checksumVerificationFinished(
        result.courseId,
        result.assignmentId,
        result.okCount,
        result.failedRelativePaths.size());

    m_running = false;
    startNext();
}
