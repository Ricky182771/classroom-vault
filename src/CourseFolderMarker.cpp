#include "CourseFolderMarker.hpp"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QUuid>

namespace {

QString markerPath(const QString &coursePath)
{
    return QDir(coursePath).filePath(CourseFolderMarker::fileName());
}

} // namespace

namespace CourseFolderMarker {

QString fileName()
{
    return QStringLiteral(".classroom_vault.json");
}

QJsonObject read(const QString &coursePath)
{
    if (coursePath.trimmed().isEmpty()) {
        return {};
    }

    QFile file(markerPath(coursePath));
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
        return {};
    }

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    return doc.isObject() ? doc.object() : QJsonObject{};
}

QString ensure(
    const QString &coursePath,
    const QString &courseIdValue,
    const QString &courseNameValue,
    const QString &semester)
{
    if (coursePath.trimmed().isEmpty() || !QFileInfo::exists(coursePath)) {
        return QString();
    }

    QJsonObject marker = read(coursePath);
    const QString existingUid = marker.value(QStringLiteral("uid")).toString().trimmed();

    QJsonObject updated = marker;
    if (existingUid.isEmpty()) {
        updated.insert(QStringLiteral("uid"), QUuid::createUuid().toString(QUuid::WithoutBraces));
        updated.insert(QStringLiteral("createdAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    }

    // El courseId y el nombre son informativos y se refrescan; el uid, jamas.
    if (!courseIdValue.trimmed().isEmpty()) {
        updated.insert(QStringLiteral("courseId"), courseIdValue.trimmed());
    }
    if (!courseNameValue.trimmed().isEmpty()) {
        updated.insert(QStringLiteral("courseName"), courseNameValue.trimmed());
    }
    if (!semester.trimmed().isEmpty()) {
        updated.insert(QStringLiteral("semester"), semester.trimmed());
    }

    if (updated == marker) {
        return existingUid;
    }

    QFile file(markerPath(coursePath));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return existingUid;
    }
    file.write(QJsonDocument(updated).toJson(QJsonDocument::Indented));
    file.close();

    return updated.value(QStringLiteral("uid")).toString();
}

QString uid(const QString &coursePath)
{
    return read(coursePath).value(QStringLiteral("uid")).toString().trimmed();
}

QString courseId(const QString &coursePath)
{
    return read(coursePath).value(QStringLiteral("courseId")).toString().trimmed();
}

QString courseName(const QString &coursePath)
{
    return read(coursePath).value(QStringLiteral("courseName")).toString().trimmed();
}

} // namespace CourseFolderMarker
