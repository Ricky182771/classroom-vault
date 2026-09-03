#pragma once

#include <QString>

// Los dos centinelas del modelo de semestres. Vivian duplicados como literales
// en ocho ficheros, asi que una comparacion podia quedarse desalineada del resto
// sin que nada lo delatara.
//
// El semestre sigue siendo su nombre visible ("Semestre 3"): es la clave que ya
// esta escrita en config.json y en las rutas en disco. Lo que se centraliza aqui
// es su normalizacion, para que toda comparacion use el mismo criterio.
namespace Semester {

// "Ninguna asignacion". Nunca es un destino de escritura valido.
inline QString none()
{
    return QStringLiteral("Sin semestre");
}

// Valor del filtro de vista que significa "no filtres". Nunca es un semestre.
inline QString all()
{
    return QStringLiteral("Todos los semestres");
}

inline bool isSentinel(const QString &semester)
{
    const QString clean = semester.trimmed();
    return clean.isEmpty() || clean == none() || clean == all();
}

// Todo semestre entra al modelo por aqui: recortado, con el vacio colapsado al
// centinela.
inline QString normalize(const QString &semester)
{
    const QString clean = semester.trimmed();
    return clean.isEmpty() ? none() : clean;
}

} // namespace Semester
