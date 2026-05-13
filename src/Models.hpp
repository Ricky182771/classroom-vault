#pragma once

#include <QDate>
#include <QJsonObject>
#include <QString>
#include <QTime>

struct Course {
    QString id;
    QString name;
    QString section;
    QString descriptionHeading;
    QString alternateLink;
};

struct Assignment {
    QString id;
    QString courseId;
    QString title;
    QString description;
    QString workType;
    QString state;
    QString alternateLink;
    QDate dueDate;
    QTime dueTime;
    QJsonObject rawJson;
};

Q_DECLARE_METATYPE(Course)
Q_DECLARE_METATYPE(Assignment)
