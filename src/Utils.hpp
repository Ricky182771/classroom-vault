#pragma once

#include "Models.hpp"

#include <QCryptographicHash>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

namespace Utils {

inline QString effectiveAssignmentTitle(const Assignment &assignment)
{
    const QString trimmed = assignment.title.trimmed();
    return trimmed.isEmpty() ? QStringLiteral("Sin titulo") : trimmed;
}

inline QString assignmentFolderLabel(const Assignment &assignment)
{
    const QString title = effectiveAssignmentTitle(assignment);
    if (assignment.dueDate.isValid()) {
        return assignment.dueDate.toString(QStringLiteral("yyyy-MM-dd")) + QStringLiteral(" - ") + title;
    }
    return QStringLiteral("Sin fecha - ") + title;
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

inline QString sha256Json(const QJsonObject &object)
{
    const QByteArray compact = QJsonDocument(object).toJson(QJsonDocument::Compact);
    return QString::fromUtf8(QCryptographicHash::hash(compact, QCryptographicHash::Sha256).toHex());
}

inline QJsonObject materialToJson(const AssignmentMaterial &material)
{
    QJsonObject obj;
    obj.insert(QStringLiteral("type"), material.type);
    obj.insert(QStringLiteral("title"), material.title);
    if (!material.alternateLink.isEmpty()) {
        obj.insert(QStringLiteral("alternateLink"), material.alternateLink);
    }
    if (!material.driveFileId.isEmpty()) {
        obj.insert(QStringLiteral("driveFileId"), material.driveFileId);
    }
    if (!material.url.isEmpty()) {
        obj.insert(QStringLiteral("url"), material.url);
    }
    return obj;
}

inline QJsonArray materialsToJsonArray(const QVector<AssignmentMaterial> &materials)
{
    QJsonArray array;
    for (const AssignmentMaterial &material : materials) {
        array.append(materialToJson(material));
    }
    return array;
}

} // namespace Utils
