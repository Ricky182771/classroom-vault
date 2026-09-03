#pragma once

#include <QJsonObject>
#include <QString>

// Identidad local y estable de una carpeta de materia.
//
// El id de Classroom no sirve como identidad del respaldo: la institucion
// reutiliza el mismo curso cada ciclo y solo lo renombra, asi que un unico
// courseId puede tener respaldo en dos semestres a la vez (uno congelado y uno
// activo). El marcador da a cada carpeta un UID propio, generado la primera vez
// que se crea, que ya no depende de lo que Classroom haga con el curso.
//
// Vive como fichero oculto dentro de la carpeta de la materia, de modo que el
// arbol en disco es autodescriptivo: si se pierde el indice, la reconstruccion
// por escaneo puede rehacerlo sin preguntarle nada a Classroom.
namespace CourseFolderMarker {

QString fileName();

// Lee el marcador. Objeto vacio si no existe o no es legible.
QJsonObject read(const QString &coursePath);

// Devuelve el UID de la carpeta, creando el marcador si aun no lo tiene. El UID
// existente nunca se sobrescribe: es la identidad del respaldo.
QString ensure(
    const QString &coursePath,
    const QString &courseId,
    const QString &courseName,
    const QString &semester);

QString uid(const QString &coursePath);
QString courseId(const QString &coursePath);
QString courseName(const QString &coursePath);

} // namespace CourseFolderMarker
