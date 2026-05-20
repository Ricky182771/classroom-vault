#pragma once

#include <QDate>
#include <QJsonObject>
#include <QString>
#include <QTime>
#include <QVector>

struct Course {
    QString id;
    QString name;
    QString section;
    QString descriptionHeading;
    QString alternateLink;
};

struct AssignmentMaterial {
    QString type;
    QString title;
    QString alternateLink;
    QString driveFileId;
    QString url;
    QJsonObject rawJson;
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
    QString submissionId;
    QString submissionState;
    QString submissionUpdateTime;
    QString submissionAlternateLink;
    bool submissionLate = false;
    bool submissionStateReliable = false;
    double maxPoints = -1.0;
    double submissionAssignedGrade = -1.0;
    double submissionDraftGrade = -1.0;
    QVector<AssignmentMaterial> materials;
    QJsonObject rawJson;
};

Q_DECLARE_METATYPE(Course)
Q_DECLARE_METATYPE(Assignment)
Q_DECLARE_METATYPE(AssignmentMaterial)
