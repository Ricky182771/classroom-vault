#include "Platform.hpp"

#include <QDesktopServices>
#include <QFileInfo>
#include <QUrl>

#if defined(Q_OS_LINUX) && defined(CV_HAVE_QTDBUS)
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QStringList>
#endif

#if defined(Q_OS_WIN)
#include <QDir>
#include <QProcess>
#include <QStandardPaths>
#endif

namespace {

void openWithDesktopServices(const QString &path)
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

#if defined(Q_OS_LINUX) && defined(CV_HAVE_QTDBUS)

// org.freedesktop.FileManager1 solo la implementan gestores de archivos
// (Dolphin, Nautilus, Thunar, Nemo), asi que la peticion no puede terminar en
// otra clase de programa. Es la diferencia con xdg-open, que entrega la carpeta
// a quien gane la asociacion MIME.
//
// `fallbackPath` es la ruta que se abre por la via generica si el bus no
// responde o ningun gestor esta registrado.
bool showInFileManager(const QString &method, const QString &uri, const QString &fallbackPath)
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        return false;
    }

    QDBusMessage request = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.FileManager1"),
        QStringLiteral("/org/freedesktop/FileManager1"),
        QStringLiteral("org.freedesktop.FileManager1"),
        method);
    request << QStringList{uri} << QString();

    // Asincrono a proposito: la llamada activa el gestor de archivos por DBus y
    // no contesta hasta que este termina de arrancar. En frio eso congelaria la
    // ventana varios segundos justo al pulsar el boton.
    auto *watcher = new QDBusPendingCallWatcher(bus.asyncCall(request));
    QObject::connect(watcher, &QDBusPendingCallWatcher::finished, watcher,
                     [fallbackPath](QDBusPendingCallWatcher *self) {
                         if (self->isError()) {
                             openWithDesktopServices(fallbackPath);
                         }
                         self->deleteLater();
                     });
    return true;
}

#endif

#if defined(Q_OS_WIN)

// explorer.exe es la unica via para dejar el archivo seleccionado dentro de su
// carpeta. QDesktopServices acaba en ShellExecute sobre la ruta, que abriria el
// archivo con su programa asociado en vez de mostrarlo en el explorador.
//
// La coma de "/select," es parte del argumento, no un separador: explorer la
// exige pegada al conmutador. Va como argumento propio porque QProcess
// entrecomilla cada uno por separado, y asi una ruta con espacios llega entera
// en vez de partirse en dos.
bool revealWithExplorer(const QString &path)
{
    const QString explorer = QStandardPaths::findExecutable(QStringLiteral("explorer.exe"));
    if (explorer.isEmpty()) {
        return false;
    }

    return QProcess::startDetached(explorer,
                                   QStringList{QStringLiteral("/select,"),
                                               QDir::toNativeSeparators(path)});
}

#endif

} // namespace

namespace Platform {

void openFolder(const QString &path)
{
    const QString clean = path.trimmed();
    if (clean.isEmpty()) {
        return;
    }

#if defined(Q_OS_LINUX) && defined(CV_HAVE_QTDBUS)
    if (showInFileManager(QStringLiteral("ShowFolders"),
                          QUrl::fromLocalFile(clean).toString(),
                          clean)) {
        return;
    }
#endif

    openWithDesktopServices(clean);
}

void revealPath(const QString &path)
{
    const QString clean = path.trimmed();
    if (clean.isEmpty()) {
        return;
    }

    const QString parent = QFileInfo(clean).absolutePath();
    const QString folder = parent.isEmpty() ? clean : parent;

#if defined(Q_OS_LINUX) && defined(CV_HAVE_QTDBUS)
    if (showInFileManager(QStringLiteral("ShowItems"),
                          QUrl::fromLocalFile(clean).toString(),
                          folder)) {
        return;
    }
#elif defined(Q_OS_WIN)
    if (revealWithExplorer(clean)) {
        return;
    }
#endif

    openWithDesktopServices(folder);
}

} // namespace Platform
