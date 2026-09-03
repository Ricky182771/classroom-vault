// Pruebas de regresion del filtro de semestres y del pipeline de sincronizacion.
//
// Cada prueba corre contra un XDG_CONFIG_HOME / XDG_CACHE_HOME propio en un
// directorio temporal, asi que ni config.json ni sync_state.json reales se tocan.
// No hay red: los cursos entran por loadSampleData() desde un fixture generado
// aqui mismo (12 materias, S2 archivado, S3 activo), que es el escenario del bug.

#include "ConfigManager.hpp"
#include "Semester.hpp"
#include "SyncManager.hpp"
#include "SyncStateManager.hpp"
#include "ui/CourseGridWidget.hpp"
#include "ui/UiModels.hpp"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

namespace {

constexpr int kCourseCount = 12;

QString courseId(int index)
{
    return QStringLiteral("curso-%1").arg(index, 2, 10, QLatin1Char('0'));
}

QString courseName(int index)
{
    return QStringLiteral("IA 3A Materia %1").arg(index, 2, 10, QLatin1Char('0'));
}

// Fixture con la misma forma que sample_classroom_data.json.
QString writeFixture(const QString &dirPath)
{
    QJsonArray courses;
    QJsonObject courseWork;

    for (int i = 0; i < kCourseCount; ++i) {
        QJsonObject course;
        course.insert(QStringLiteral("id"), courseId(i));
        course.insert(QStringLiteral("name"), courseName(i));
        course.insert(QStringLiteral("section"), QStringLiteral("A"));
        course.insert(QStringLiteral("alternateLink"), QStringLiteral("https://classroom.google.com/c/%1").arg(courseId(i)));
        courses.append(course);

        QJsonObject dueDate;
        dueDate.insert(QStringLiteral("year"), 2026);
        dueDate.insert(QStringLiteral("month"), 9);
        dueDate.insert(QStringLiteral("day"), 15);

        QJsonObject work;
        work.insert(QStringLiteral("id"), QStringLiteral("tarea-%1").arg(i));
        work.insert(QStringLiteral("title"), QStringLiteral("Tarea de %1").arg(courseName(i)));
        work.insert(QStringLiteral("description"), QStringLiteral("Descripcion de prueba"));
        work.insert(QStringLiteral("workType"), QStringLiteral("ASSIGNMENT"));
        work.insert(QStringLiteral("state"), QStringLiteral("PUBLISHED"));
        work.insert(QStringLiteral("alternateLink"), QStringLiteral("https://classroom.google.com/c/%1/a/1").arg(courseId(i)));
        work.insert(QStringLiteral("materials"), QJsonArray{});
        work.insert(QStringLiteral("dueDate"), dueDate);

        courseWork.insert(courseId(i), QJsonArray{work});
    }

    QJsonObject root;
    root.insert(QStringLiteral("courses"), courses);
    root.insert(QStringLiteral("courseWork"), courseWork);
    root.insert(QStringLiteral("announcements"), QJsonObject{});
    root.insert(QStringLiteral("courseWorkMaterials"), QJsonObject{});
    root.insert(QStringLiteral("studentSubmissions"), QJsonObject{});

    const QString path = QDir(dirPath).filePath(QStringLiteral("fixture_classroom.json"));
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return QString();
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    file.close();
    return path;
}

// Instantanea (ruta -> mtime) de un arbol, para probar "no se escribio nada".
QMap<QString, QDateTime> treeSnapshot(const QString &root)
{
    QMap<QString, QDateTime> snapshot;
    QDirIterator it(root, QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        snapshot.insert(it.filePath(), it.fileInfo().lastModified());
    }
    return snapshot;
}

} // namespace

class TestSemesterSync : public QObject {
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void newCoursesLandInTheActiveSemester();
    void archivedSemesterWritesNothingAndReportsOneSkip();
    void archivingDoesNotDragCoursesThatOnlyMatchedByFallback();
    void viewFilterNeverChangesTheWriteTarget();
    void countersAlwaysComeFromTheFilteredSet();
    void emptyGridSaysWhyItIsEmpty();
    void renamedCoursesTrappedInArchivedSemesterCanBeRescued();
    void rebuildRefusesToWipeTheIndexWhenItCannotRepopulateIt();
    void rebasePathsIsIdempotentAndPreservesHistory();

private:
    void makeSyncManager();
    // ClassroomClient entrega tambien los datos de prueba de forma asincrona, asi
    // que hay que esperar a assignmentsChanged antes de sincronizar carpetas.
    void loadFixtureAndWait();

