#include "MainWindow.hpp"
#include "Models.hpp"
#include "SyncManager.hpp"

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QTextStream>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
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

    parser.addOption(cliSyncOption);
    parser.addOption(basePathOption);
    parser.addOption(samplePathOption);
    parser.process(app);

    if (parser.isSet(cliSyncOption)) {
        QTextStream out(stdout);
        QTextStream err(stderr);
        int cliExitCode = 0;
        bool syncTriggered = false;

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
            [&out](int newCount, int updatedCount, int unchangedCount, int errorCount) {
                out << "Sincronizacion finalizada. Nuevas: " << newCount
                    << ", actualizadas: " << updatedCount
                    << ", sin cambios: " << unchangedCount
                    << ", errores: " << errorCount << "\n";
                out.flush();
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
