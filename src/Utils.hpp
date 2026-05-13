#pragma once

#include "Models.hpp"

#include <QDateTime>
#include <QJsonObject>
#include <QString>

namespace Utils {

inline QString effectiveAssignmentTitle(const Assignment &assignment)
{
    const QString trimmed = assignment.title.trimmed();
    return trimmed.isEmpty() ? QStringLiteral("Tarea sin titulo") : trimmed;
}

inline QString assignmentFolderLabel(const Assignment &assignment)
{
    const QString title = effectiveAssignmentTitle(assignment);
    if (assignment.dueDate.isValid()) {
        return assignment.dueDate.toString(QStringLiteral("yyyy-MM-dd")) + QStringLiteral(" - ") + title;
    }
    return title;
}

inline QJsonObject dueDateObject(const QDate &date)
{
    QJsonObject due;
    if (date.isValid()) {
        due.insert(QStringLiteral("year"), date.year());
        due.insert(QStringLiteral("month"), date.month());
        due.insert(QStringLiteral("day"), date.day());
    }
    return due;
}

inline QJsonObject dueTimeObject(const QTime &time)
{
    QJsonObject due;
    if (time.isValid()) {
        due.insert(QStringLiteral("hours"), time.hour());
        due.insert(QStringLiteral("minutes"), time.minute());
        due.insert(QStringLiteral("seconds"), time.second());
    }
    return due;
}

inline QString nowIsoStringUtc()
{
    return QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
}

} // namespace Utils
