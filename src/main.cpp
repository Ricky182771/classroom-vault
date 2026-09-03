#include "ConfigManager.hpp"
#include "Models.hpp"
#include "SyncManager.hpp"
#include "SyncStateManager.hpp"
#include "ui/MainWindow.hpp"
#include "ui/StyleManager.hpp"

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QIcon>
#include <QTextStream>

#ifdef Q_OS_WIN
#include <cstdio>
#include <windows.h>
#endif

namespace {

// Mueve el contenido de src dentro de dst sin sobrescribir nada. Devuelve cuantas
// entradas se movieron; las que ya existian en destino se dejan en su sitio y se
// reportan. No borra ningun fichero.
int mergeDirectoryInto(const QString &src, const QString &dst, QTextStream &out)
{
    QDir source(src);
    if (!source.exists()) {
        return 0;
    }

    QDir().mkpath(dst);

    int moved = 0;
    const QFileInfoList entries = source.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden);
    for (const QFileInfo &entry : entries) {
        const QString target = QDir(dst).filePath(entry.fileName());
        if (QFileInfo::exists(target)) {
            out << "      · ya existe en destino, se deja donde esta: " << entry.fileName() << "\n";
            continue;
        }
        if (QFile::rename(entry.absoluteFilePath(), target)) {
            ++moved;
        } else {
            out << "      · NO se pudo mover: " << entry.absoluteFilePath() << "\n";
        }
    }

    // Solo se elimina el directorio si quedo completamente vacio.
    source.rmdir(QStringLiteral("."));
    return moved;
}

// Reconcilia el residuo de que tareas y publicaciones resolvieran la carpeta de la
// materia por caminos distintos: carpetas de materia que solo contienen
// _Publicaciones, colgando de un semestre que no es el registrado para esa materia.
void reconcileSplitCourses(SyncStateManager &state, const QString &newBase, bool apply, QTextStream &out)
{
    const QString tasksRoot = QDir(newBase).filePath(QStringLiteral("Tareas"));
    const QDir root(tasksRoot);
    if (!root.exists()) {
        return;
    }

    bool headerPrinted = false;
    const QStringList courseIds = state.courseIds();
    for (const QString &courseId : courseIds) {
        const QString registered = QDir::cleanPath(state.courseFolderPath(courseId).trimmed());
        // Solo se reconcilia contra una carpeta registrada que este bajo la base
        // actual: si el indice todavia apunta a una base anterior, el destino no
        // seria alcanzable y moveriamos ficheros a ninguna parte.
        if (registered.isEmpty() || !(registered == tasksRoot || registered.startsWith(tasksRoot + QLatin1Char('/')))) {
            continue;
        }
        const QString courseFolderName = QFileInfo(registered).fileName();

        const QStringList semesters = root.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QString &semester : semesters) {
            const QString candidate = QDir::cleanPath(QDir(root.filePath(semester)).filePath(courseFolderName));
            if (candidate == registered || !QFileInfo::exists(candidate)) {
                continue;
            }

            const QStringList contents = QDir(candidate).entryList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden);
            if (contents != QStringList{QStringLiteral("_Publicaciones")}) {
                // Solo se toca el residuo inequivoco: una carpeta de materia que no
                // contiene mas que publicaciones.
                continue;
            }

            if (!headerPrinted) {
                out << "\nMaterias partidas entre dos semestres (solo _Publicaciones sueltas):\n";
                headerPrinted = true;
            }
            out << "  " << candidate << "\n    -> " << registered << "\n";

            if (apply) {
                const int moved = mergeDirectoryInto(
                    QDir(candidate).filePath(QStringLiteral("_Publicaciones")),
                    QDir(registered).filePath(QStringLiteral("_Publicaciones")),
                    out);
                QDir(candidate).rmdir(QStringLiteral("."));
                out << "    movidas " << moved << " entradas\n";
            }
        }
    }
}

