#include "SyncDiffEngine.hpp"

#include "SyncStateManager.hpp"
#include "Utils.hpp"

#include <QSet>

QVector<SyncAction> SyncDiffEngine::diffCourse(
    const QString &courseId,
    const SyncStagingManager &staging,
    const SyncStateManager &state,
    bool allowDeletedArchive) const
{
    QVector<SyncAction> actions;
    if (courseId.trimmed().isEmpty()) {
        return actions;
    }

    const QStringList stagedIds = staging.stagedAssignmentIds(courseId);
    const QStringList localIds = state.assignmentIds(courseId);
    const QSet<QString> stagedSet = QSet<QString>(stagedIds.begin(), stagedIds.end());

    actions.reserve(stagedIds.size() + localIds.size());

    for (const QString &assignmentId : stagedIds) {
        const QJsonObject stagedMetadata = staging.readAssignmentMetadata(courseId, assignmentId);
        if (stagedMetadata.isEmpty()) {
            continue;
        }

        const QJsonObject localState = state.assignmentState(courseId, assignmentId);
        const bool hasLocal = state.hasAssignment(courseId, assignmentId);
        const bool archivedDeleted = state.isAssignmentArchivedDeleted(courseId, assignmentId);
        const QString stagedHash = Utils::sha256Json(stagedMetadata);
        const QString localHash = state.assignmentMetadataHash(courseId, assignmentId);
        const bool localMetadataExists = state.localMetadataExists(courseId, assignmentId);

        SyncAction action;
        action.courseId = courseId;
        action.assignmentId = assignmentId;
        action.title = stagedMetadata.value(QStringLiteral("title")).toString();
        action.stagedMetadata = stagedMetadata;
        action.localState = localState;

        if (!hasLocal) {
            action.type = SyncActionType::NewAssignment;
        } else if (archivedDeleted) {
            action.type = SyncActionType::RestoredAssignment;
        } else if (localHash != stagedHash || !localMetadataExists) {
            action.type = SyncActionType::UpdatedAssignment;
        } else {
            action.type = SyncActionType::UnchangedAssignment;
        }

        actions.append(action);
    }

    if (!allowDeletedArchive) {
        return actions;
    }

    for (const QString &localId : localIds) {
        if (stagedSet.contains(localId)) {
            continue;
        }

        SyncAction action;
        action.type = SyncActionType::DeletedArchivedAssignment;
        action.courseId = courseId;
        action.assignmentId = localId;
        action.localState = state.assignmentState(courseId, localId);
        action.title = action.localState.value(QStringLiteral("title")).toString();
        actions.append(action);
    }

    return actions;
}
