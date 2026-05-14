#pragma once

#include "SyncStagingManager.hpp"

#include <QJsonObject>
#include <QString>
#include <QVector>

class SyncStateManager;

enum class SyncActionType {
    NewAssignment,
    UpdatedAssignment,
    UnchangedAssignment,
    DeletedArchivedAssignment,
    RestoredAssignment
};

struct SyncAction {
    SyncActionType type = SyncActionType::UnchangedAssignment;
    QString courseId;
    QString assignmentId;
    QString title;
    QJsonObject stagedMetadata;
    QJsonObject localState;
};

class SyncDiffEngine {
public:
    QVector<SyncAction> diffCourse(
        const QString &courseId,
        const SyncStagingManager &staging,
        const SyncStateManager &state,
        bool allowDeletedArchive) const;
};
