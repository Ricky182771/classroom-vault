#pragma once

#include <QString>
#include <QVector>

struct KpiData {
    QString label;
    int value = 0;
    QString iconName;
    QString status;
};

struct CourseUiState {
    QString id;
    QString name;
    QString code;
    QString semester;
    QString status;
    QString lastSync;
    QString folderPath;
    QString classroomUrl;

    int totalTasks = 0;
    int backedUpTasks = 0;
    int attachments = 0;
    int pending = 0;
    int errors = 0;
};

struct ActivityItem {
    QString time;
    QString kind;
    QString message;
};

struct TaskRowData {
    QString courseId;
    QString assignmentId;
    QString courseName;
    QString title;
    QString dueDate;
    QString metadataStatus;
    QString attachmentsStatus;
    int files = 0;
    QString lastSync;
    QString state;
    QString folderPath;
};

struct AttachmentRowData {
    QString courseName;
    QString assignmentTitle;
    QString fileName;
    QString type;
    QString status;
    QString size;
    QString localPath;
};

inline QString statusLabel(const QString &status)
{
    if (status == QStringLiteral("complete")) {
        return QStringLiteral("Completo");
    }
    if (status == QStringLiteral("pending")) {
        return QStringLiteral("Pendiente");
    }
    if (status == QStringLiteral("error")) {
        return QStringLiteral("Error");
    }
    return QStringLiteral("Sin sync");
}

inline double completionRatio(const CourseUiState &course)
{
    if (course.totalTasks <= 0) {
        return 0.0;
    }
    return static_cast<double>(course.backedUpTasks) / static_cast<double>(course.totalTasks);
}
