#include "AssignmentStatusResolver.hpp"

namespace {

using AssignmentStatusResolver::AssignmentVisualStatus;
using AssignmentStatusResolver::VisualState;

QDate parseDueDateText(const QString &dueDateText)
{
    const QString clean = dueDateText.trimmed();
    if (clean.isEmpty() || clean == QStringLiteral("Sin fecha") || clean == QStringLiteral("—")) {
        return {};
    }
    return QDate::fromString(clean, QStringLiteral("yyyy-MM-dd"));
}

bool seemsSubmitted(const QString &stateText)
{
    const QString text = stateText.trimmed().toUpper();
    return text.contains(QStringLiteral("TURNED_IN"))
        || text.contains(QStringLiteral("SUBMITTED"))
        || text.contains(QStringLiteral("RETURNED"))
        || text.contains(QStringLiteral("ENTREGADA"));
}

VisualState makeState(AssignmentVisualStatus status)
{
    switch (status) {
    case AssignmentVisualStatus::Submitted:
        return {status, QStringLiteral("Entregada"), QStringLiteral("#40ff00"), QStringLiteral("#121212")};
    case AssignmentVisualStatus::ExpiredSubmitted:
        return {status, QStringLiteral("Expirada y entregada"), QStringLiteral("#949292"), QStringLiteral("#111111")};
    case AssignmentVisualStatus::ExpiredMissing:
        return {status, QStringLiteral("No entregada"), QStringLiteral("#ff0d00"), QStringLiteral("#ffffff")};
    case AssignmentVisualStatus::DeletedArchived:
        return {status, QStringLiteral("Eliminada y archivada"), QStringLiteral("#e77831"), QStringLiteral("#101010")};
    case AssignmentVisualStatus::Active:
    default:
        return {status, QStringLiteral("Vigente"), QStringLiteral("#239cc7"), QStringLiteral("#ffffff")};
    }
}

AssignmentVisualStatus resolveCore(bool archivedDeleted, const QDate &dueDate, const QString &stateText, const QDate &today)
{
    if (archivedDeleted) {
        return AssignmentVisualStatus::DeletedArchived;
    }

    const bool submitted = seemsSubmitted(stateText);
    if (submitted && dueDate.isValid() && dueDate < today) {
        return AssignmentVisualStatus::ExpiredSubmitted;
    }
    if (submitted) {
        return AssignmentVisualStatus::Submitted;
    }

    if (dueDate.isValid() && dueDate < today) {
        return AssignmentVisualStatus::ExpiredMissing;
    }

    return AssignmentVisualStatus::Active;
}

} // namespace

namespace AssignmentStatusResolver {

VisualState resolve(const AssignmentListItemData &item, const QDate &today)
{
    const AssignmentVisualStatus status = resolveCore(
        item.archivedDeleted,
        parseDueDateText(item.dueDateText),
        item.stateText,
        today);
    return makeState(status);
}

VisualState resolve(const AssignmentPreviewData &preview, const QDate &today)
{
    const AssignmentVisualStatus status = resolveCore(
        preview.archivedDeleted,
        parseDueDateText(preview.dueDateText),
        preview.state,
        today);
    return makeState(status);
}

} // namespace AssignmentStatusResolver