int runPathMigration(const QString &oldBaseArg, const QString &newBaseArg, bool apply)
{
    QTextStream out(stdout);
    QTextStream err(stderr);

    ConfigManager config;
    config.load();

    const QString newBase = QDir::cleanPath(
        (newBaseArg.isEmpty() ? config.basePath() : newBaseArg).trimmed());
    if (newBase.isEmpty()) {
        err << "Error: no hay ruta base configurada. Indica --new-base.\n";
        return 1;
    }

    const QString statePath = config.syncStatePath();
    SyncStateManager state(statePath);
    if (!state.load()) {
        err << "Error: no se pudo leer " << statePath << "\n";
        return 1;
    }

    QString oldBase = QDir::cleanPath(oldBaseArg);
    if (oldBaseArg.isEmpty()) {
        oldBase = state.detectPreviousBasePath(newBase);
        if (oldBase.isEmpty()) {
            out << "No hay rutas que migrar: el indice ya cuelga de " << newBase << "\n"
                << "(si hubiera mas de una base anterior, indicala con --old-base).\n";
            reconcileSplitCourses(state, newBase, apply, out);
            return 0;
        }
        out << "Ruta base anterior deducida del indice: " << oldBase << "\n";
    }

    out << "Indice:      " << statePath << "\n"
        << "Base actual: " << newBase << "\n"
        << "Base previa: " << oldBase << "\n"
        << (apply ? "Modo:        APLICAR\n" : "Modo:        simulacion (dry-run). Añade --apply para escribir.\n")
        << "\n";

    const PathRebasePlan preview = state.rebasePaths(oldBase, newBase, false);
    if (preview.entries.isEmpty()) {
        out << "Nada que migrar.\n";
        reconcileSplitCourses(state, newBase, apply, out);
        return 0;
    }

    QString currentCourse;
    for (const PathRebaseEntry &entry : preview.entries) {
        if (entry.courseId != currentCourse) {
            currentCourse = entry.courseId;
            out << "Materia " << entry.courseId << "\n";
        }
        if (entry.alreadyMigrated) {
            continue;
        }
        out << "  [" << entry.field << "] " << entry.context << "\n"
            << "    " << entry.oldPath << "\n"
            << " -> " << entry.newPath
            << (entry.existsAtNewPath ? "  (existe en disco)" : "  (AVISO: no existe en disco)") << "\n";
    }

    out << "\nResumen: " << preview.toMigrate() << " rutas a reescribir, "
        << preview.alreadyMigrated() << " ya migradas, "
        << preview.missingAfterMigration() << " seguiran sin existir en disco.\n";

    if (!apply) {
        // Se aplica el rebase SOLO en memoria (nunca se llama a save()) para que la
        // deteccion de materias partidas trabaje sobre las rutas que tendria el
        // indice ya migrado; si no, el plan no las mostraria.
        state.rebasePaths(oldBase, newBase, true);
        reconcileSplitCourses(state, newBase, false, out);
        out << "\nSimulacion: no se ha escrito nada. Repite con --apply para aplicarlo.\n";
        return 0;
    }

    const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"));
    // La marca tiene resolucion de segundo: dos ejecuciones seguidas chocaban y
    // QFile::copy fallaba, abortando la migracion. Se busca un nombre libre.
    QString backupPath = statePath + QStringLiteral(".bak.") + stamp;
    for (int suffix = 2; QFileInfo::exists(backupPath); ++suffix) {
        backupPath = QStringLiteral("%1.bak.%2-%3").arg(statePath, stamp).arg(suffix);
    }
    if (!QFile::copy(statePath, backupPath)) {
        err << "Error: no se pudo crear el backup " << backupPath << ". No se aplica nada.\n";
        return 1;
    }
    out << "\nBackup creado: " << backupPath << "\n";

    state.rebasePaths(oldBase, newBase, true);
    if (!state.save()) {
        err << "Error: no se pudo guardar " << statePath << ". El backup sigue intacto.\n";
        return 1;
    }

    out << "Indice actualizado.\n";
    reconcileSplitCourses(state, newBase, true, out);
    out << "\nPara revertir:\n  cp \"" << backupPath << "\" \"" << statePath << "\"\n";
    return 0;
}

} // namespace

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
            if (QLatin1String(argv[i]) == QLatin1String("--cli-sync")
                || QLatin1String(argv[i]) == QLatin1String("--migrate-paths")) {
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
    QCommandLineOption migratePathsOption(
        QStringList{QStringLiteral("migrate-paths")},
        QStringLiteral("Reescribe en sync_state.json las rutas absolutas de una ruta base anterior. "
                       "Sin --apply solo imprime el plan."));
    QCommandLineOption migrateApplyOption(
        QStringList{QStringLiteral("apply")},
        QStringLiteral("Aplica la migracion en vez de limitarse a imprimir el plan."));
    QCommandLineOption oldBaseOption(
        QStringList{QStringLiteral("old-base")},
        QStringLiteral("Ruta base anterior (se deduce del indice si se omite)."),
        QStringLiteral("path"));
    QCommandLineOption newBaseOption(
        QStringList{QStringLiteral("new-base")},
        QStringLiteral("Ruta base nueva (por defecto, la de config.json)."),
        QStringLiteral("path"));
    QCommandLineOption cliDownloadAttachmentsOption(
        QStringList{QStringLiteral("cli-download-attachments")},
        QStringLiteral("Descarga adjuntos en modo CLI despues de sincronizar carpetas."));

    parser.addOption(cliSyncOption);
    parser.addOption(basePathOption);
    parser.addOption(samplePathOption);
    parser.addOption(cliDownloadAttachmentsOption);
    parser.addOption(migratePathsOption);
    parser.addOption(migrateApplyOption);
    parser.addOption(oldBaseOption);
    parser.addOption(newBaseOption);
    parser.process(app);

    if (parser.isSet(migratePathsOption)) {
        return runPathMigration(
            parser.value(oldBaseOption).trimmed(),
            parser.value(newBaseOption).trimmed(),
            parser.isSet(migrateApplyOption));
    }

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