    QTemporaryDir *m_home = nullptr;
    QString m_basePath;
    QString m_fixture;
    SyncManager *m_sync = nullptr;
};

void TestSemesterSync::init()
{
    m_home = new QTemporaryDir;
    QVERIFY(m_home->isValid());

    qputenv("XDG_CONFIG_HOME", QFile::encodeName(QDir(m_home->path()).filePath(QStringLiteral("config"))));
    qputenv("XDG_CACHE_HOME", QFile::encodeName(QDir(m_home->path()).filePath(QStringLiteral("cache"))));

    m_basePath = QDir(m_home->path()).filePath(QStringLiteral("datos"));
    QVERIFY(QDir().mkpath(m_basePath));

    m_fixture = writeFixture(m_home->path());
    QVERIFY(!m_fixture.isEmpty());

    m_sync = nullptr;
}

void TestSemesterSync::cleanup()
{
    delete m_sync;
    m_sync = nullptr;
    delete m_home;
    m_home = nullptr;
}

void TestSemesterSync::makeSyncManager()
{
    m_sync = new SyncManager;
    m_sync->setBasePath(m_basePath);
}

void TestSemesterSync::loadFixtureAndWait()
{
    QSignalSpy assignments(m_sync, &SyncManager::assignmentsChanged);
    m_sync->loadSampleData(m_fixture);
    QVERIFY(assignments.wait(5000));
    QCOMPARE(m_sync->courses().size(), kCourseCount);
}

// Caso 1: S2 archivado y S3 activo -> las materias aterrizan en S3 y S2 no se toca.
void TestSemesterSync::newCoursesLandInTheActiveSemester()
{
    makeSyncManager();

    // S2 existe en disco con contenido previo, como el respaldo del ciclo pasado.
    const QString s2 = QDir(m_basePath).filePath(QStringLiteral("Tareas/Semestre 2/Materia vieja"));
    QVERIFY(QDir().mkpath(s2));
    QFile marker(QDir(s2).filePath(QStringLiteral("metadata.json")));
    QVERIFY(marker.open(QIODevice::WriteOnly));
    marker.write("{}");
    marker.close();

    QVERIFY(m_sync->archiveSemester(QStringLiteral("Semestre 2")));
    m_sync->setDefaultSemester(QStringLiteral("Semestre 3"));
    QCOMPARE(m_sync->defaultSemester(), QStringLiteral("Semestre 3"));

    const QMap<QString, QDateTime> before = treeSnapshot(QDir(m_basePath).filePath(QStringLiteral("Tareas/Semestre 2")));

    loadFixtureAndWait();

    QSignalSpy finished(m_sync, &SyncManager::syncFinished);
    m_sync->syncFolders();
    QCOMPARE(finished.count(), 1);

    for (int i = 0; i < kCourseCount; ++i) {
        const QString expected =
            QDir(m_basePath).filePath(QStringLiteral("Tareas/Semestre 3/%1").arg(courseName(i)));
        QVERIFY2(QFileInfo::exists(expected), qPrintable(QStringLiteral("falta %1").arg(expected)));
        QCOMPARE(m_sync->semesterForCourse(courseId(i)), QStringLiteral("Semestre 3"));
    }

    QCOMPARE(treeSnapshot(QDir(m_basePath).filePath(QStringLiteral("Tareas/Semestre 2"))), before);
}

// Caso 3: con todo archivado no hay errores, no hay escrituras y hay UNA sola
// linea de omision.
void TestSemesterSync::archivedSemesterWritesNothingAndReportsOneSkip()
{
    makeSyncManager();
    m_sync->setDefaultSemester(QStringLiteral("Semestre 3"));
    loadFixtureAndWait();

    // Todas las materias mapeadas a S2 y S2 archivado despues.
    for (int i = 0; i < kCourseCount; ++i) {
        m_sync->setSemesterForCourse(courseId(i), QStringLiteral("Semestre 2"));
    }
    QVERIFY(m_sync->archiveSemester(QStringLiteral("Semestre 2")));

    const QString tasksRoot = QDir(m_basePath).filePath(QStringLiteral("Tareas"));
    QDir().mkpath(tasksRoot);
    const QMap<QString, QDateTime> before = treeSnapshot(tasksRoot);

    QStringList archLines;
    connect(m_sync, &SyncManager::logMessage, this, [&archLines](const QString &message) {
        if (message.startsWith(QStringLiteral("[ARCH]"))) {
            archLines.append(message);
        }
    });
    QSignalSpy errors(m_sync, &SyncManager::errorOccurred);
    QSignalSpy finished(m_sync, &SyncManager::syncFinished);

    m_sync->syncFolders();

    QCOMPARE(finished.count(), 1);
    QCOMPARE(finished.first().at(3).toInt(), 0); // errorCount
    QCOMPARE(errors.count(), 0);
    QCOMPARE(archLines.count(), 1);
    QVERIFY(archLines.first().contains(QStringLiteral("Semestre 2")));
    QCOMPARE(treeSnapshot(tasksRoot), before);
}

