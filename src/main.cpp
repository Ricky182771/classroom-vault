#include "Models.hpp"
#include "SyncManager.hpp"
#include "ui/MainWindow.hpp"
#include "ui/StyleManager.hpp"

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QGuiApplication>
#include <QIcon>
#include <QTextStream>

#ifdef Q_OS_WIN
#include <cstdio>
#include <windows.h>
#endif

int main(int argc, char *argv[])
{
#ifdef Q_OS_WIN
    // When built as a GUI subsystem (WIN32) there is no default console.
    // For --cli-sync, re-attach to the parent process console so that stdout
    // and stderr reach the terminal that launched us (cmd / PowerShell).
    // This must run before QApplication so the streams are ready for Qt's
    // own early output as well.
    const bool hasCliSync = [&]() {
        for (int i = 1; i < argc; ++i) {
            if (QLatin1String(argv[i]) == QLatin1String("--cli-sync")) {
                return true;
            }
        }
        return false;
    }();
    if (hasCliSync && AttachConsole(ATTACH_PARENT_PROCESS)) {
        // NOLINTNEXTLINE(*-mt-unsafe) — single-threaded at this point
        freopen("CONOUT$", "w", stdout);
        freopen("CONOUT$", "w", stderr);
        freopen("CONIN$",  "r", stdin);
    }
#endif

    QApplication app(argc, argv);
    app.setWindowIcon(QIcon::fromTheme(
        QStringLiteral("classroom-vault"),
        QIcon(QStringLiteral(":/icons/classroom-vault.svg"))));
    if (!qEnvironmentVariableIsSet("CLASSROOM_VAULT_DISABLE_THEME")) {
        StyleManager::applyDarkTheme(app);
    }
    QGuiApplication::setDesktopFileName(QStringLiteral("classroom-vault"));
    app.setOrganizationName(QStringLiteral("ClassroomVault"));
    app.setApplicationName(QStringLiteral("ClassroomVault"));

    qRegisterMetaType<QList<Course>>("QList<Course>");
    qRegisterMetaType<QList<Assignment>>("QList<Assignment>");
    qRegisterMetaType<AssignmentMaterial>("AssignmentMaterial");
    qRegisterMetaType<Publication>("Publication");
    qRegisterMetaType<QList<Publication>>("QList<Publication>");

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Classroom Vault / TareaSync"));
    parser.addHelpOption();

    QCommandLineOption cliSyncOption(
        QStringList{QStringLiteral("cli-sync")},
        QStringLiteral("Ejecuta sincronizacion en modo CLI usando datos de prueba."));
    QCommandLineOption basePathOption(
        QStringList{QStringLiteral("base-path")},
        QStringLiteral("Ruta base para crear la carpeta Tareas."),
        QStringLiteral("path"));
    QCommandLineOption samplePathOption(
        QStringList{QStringLiteral("sample")},
        QStringLiteral("Ruta a sample_classroom_data.json (opcional)."),
        QStringLiteral("path"));
    QCommandLineOption cliDownloadAttachmentsOption(
        QStringList{QStringLiteral("cli-download-attachments")},
        QStringLiteral("Descarga adjuntos en modo CLI despues de sincronizar carpetas."));

    parser.addOption(cliSyncOption);
    parser.addOption(basePathOption);
    parser.addOption(samplePathOption);
    parser.addOption(cliDownloadAttachmentsOption);
    parser.process(app);

    if (parser.isSet(cliSyncOption)) {
        QTextStream out(stdout);
        QTextStream err(stderr);
        int cliExitCode = 0;
        bool syncTriggered = false;
        bool syncCompleted = false;
        const bool cliDownloadAttachments = parser.isSet(cliDownloadAttachmentsOption);

        const QString basePath = parser.value(basePathOption).trimmed();
        if (basePath.isEmpty()) {
            err << "Error: debes indicar --base-path en modo CLI.\n";
            return 1;
        }

        const QString samplePath = parser.value(samplePathOption).trimmed();

        SyncManager syncManager;
        syncManager.setBasePath(basePath);

        QObject::connect(&syncManager, &SyncManager::logMessage, [&out](const QString &message) {
            out << message << "\n";
            out.flush();
        });

        QObject::connect(&syncManager, &SyncManager::errorOccurred, [&err, &cliExitCode, &syncTriggered](const QString &message) {
            err << "ERROR: " << message << "\n";
            err.flush();
            cliExitCode = 1;
            if (!syncTriggered) {
                qApp->quit();
            }
        });

        QObject::connect(&syncManager, &SyncManager::assignmentsChanged, [&syncManager, &syncTriggered](const QList<Assignment> &) {
            syncTriggered = true;
            syncManager.syncFolders();
        });

        QObject::connect(
            &syncManager,
            &SyncManager::syncFinished,
            [&out, &syncManager, &syncCompleted, cliDownloadAttachments](int newCount, int updatedCount, int unchangedCount, int errorCount) {
                syncCompleted = true;
                out << "Sincronizacion finalizada. Nuevas: " << newCount
                    << ", actualizadas: " << updatedCount
                    << ", sin cambios: " << unchangedCount
                    << ", errores: " << errorCount << "\n";
                out.flush();
                if (cliDownloadAttachments) {
                    syncManager.downloadAttachments();
                } else {
                    qApp->quit();
                }
            });

        QObject::connect(
            &syncManager,
            &SyncManager::attachmentFinished,
            [&out, &cliExitCode, &syncCompleted](int downloaded, int skipped, int errors) {
                if (!syncCompleted) {
                    return;
                }
                out << "Adjuntos finalizados. Descargados: " << downloaded
                    << ", omitidos: " << skipped
                    << ", errores: " << errors << "\n";
                out.flush();
                if (errors > 0) {
                    cliExitCode = 1;
                }
                qApp->quit();
            });

        syncManager.loadSampleData(samplePath);
        app.exec();
        return cliExitCode;
    }

    MainWindow window;
    window.showMaximized();

    return app.exec();
}
