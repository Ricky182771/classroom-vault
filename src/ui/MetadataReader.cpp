#include "MetadataReader.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>

namespace {

QString dueDateText(const QJsonObject &dueDate)
{
    const int year = dueDate.value(QStringLiteral("year")).toInt();
    const int month = dueDate.value(QStringLiteral("month")).toInt();
    const int day = dueDate.value(QStringLiteral("day")).toInt();

    if (year <= 0 || month <= 0 || day <= 0) {
        return QStringLiteral("Sin fecha");
    }

    return QStringLiteral("%1-%2-%3")
        .arg(year, 4, 10, QLatin1Char('0'))
        .arg(month, 2, 10, QLatin1Char('0'))
        .arg(day, 2, 10, QLatin1Char('0'));
}

QString dueTimeText(const QJsonObject &dueTime)
{
    const int hours = dueTime.value(QStringLiteral("hours")).toInt(-1);
    const int minutes = dueTime.value(QStringLiteral("minutes")).toInt(-1);
    if (hours < 0 || minutes < 0) {
        return QStringLiteral("Sin hora");
    }

    return QStringLiteral("%1:%2")
        .arg(hours, 2, 10, QLatin1Char('0'))
        .arg(minutes, 2, 10, QLatin1Char('0'));
}

AttachmentUiState parseAttachment(const QJsonObject &obj)
{
    AttachmentUiState attachment;
    attachment.id = obj.value(QStringLiteral("driveFileId")).toString().trimmed();
    if (attachment.id.isEmpty()) {
        attachment.id = obj.value(QStringLiteral("id")).toString().trimmed();
    }

    attachment.title = obj.value(QStringLiteral("title")).toString();
    attachment.fileName = obj.value(QStringLiteral("localFileName")).toString();
    if (attachment.fileName.isEmpty()) {
        attachment.fileName = obj.value(QStringLiteral("name")).toString();
    }
    attachment.type = obj.value(QStringLiteral("type")).toString();
    attachment.status = obj.value(QStringLiteral("status")).toString();
    attachment.localPath = obj.value(QStringLiteral("localPath")).toString();
    attachment.url = obj.value(QStringLiteral("url")).toString();
    if (attachment.url.isEmpty()) {
        attachment.url = obj.value(QStringLiteral("webViewLink")).toString();
    }
    attachment.mimeType = obj.value(QStringLiteral("mimeType")).toString();
    attachment.sourceMimeType = obj.value(QStringLiteral("sourceMimeType")).toString();
    attachment.exportMimeType = obj.value(QStringLiteral("exportMimeType")).toString();

    if (obj.contains(QStringLiteral("size"))) {
        const qint64 size = obj.value(QStringLiteral("size")).toVariant().toLongLong();
        if (size > 0) {
            attachment.sizeText = QString::number(size);
        }
    }

    attachment.existsLocally = !attachment.localPath.trimmed().isEmpty() && QFileInfo::exists(attachment.localPath);
    return attachment;
}

} // namespace

bool MetadataReader::readMetadataFile(const QString &metadataPath, AssignmentPreviewData *outData, QString *errorMessage)
{
    if (outData) {
        *outData = AssignmentPreviewData{};
    }

    if (errorMessage) {
        errorMessage->clear();
    }

    const QString path = metadataPath.trimmed();
    if (path.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Ruta de metadata vacia.");
        }
        return false;
    }

    QFile file(path);
    if (!file.exists()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("No se encontro metadata.json.");
        }
        return false;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("No se pudo leer metadata.json.");
        }
        return false;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    if (!doc.isObject()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("metadata.json invalido (no es objeto JSON).");
        }
        return false;
    }

    if (outData) {
        *outData = fromJsonObject(doc.object(), path);
    }

    return true;
}

AssignmentPreviewData MetadataReader::fromJsonObject(const QJsonObject &root, const QString &metadataPath)
{
    AssignmentPreviewData data;
    data.rawJson = root;

    data.courseId = root.value(QStringLiteral("courseId")).toString();
    data.courseName = root.value(QStringLiteral("courseName")).toString();
    data.assignmentId = root.value(QStringLiteral("assignmentId")).toString();
    data.title = root.value(QStringLiteral("title")).toString();
    data.description = root.value(QStringLiteral("description")).toString();
    data.workType = root.value(QStringLiteral("workType")).toString();
    data.state = root.value(QStringLiteral("state")).toString();
    data.alternateLink = root.value(QStringLiteral("alternateLink")).toString();
    data.syncedAt = root.value(QStringLiteral("syncedAt")).toString();

    data.metadataPath = metadataPath.trimmed();
    if (!data.metadataPath.isEmpty()) {
        data.localFolderPath = QFileInfo(data.metadataPath).absolutePath();
    }

    data.dueDateText = dueDateText(root.value(QStringLiteral("dueDate")).toObject());
    data.dueTimeText = dueTimeText(root.value(QStringLiteral("dueTime")).toObject());

    const QJsonArray attachments = root.value(QStringLiteral("attachments")).toArray();
    for (const QJsonValue &value : attachments) {
        if (!value.isObject()) {
            continue;
        }
        data.attachments.append(parseAttachment(value.toObject()));
    }

    if (data.attachments.isEmpty()) {
        const QJsonArray materials = root.value(QStringLiteral("materials")).toArray();
        for (const QJsonValue &value : materials) {
            if (!value.isObject()) {
                continue;
            }
            AttachmentUiState attachment = parseAttachment(value.toObject());
            if (attachment.type.trimmed().isEmpty()) {
                attachment.type = QStringLiteral("material");
            }
            if (attachment.status.trimmed().isEmpty()) {
                attachment.status = QStringLiteral("pending");
            }
            data.attachments.append(attachment);
        }
    }

    return data;
}
