#pragma once

#include "UiModels.hpp"

#include <QDate>
#include <QString>

namespace AssignmentStatusResolver {

enum class AssignmentVisualStatus {
    Active,
    Submitted,
    ExpiredSubmitted,
    ExpiredMissing,
    DeletedArchived,
};

struct VisualState {
    AssignmentVisualStatus status = AssignmentVisualStatus::Active;
    QString label;
    QString backgroundColor;
    QString textColor;
};

VisualState resolve(const AssignmentListItemData &item, const QDate &today = QDate::currentDate());
VisualState resolve(const AssignmentPreviewData &preview, const QDate &today = QDate::currentDate());

} // namespace AssignmentStatusResolver