// Regresion de la causa raiz: archivar un semestre no puede arrastrar dentro a las
// materias que solo caian en el por el fallback a defaultSemester.
void TestSemesterSync::archivingDoesNotDragCoursesThatOnlyMatchedByFallback()
{
    makeSyncManager();
    loadFixtureAndWait();

    // Ninguna materia tiene mapeo explicito; todas resuelven por el fallback.
    m_sync->setDefaultSemester(QStringLiteral("Semestre 2"));
    for (int i = 0; i < kCourseCount; ++i) {
        QCOMPARE(m_sync->semesterForCourse(courseId(i)), QStringLiteral("Semestre 2"));
    }

    QVERIFY(m_sync->archiveSemester(QStringLiteral("Semestre 2")));

    ConfigManager config;
    QVERIFY(config.load());
    QCOMPARE(config.semesterMapping().size(), 0);
    QCOMPARE(config.defaultSemester(), QString());

    for (int i = 0; i < kCourseCount; ++i) {
        QVERIFY2(!m_sync->isCourseArchived(courseId(i)),
                 "una materia sin mapeo propio no puede quedar archivada");
    }

    // Y el daño equivalente ya escrito debe poder deshacerse: se reproduce el
    // estado corrupto (mapeo explicito a un semestre que luego se archiva) y se
    // comprueba que la materia puede salir de ahi.
    QVERIFY(m_sync->unarchiveSemester(QStringLiteral("Semestre 2")));
    m_sync->setSemesterForCourse(courseId(0), QStringLiteral("Semestre 2"));
    QVERIFY(m_sync->archiveSemester(QStringLiteral("Semestre 2")));
    QVERIFY(m_sync->isCourseArchived(courseId(0)));

    m_sync->setSemesterForCourse(courseId(0), QStringLiteral("Semestre 3"));
    QCOMPARE(m_sync->semesterForCourse(courseId(0)), QStringLiteral("Semestre 3"));
    QVERIFY(!m_sync->isCourseArchived(courseId(0)));

    // Y meterla dentro de un semestre archivado sigue prohibido.
    m_sync->setSemesterForCourse(courseId(1), QStringLiteral("Semestre 2"));
    QVERIFY(!m_sync->isCourseArchived(courseId(1)));

    QVERIFY(m_sync->unarchiveSemester(QStringLiteral("Semestre 2")));
    QVERIFY(!m_sync->isSemesterArchived(QStringLiteral("Semestre 2")));
}

// Caso 4: cambiar el filtro de vista no puede mover el destino de escritura. Con
// el pipeline actual el sync no es interrumpible desde una prueba, asi que se
// verifica la invariante que lo garantiza en vez de simular una carrera.
void TestSemesterSync::viewFilterNeverChangesTheWriteTarget()
{
    makeSyncManager();
    loadFixtureAndWait();
    m_sync->setDefaultSemester(QStringLiteral("Semestre 3"));

    const QString target = m_sync->defaultSemester();
    QStringList resolvedBefore;
    for (int i = 0; i < kCourseCount; ++i) {
        resolvedBefore.append(m_sync->semesterForCourse(courseId(i)));
    }

    for (const QString &filter : {QStringLiteral("Semestre 1"), Semester::all(), QStringLiteral("Semestre 2"), Semester::none()}) {
        m_sync->setGlobalSemesterFilter(filter);
        QCOMPARE(m_sync->globalSemesterFilter(), filter);
        QCOMPARE(m_sync->defaultSemester(), target);
    }

    QStringList resolvedAfter;
    for (int i = 0; i < kCourseCount; ++i) {
        resolvedAfter.append(m_sync->semesterForCourse(courseId(i)));
    }
    QCOMPARE(resolvedAfter, resolvedBefore);

    // Y el sync encolado despues sigue aterrizando donde dice el destino.
    m_sync->syncFolders();
    QVERIFY(QFileInfo::exists(QDir(m_basePath).filePath(QStringLiteral("Tareas/Semestre 3/%1").arg(courseName(0)))));
}

