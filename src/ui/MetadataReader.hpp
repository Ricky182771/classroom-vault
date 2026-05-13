#pragma once

#include "UiModels.hpp"

#include <QString>

class MetadataReader {
public:
    static bool readMetadataFile(
        const QString &metadataPath,
        AssignmentPreviewData *outData,
        QString *errorMessage = nullptr);

    static AssignmentPreviewData fromJsonObject(const QJsonObject &root, const QString &metadataPath = QString());
};
