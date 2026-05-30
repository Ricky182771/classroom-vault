#pragma once

#include <QDir>
#include <QStandardPaths>
#include <QString>

// Central helpers for user-data directories.
// On Linux  : configDir() = ~/.config/ClassroomVault,  cacheDir() = ~/.cache/ClassroomVault
// On Windows: configDir() = %APPDATA%\ClassroomVault, cacheDir() = %LOCALAPPDATA%\ClassroomVault
// Using GenericConfigLocation / GenericCacheLocation as the base guarantees the correct
// platform root without depending on QCoreApplication::applicationName() being set first.
namespace Paths {

inline QString configDir()
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    if (base.isEmpty()) {
        base = QDir::homePath() + QStringLiteral("/.config");
    }
    return QDir(base).filePath(QStringLiteral("ClassroomVault"));
}

inline QString cacheDir()
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation);
    if (base.isEmpty()) {
        base = QDir::homePath() + QStringLiteral("/.cache");
    }
    return QDir(base).filePath(QStringLiteral("ClassroomVault"));
}

inline QString defaultBasePath()
{
    const QString docs = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    return QDir(docs.isEmpty() ? QDir::homePath() : docs)
               .filePath(QStringLiteral("ClassroomVault"));
}

} // namespace Paths