// Caso 2: los cinco contadores salen del mismo conjunto filtrado.
void TestSemesterSync::countersAlwaysComeFromTheFilteredSet()
{
    QVector<CourseUiState> s3;
    for (int i = 0; i < kCourseCount; ++i) {
        CourseUiState course;
        course.id = courseId(i);
        course.semester = QStringLiteral("Semestre 3");
        course.totalTasks = 2;
        course.backedUpTasks = 1;
        course.attachments = 3;
        course.pending = 1;
        course.errors = i == 0 ? 1 : 0;
        s3.append(course);
    }

    const VaultStats full = aggregateCourseStats(s3);
    QCOMPARE(full.courses, kCourseCount);
    QCOMPARE(full.tasks, 2 * kCourseCount);
    QCOMPARE(full.attachments, 3 * kCourseCount);
    QCOMPARE(full.pending, kCourseCount);
    QCOMPARE(full.errors, 1);

    // Un semestre sin materias pone TODOS los contadores a cero, no solo tres.
    const VaultStats empty = aggregateCourseStats({});
    QCOMPARE(empty.courses, 0);
    QCOMPARE(empty.tasks, 0);
    QCOMPARE(empty.attachments, 0);
    QCOMPARE(empty.pending, 0);
    QCOMPARE(empty.errors, 0);

    // Y volver al conjunto anterior restituye exactamente las mismas cifras.
    const VaultStats back = aggregateCourseStats(s3);
    QCOMPARE(back.courses, full.courses);
    QCOMPARE(back.tasks, full.tasks);
    QCOMPARE(back.attachments, full.attachments);
    QCOMPARE(back.pending, full.pending);
    QCOMPARE(back.errors, full.errors);
}

// Caso 5: una grilla vacia debe decir por que lo esta.
void TestSemesterSync::emptyGridSaysWhyItIsEmpty()
{
    const auto visibleText = [](const CourseGridWidget &grid) {
        QString text;
        const QList<QLabel *> labels = grid.findChildren<QLabel *>();
        for (const QLabel *label : labels) {
            text += label->text();
        }
        return text;
    };

    CourseGridWidget grid;
    grid.resize(900, 600);

    grid.setSemesterContext(QStringLiteral("Semestre 3"));
    grid.setCourses({});
    QCoreApplication::processEvents();
    const QString emptySemester = visibleText(grid);
    QVERIFY2(emptySemester.contains(QStringLiteral("Semestre 3")),
             qPrintable(QStringLiteral("estado vacio sin contexto: %1").arg(emptySemester)));

    CourseUiState course;
    course.id = courseId(0);
    course.name = courseName(0);
    course.semester = QStringLiteral("Semestre 3");
    course.status = QStringLiteral("complete");
    grid.setCourses({course});
    grid.setSearchText(QStringLiteral("no-existe-esta-materia"));
    QCoreApplication::processEvents();
    const QString noMatch = visibleText(grid);

    // Los dos vacios deben ser distinguibles: es justo lo que no lo era.
    QVERIFY(!noMatch.isEmpty());
    QVERIFY(noMatch != emptySemester);
    QVERIFY(noMatch.contains(QStringLiteral("filtro")) || noMatch.contains(QStringLiteral("busqueda")));
}

