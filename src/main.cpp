#include "Models.hpp"
#include "SyncManager.hpp"
#include "ui/MainWindow.hpp"
#include "ui/StyleManager.hpp"

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QTextStream>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    if (!qEnvironmentVariableIsSet("CLASSROOM_VAULT_DISABLE_THEME")) {
        StyleManager::applyDarkTheme(app);
    }
    app.setOrganizationName(QStringLiteral("ClassroomVault"));
    app.setApplicationName(QStringLiteral("ClassroomVault"));

    qRegisterMetaType<QList<Course>>("QList<Course>");
    qRegisterMetaType<QList<Assignment>>("QList<Assignment>");
    qRegisterMetaType<AssignmentMaterial>("AssignmentMaterial");

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
    window.show();

    return app.exec();
}
