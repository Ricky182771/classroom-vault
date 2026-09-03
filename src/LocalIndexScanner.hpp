#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>

// Una carpeta de materia encontrada en disco, ya traducida al esquema de
// sync_state.json.
struct ScannedCourse {
    QString uid;         // identidad local de la carpeta (ver CourseFolderMarker)
    QString courseId;    // id de Classroom, si el marcador o los metadatos lo saben
    QString stateKey;    // clave con la que entra en sync_state.courses
    QString name;
    QString semester;
    QString folderPath;
    QJsonObject state;   // objeto listo para SyncStateManager::setCourseStateRaw
    int assignments = 0;
    int publications = 0;
    int attachments = 0;
    bool markerCreated = false;
};

struct LocalScanResult {
    QVector<ScannedCourse> courses;
    QStringList warnings;
    int semesterFolders = 0;
    int courseFolders = 0;

    int assignments() const;
    int publications() const;
    int attachments() const;
};

// Reconstruye el indice a partir de lo que hay en disco, sin preguntarle nada a
// Classroom. Es posible porque cada tarea y cada publicacion respaldada lleva su
// metadata.json al lado: el arbol es la fuente de verdad, no una copia opaca.
namespace LocalIndexScanner {

// archivedSemesters solo se usa para decidir quien se queda con la clave
// "limpia" cuando dos carpetas de semestres distintos comparten courseId: la
// materia activa conserva el courseId y la congelada pasa a su uid.
LocalScanResult scan(
    const QString &basePath,
    const QStringList &archivedSemesters,
    bool createMissingMarkers);

} // namespace LocalIndexScanner