// El caso real: la institucion reutiliza el mismo curso de Classroom cada ciclo y
// solo lo renombra. Sus ids quedaron mapeados a un semestre que despues se
// archivo, asi que Classroom los sigue devolviendo pero ningun sync los respalda,
// para siempre y sin un solo error. Deben poder rescatarse, y el respaldo nuevo
// tiene que ir al semestre activo, no a la carpeta archivada que ya tenian.
void TestSemesterSync::renamedCoursesTrappedInArchivedSemesterCanBeRescued()
{
    makeSyncManager();
    loadFixtureAndWait();

    // Ciclo anterior: las materias viven en S2 y se respaldan ahi.
    m_sync->setDefaultSemester(QStringLiteral("Semestre 2"));
    for (int i = 0; i < kCourseCount; ++i) {
        m_sync->setSemesterForCourse(courseId(i), QStringLiteral("Semestre 2"));
    }
    m_sync->syncFolders();
    const QString oldFolder =
        QDir(m_basePath).filePath(QStringLiteral("Tareas/Semestre 2/%1").arg(courseName(0)));
    QVERIFY(QFileInfo::exists(oldFolder));

    // Fin de ciclo: se archiva S2 y el destino pasa a ser S3.
    QVERIFY(m_sync->archiveSemester(QStringLiteral("Semestre 2")));
    m_sync->setDefaultSemester(QStringLiteral("Semestre 3"));

    // Ciclo nuevo: Classroom devuelve LOS MISMOS ids (renombrados). Hoy quedaban
    // atrapados: excluidos del alcance en cada sync.
    loadFixtureAndWait();
    QCOMPARE(m_sync->coursesTrappedInArchivedSemester().size(), kCourseCount);

    QSignalSpy finished(m_sync, &SyncManager::syncFinished);
    m_sync->syncFolders();
    QCOMPARE(finished.count(), 1);
    QVERIFY2(!QFileInfo::exists(QDir(m_basePath).filePath(QStringLiteral("Tareas/Semestre 3/%1").arg(courseName(0)))),
             "sin rescatar, la materia no debe respaldarse en ningun sitio");

    // Rescate.
    QCOMPARE(m_sync->releaseCoursesFromArchivedSemester(QStringLiteral("Semestre 3")), kCourseCount);
    QCOMPARE(m_sync->coursesTrappedInArchivedSemester().size(), 0);

    const QMap<QString, QDateTime> archivedBefore =
        treeSnapshot(QDir(m_basePath).filePath(QStringLiteral("Tareas/Semestre 2")));

    m_sync->syncFolders();

    for (int i = 0; i < kCourseCount; ++i) {
        const QString expected =
            QDir(m_basePath).filePath(QStringLiteral("Tareas/Semestre 3/%1").arg(courseName(i)));
        QVERIFY2(QFileInfo::exists(expected), qPrintable(QStringLiteral("falta %1").arg(expected)));
    }

    // Y el respaldo archivado del ciclo anterior no se movio ni se toco.
    QCOMPARE(treeSnapshot(QDir(m_basePath).filePath(QStringLiteral("Tareas/Semestre 2"))), archivedBefore);
    QVERIFY(QFileInfo::exists(oldFolder));

    // Un destino archivado nunca es valido como rescate.
    QCOMPARE(m_sync->releaseCoursesFromArchivedSemester(QStringLiteral("Semestre 2")), 0);
}

// "Reconstruir indice local" vaciaba el indice y solo despues intentaba cargar
// Classroom. Con un refresh token pero sin clientId/clientSecret la carga fallaba
// y el indice se quedaba truncado, sin mas rastro que un ERR suelto.
void TestSemesterSync::rebuildRefusesToWipeTheIndexWhenItCannotRepopulateIt()
{
    makeSyncManager();
    loadFixtureAndWait();
    m_sync->setDefaultSemester(QStringLiteral("Semestre 3"));
    m_sync->syncFolders();

    const QString statePath = m_sync->configManager().syncStatePath();
    QVERIFY(QFileInfo::exists(statePath));

    QFile before(statePath);
    QVERIFY(before.open(QIODevice::ReadOnly));
    const QByteArray contentBefore = before.readAll();
    before.close();
    QVERIFY(!contentBefore.isEmpty());

    // Sin OAuth configurado no puede repoblar: debe negarse, no vaciar.
    QVERIFY(!m_sync->rebuildLocalIndex());

    QFile after(statePath);
    QVERIFY(after.open(QIODevice::ReadOnly));
    QCOMPARE(after.readAll(), contentBefore);
    after.close();

    // Y no puede haber dejado backups sueltos de un vaciado que no ocurrio.
    const QStringList backups =
        QDir(QFileInfo(statePath).absolutePath()).entryList({QStringLiteral("sync_state.json.bak.*")}, QDir::Files);
    QCOMPARE(backups.size(), 0);
}

