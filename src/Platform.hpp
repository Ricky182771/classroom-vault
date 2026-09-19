#pragma once

#include <QString>

// Apertura de rutas locales en el gestor de archivos del escritorio.
//
// Existe porque QDesktopServices::openUrl delega en xdg-open, que resuelve
// inode/directory por asociacion MIME: si el sistema no tiene un default de
// usuario y el gestor que figura en el default del sistema no esta instalado,
// gana el primer .desktop de mimeinfo.cache que se declare handler de
// directorios. Un emulador de terminal puede hacerlo (kitty-open.desktop lo
// hace), y entonces "Abrir carpeta" abre una terminal.
namespace Platform {

// Abre `path` como carpeta en el gestor de archivos.
void openFolder(const QString &path);

// Abre la carpeta contenedora de `path` y, si el gestor lo soporta, deja el
// archivo seleccionado dentro de ella.
void revealPath(const QString &path);

} // namespace Platform