// Fase 2: la migracion de rutas es idempotente y no pierde historial.
void TestSemesterSync::rebasePathsIsIdempotentAndPreservesHistory()
{
    const QString statePath = QDir(m_home->path()).filePath(QStringLiteral("sync_state.json"));

    QJsonObject attachment;
    attachment.insert(QStringLiteral("localPath"), QStringLiteral("/base/vieja/Tareas/Semestre 2/M/T/Adjuntos/f.pdf"));
    attachment.insert(QStringLiteral("lastDownloaded"), QStringLiteral("2026-06-28T02:36:24Z"));

    QJsonObject archivalStatus;
    archivalStatus.insert(QStringLiteral("status"), QStringLiteral("deleted_archived"));

    QJsonObject assignment;
    assignment.insert(QStringLiteral("folderPath"), QStringLiteral("/base/vieja/Tareas/Semestre 2/M/T"));
    assignment.insert(QStringLiteral("metadataPath"), QStringLiteral("/base/vieja/Tareas/Semestre 2/M/T/metadata.json"));
    assignment.insert(QStringLiteral("attachments"), QJsonObject{{QStringLiteral("f"), attachment}});
    assignment.insert(QStringLiteral("archivalStatus"), archivalStatus);
    assignment.insert(QStringLiteral("lastSeen"), QStringLiteral("2026-06-28T02:36:16Z"));

    QJsonObject course;
    course.insert(QStringLiteral("name"), QStringLiteral("M"));
    course.insert(QStringLiteral("folderPath"), QStringLiteral("/base/vieja/Tareas/Semestre 2/M"));
    course.insert(QStringLiteral("semester"), QStringLiteral("Semestre 2"));
    course.insert(QStringLiteral("lastSeen"), QStringLiteral("2026-06-28T02:36:16Z"));
    course.insert(QStringLiteral("assignments"), QJsonObject{{QStringLiteral("t"), assignment}});

    QJsonObject root;
    root.insert(QStringLiteral("courses"), QJsonObject{{QStringLiteral("c1"), course}});
    root.insert(QStringLiteral("lastSync"), QStringLiteral("2026-09-03T03:52:10Z"));

    QFile file(statePath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(QJsonDocument(root).toJson());
    file.close();

    SyncStateManager state(statePath);
    QVERIFY(state.load());

    QCOMPARE(state.detectPreviousBasePath(QStringLiteral("/base/nueva")), QStringLiteral("/base/vieja"));

    const PathRebasePlan dryRun = state.rebasePaths(QStringLiteral("/base/vieja"), QStringLiteral("/base/nueva"), false);
    QCOMPARE(dryRun.toMigrate(), 4);
    QCOMPARE(state.courseFolderPath(QStringLiteral("c1")),
             QStringLiteral("/base/vieja/Tareas/Semestre 2/M")); // el dry-run no toca nada

    const PathRebasePlan applied = state.rebasePaths(QStringLiteral("/base/vieja"), QStringLiteral("/base/nueva"), true);
    QCOMPARE(applied.toMigrate(), 4);
    QCOMPARE(state.courseFolderPath(QStringLiteral("c1")),
             QStringLiteral("/base/nueva/Tareas/Semestre 2/M"));

    // Idempotencia: la segunda pasada no reescribe nada.
    const PathRebasePlan again = state.rebasePaths(QStringLiteral("/base/vieja"), QStringLiteral("/base/nueva"), true);
    QCOMPARE(again.toMigrate(), 0);
    QCOMPARE(again.alreadyMigrated(), 4);

    // Historial y evidencia de archivado intactos.
    const QJsonObject migrated = state.assignmentState(QStringLiteral("c1"), QStringLiteral("t"));
    QCOMPARE(migrated.value(QStringLiteral("lastSeen")).toString(), QStringLiteral("2026-06-28T02:36:16Z"));
    QCOMPARE(migrated.value(QStringLiteral("archivalStatus")).toObject().value(QStringLiteral("status")).toString(),
             QStringLiteral("deleted_archived"));
    QCOMPARE(state.assignmentAttachments(QStringLiteral("c1"), QStringLiteral("t"))
                 .value(QStringLiteral("f")).toObject()
                 .value(QStringLiteral("lastDownloaded")).toString(),
             QStringLiteral("2026-06-28T02:36:24Z"));
    QCOMPARE(state.lastSync(), QStringLiteral("2026-09-03T03:52:10Z"));
}

QTEST_MAIN(TestSemesterSync)
#include "tst_semester_sync.moc"
