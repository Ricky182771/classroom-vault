# Classroom Vault / TareaSync

Aplicación de escritorio en **C++20 + Qt6** para sincronizar Google Classroom y organizar tareas por semestre/materia, incluyendo adjuntos, dentro del mismo flujo de sincronización.

> Respaldo histórico local de tus tareas de Classroom — con protección contra pérdida accidental de evidencia académica.

---

## Índice

- [Estado actual](#estado-actual)
- [Instalación y compilación](#instalación-y-compilación)
- [Uso](#uso)
- [Configuración](#configuración)
- [Seguridad y protección de datos](#seguridad-y-protección-de-datos)
- [Sincronización (detalle técnico)](#sincronización-detalle-técnico)
- [Troubleshooting](#troubleshooting)
- [Limitaciones actuales](#limitaciones-actuales)

---

## Estado actual

- OAuth 2.0 para app de escritorio con **login automático por navegador + loopback local** (`127.0.0.1`).
- Lectura de cursos activos y tareas desde Classroom API.
- Respaldo de **publicaciones del curso** (anuncios y materiales de clase) en `_Publicaciones/`.
- Creación y actualización de carpetas, `metadata.json` y `descripcion.md`.
- Estado persistente en `sync_state.json` para evitar duplicados.
- Restauración de estado local al iniciar (cursos/tareas desde `sync_state.json`).
- Carga automática de Classroom al abrir si existe sesión válida (sin abrir navegador automáticamente).
- Selector global de semestre (Todos/Sin semestre/Semestre 1..6) y selector manual por materia.
- Reconstrucción segura de índice local con backup `.bak` de `sync_state.json`.
- Sincronización por **staging de metadata fresca** antes de tocar el estado persistente.
- `Sincronizar materia` sincroniza solo el `courseId` seleccionado (no ejecuta sync global).
- Interfaz Qt Widgets modular en modo oscuro, con flujo jerárquico: **Inicio → Materia → Tarea**.

<details>
<summary><strong>Descarga de adjuntos (fase actual)</strong></summary>

- `driveFile`: metadata + descarga binaria.
- Google Docs/Sheets/Slides/Drawings: exportación.
- Links/YouTube/Forms: guardado como `.url`.

### Exportaciones Workspace

| Tipo de Google Workspace | Se exporta como |
|---|---|
| Docs (`document`) | `.docx` |
| Sheets (`spreadsheet`) | `.xlsx` |
| Slides (`presentation`) | `.pptx` |
| Drawings (`drawing`) | `.png` |
| Forms (`form`) | `.url` (enlace, no hay export binario) |
| Otros no exportables | `.url` con `webViewLink` cuando exista |

</details>

---

## Instalación y compilación

### Dependencias (Fedora)

```bash
sudo dnf install qt6-qtbase-devel cmake gcc-c++ ninja-build
```

### Compilar (desarrollo local — Linux)

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/classroom-vault
```

### Instalar en Fedora/Linux

```bash
sudo cmake --install build
# o equivalente:
cd build && sudo ninja install
```

Después de instalar, el ejecutable queda disponible como `classroom-vault` y aparece una entrada de menú **Classroom Vault** (KDE/GNOME).

<details>
<summary><strong>Desinstalación</strong></summary>

Si compilaste con Ninja:

```bash
cd build
sudo ninja uninstall
```

</details>

<details>
<summary><strong>Compilar en Windows</strong></summary>

**Prerrequisitos:**

- [Qt 6.6+](https://www.qt.io/download) — seleccionar el componente **MSVC 2022 64-bit**.
- [Visual Studio 2022](https://visualstudio.microsoft.com/) con la carga de trabajo **Desktop development with C++**.
- [CMake 3.20+](https://cmake.org/download/) y [Ninja](https://ninja-build.org/).

```powershell
# PowerShell — ajustar la ruta a Qt según tu instalación
cmake -S . -B build -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_PREFIX_PATH="C:\Qt\6.6.0\msvc2022_64"
cmake --build build
.\build\classroom-vault.exe
```

**Archivos de usuario en Windows:**

| Tipo | Ruta |
|---|---|
| Configuración (`config.json`, `token.json`) | `%APPDATA%\ClassroomVault\` |
| Cache / staging | `%LOCALAPPDATA%\ClassroomVault\` |

**Distribución (ZIP portable):**

Después de compilar, ejecuta `windeployqt` (incluido en Qt) para copiar las DLLs de Qt junto al ejecutable. Si CMake encontró `windeployqt` en el `PATH`, esto ocurre automáticamente en cada build de desarrollo.

```powershell
windeployqt --no-translations --no-quick-import .\build\classroom-vault.exe
# Comprimir la carpeta build\ como ZIP y distribuir.
```

**Long paths (opcional):** si los paths completos de tus tareas superan 260 caracteres, habilita la política de paths largos en Windows 10/11:

```
HKLM\SYSTEM\CurrentControlSet\Control\FileSystem\LongPathsEnabled = 1
```

</details>

---

## Uso

### Navegación UI

| Vista | Descripción |
|---|---|
| `Inicio` | Resumen de respaldo, KPIs, filtros y grid de materias. |
| `Materia` | Lista de tareas del curso seleccionado, estado de metadata/adjuntos y acciones. |
| `Tarea` | Previsualización local (título, descripción, fecha, estado, evidencia, adjuntos). |
| `Actividad` | Panel desplegable para logs recientes. |
| `Ruta` | Controles globales (cambiar ruta, abrir respaldo, sincronizar todo). |

### Estados visuales de curso

- `Completo`: todas las tareas detectadas con respaldo local.
- `Pendiente`: faltan tareas por respaldar.
- `Error`: inconsistencias o errores detectados durante procesos previos.
- `Sin sync`: curso sin sincronización útil aún.

### Estado de entrega de tareas

- `Entregada` / `Expirada y entregada`: basado en `studentSubmissions` (`TURNED_IN`, `RETURNED`).
- `No entregada`: solo con evidencia confiable de no entrega (`NEW`, `CREATED`, `RECLAIMED_BY_STUDENT`) y tarea vencida.
- `Expirada` (neutral): cuando no hay evidencia confiable, para evitar marcar en rojo por inferencia.

<details>
<summary><strong>Modo CLI</strong></summary>

Sincronizar carpetas con sample local:

```bash
./classroom-vault --cli-sync --base-path /ruta/del/disco --sample ../sample_classroom_data.json
```

Sincronizar y luego descargar adjuntos en CLI:

```bash
./classroom-vault --cli-sync --cli-download-attachments --base-path /ruta/del/disco --sample ../sample_classroom_data.json
```

En Windows:

```powershell
.\build\classroom-vault.exe --cli-sync --base-path "C:\Respaldo\Tareas" --sample sample_classroom_data.json
```

La app re-adjunta automáticamente stdout/stderr a la consola de PowerShell o cmd que la lanzó.

</details>

<details>
<summary><strong>Estructura de salida en disco</strong></summary>

```
Ruta base/
└── Tareas/
    └── Semestre N/
        └── Materia/
            ├── YYYY-MM-DD - Nombre de tarea/
            │   ├── metadata.json
            │   ├── descripcion.md
            │   ├── Adjuntos/
            │   │   ├── archivo.pdf
            │   │   ├── documento.docx
            │   │   └── Enlace - Referencia.url
            │   └── (archivos propios del usuario)
            ├── Sin fecha - Nombre de tarea/
            │   ├── metadata.json
            │   └── descripcion.md
            └── _Publicaciones/
                ├── Aviso - Primeras palabras del anuncio/
                │   ├── metadata.json
                │   └── descripcion.md
                └── Material - Titulo del material/
                    ├── metadata.json
                    └── descripcion.md
```

### Convenciones de nombres

| Patrón | Significado |
|---|---|
| `YYYY-MM-DD - Titulo` | Tarea con fecha de entrega. |
| `Sin fecha - Titulo` | Tarea sin `dueDate` en Classroom. |
| `Aviso - Texto` | Anuncio del curso (`announcements`). El nombre usa un extracto del texto, truncado. |
| `Material - Titulo` | Material de clase (`courseWorkMaterials`). |
| `... [123456]` | Sufijo de desambiguación cuando dos elementos generarían el mismo nombre de carpeta. |

### Archivos por carpeta

- `metadata.json` — fuente principal para la vista de detalle.
- `descripcion.md` — descripción del elemento en formato legible.
- `Adjuntos/` — solo se crea si el elemento tiene adjuntos.
- Cualquier otro archivo en la carpeta se considera **trabajo propio del usuario** y nunca se toca durante la sincronización.

</details>

<details>
<summary><strong>Columna "Tu trabajo"</strong></summary>

- En el detalle de tarea existe un panel lateral **Tu trabajo**.
- Lista archivos y carpetas propios dentro de `../Tarea/`.
- Ignora `metadata.json`, `descripcion.md` y `Adjuntos/`.
- Doble clic abre el archivo/carpeta local.

</details>

---

## Configuración

### APIs y scopes

**APIs necesarias** (habilitar en Google Cloud Console):
- Google Classroom API
- Google Drive API

**Scopes requeridos:**

| Scope | Para qué se usa |
|---|---|
| `classroom.courses.readonly` | Lectura de cursos activos. |
| `classroom.coursework.me.readonly` | Lectura de tareas. |
| `classroom.student-submissions.me.readonly` | Estado real de entrega (`Entregada`/`No entregada`). |
| `classroom.announcements.readonly` | Respaldo de publicaciones del curso. |
| `classroom.courseworkmaterials.readonly` | Materiales de clase asociados a los cursos. |
| `drive.readonly` | Descarga de adjuntos y exportación de Google Workspace. |

Prefijo completo: `https://www.googleapis.com/auth/`

### Archivos de configuración

| Archivo | Ruta |
|---|---|
| `config.json`, `token.json` | `~/.config/ClassroomVault/` |
| `sync_state.json` | `~/.config/ClassroomVault/` |
| Cache / staging | `~/.cache/ClassroomVault/` |

### Credenciales OAuth

Los builds oficiales incluyen credenciales embebidas, así que **no necesitas configurar nada** para usar la app. Si prefieres usar tu propio proyecto de Google Cloud (por cuota, privacidad o desarrollo), puedes sobrescribirlas.

Orden de prioridad:

1. `oauth.clientId` y `oauth.clientSecret` en `config.json`.
2. `oauth.credentialsFile` apuntando a un `credentials.json` de Google (tipo `installed`).
3. Credenciales embebidas en tiempo de compilación (fallback).

El origen efectivo se registra al iniciar sesión: `[AUTH] Credenciales cargadas (origen: ...)`.

<details>
<summary><strong>Compilar con credenciales propias embebidas</strong></summary>

```bash
export CV_OAUTH_CLIENT_ID="tu-client-id.apps.googleusercontent.com"
export CV_OAUTH_CLIENT_SECRET="tu-client-secret"
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Sin estas variables, el build no contiene ninguna credencial y la app requiere que el usuario configure las suyas en `config.json`.

</details>

> Nota: ninguno de estos archivos se instala con `cmake install` ni se sube al repositorio — están en `.gitignore`. Si usas tu propio `credentials.json`, colócalo manualmente en `~/.config/ClassroomVault/`.

<details>
<summary><strong>Autenticación OAuth automática (detalle del flujo)</strong></summary>

- Al presionar **Iniciar sesión**, la app levanta un servidor temporal local en `127.0.0.1` con puerto dinámico.
- La app abre el navegador del sistema y envía a Google OAuth.
- Google redirige a `http://127.0.0.1:<puerto>/callback?...`.
- Classroom Vault captura el `code` automáticamente y cierra el servidor local.
- Se muestra una página de éxito: *"Autorización completada. Ya puedes volver a Classroom Vault."*
- La app intercambia el `code` por `access_token` y `refresh_token`, y guarda `token.json`.
- En ejecuciones siguientes, si el token expiró y hay `refresh_token`, se refresca automáticamente sin abrir navegador.
- Si falla el refresco, se solicita iniciar sesión nuevamente.
- Timeout de autenticación: **180 segundos**.

**Comportamiento al abrir la app:**
1. Primero restaura el estado local desde `sync_state.json` (info disponible aun sin red).
2. Luego revisa la sesión guardada: token válido → carga Classroom automáticamente; token vencido con refresh token → intenta refrescar; sin sesión → queda en `No conectado`.
3. **No abre navegador automáticamente** si no hay sesión — solo al presionar **Iniciar sesión**.

**Fallback manual** (si no se puede iniciar `QTcpServer` local):
1. Abre el navegador.
2. Solicita pegar el `authorization code`.
3. Continúa con el intercambio de tokens.

</details>

---

## Seguridad y protección de datos

> Estas son las garantías clave que hacen de esta app una herramienta segura para respaldo histórico, no solo un sincronizador más.

<details open>
<summary><strong>Protección de evidencia académica</strong></summary>

Las tareas marcadas como **"Eliminada y archivada"** (`isArchivedDeleted`) son evidencia histórica protegida:

- Su `metadata.json`, `Adjuntos/` y archivos del usuario **nunca se sobrescriben ni borran automáticamente**.
- No son procesadas por `syncFolders()`, `downloadAttachments()` ni `onChecksumFailed()`.
- No se cargan en el estado activo en memoria al iniciar la app (`loadLocalStateIntoMemory` las omite).
- Solo se restauran explícitamente si Classroom vuelve a devolver el mismo `assignmentId` en un fetch completo y exitoso.
- Una tarea se marca como eliminada solo si el fetch del curso fue **completo** (`fetchStatus: complete`). Un fetch incompleto nunca genera archivaciones.
- El `.archived_deleted.json` dentro de la carpeta de tarea es un marcador adicional de protección.

Logs de protección visibles en la app: `[ARCH]`

</details>

<details>
<summary><strong>Sesión de Google</strong></summary>

- `token.json` y `credentials.json` se guardan en `~/.config/ClassroomVault/` (ruta de usuario).
- Estos archivos **no se instalan** con `cmake install`, no se suben al repositorio y están en `.gitignore`.
- El `access_token` **no se imprime completo en logs**.
- Si el `access_token` expira, se refresca automáticamente con el `refresh_token` (`[AUTH] Access token refrescado...`).
- Si el `refresh_token` falla, se limpia la sesión y se solicita nuevo inicio de sesión (`[AUTH] La sesión expiró...`).
- El cierre de sesión borra el token local pero no borra la configuración ni los respaldos de tareas.

</details>

<details>
<summary><strong>Validación de ruta base</strong></summary>

Antes de cada sincronización (`syncAll`, `syncCourse`, `syncFolders`), se valida la ruta base:

- Si está vacía, no existe, no es directorio, o no tiene permisos de escritura → la sync se cancela.
- **No se usa fallback silencioso** a ninguna otra ruta.
- El `sync_state.json` no se actualiza como si la sync hubiera ocurrido.
- El usuario recibe un mensaje claro: `[SEC] Ruta base no existe: /ruta. Sincronización cancelada.`

Esto protege de sincronizaciones accidentales cuando el disco externo está desmontado.

</details>

<details>
<summary><strong>Prefijos de log</strong></summary>

| Prefijo | Significado |
|---|---|
| `[SEC]` | Validación de seguridad (ruta base, permisos) |
| `[AUTH]` | Autenticación y tokens de Google |
| `[ARCH]` | Protección de tarea archivada o restauración |
| `[STAGE]` | Escritura de staging temporal |
| `DIFF` | Resultado del diff staging vs estado persistente |
| `ERR` | Error general |

</details>

---

## Sincronización (detalle técnico)

<details>
<summary><strong>Staging de metadata</strong></summary>

Cada sincronización completa (`syncAll`/`syncCourse`) crea un staging temporal en `~/.cache/ClassroomVault/sync_staging/`:

1. Se obtiene metadata fresca de Classroom.
2. Se escribe en staging (nunca directo al estado persistente).
3. Se crea un manifest por curso con `fetchStatus: complete/incomplete`.
4. Se hace diff de staging vs `sync_state.json`.
5. Solo después se aplican cambios en disco y se actualizan `metadata.json` y `sync_state.json`.
6. Solo después se procesan adjuntos.

La metadata nueva de Classroom tiene prioridad sobre la local en todos los casos, **excepto** para tareas archivadas.

**Flujo de `Sincronizar materia`:** solo consulta y aplica cambios para ese curso — no modifica ni descarga adjuntos de otras materias.

</details>

<details>
<summary><strong>Reglas de deduplicación</strong></summary>

- Si `sync_state.json` indica el mismo `modifiedTime` y existe `localPath`, se omite.
- Si hay `md5Checksum`, se compara el hash local para omitir/reemplazar.
- Si no hay hash pero hay tamaño remoto, se compara tamaño para omitir/reemplazar.
- Si cambia, se vuelve a descargar/exportar y reemplaza el archivo local.
- El estado de adjuntos se guarda por tarea en `sync_state.json`.
- `metadata.json` se actualiza con una sección `attachments`, y **no se reescribe** si su hash (`metadataHash`) no cambió.
- Si falta carpeta/metadata/adjunto local previamente registrado, se marca como `MISS` y se repara en la siguiente sincronización.

</details>

<details>
<summary><strong>Checksums de adjuntos</strong></summary>

- Después de procesar adjuntos, la app genera `../Tarea/Adjuntos/.checksum` con **SHA256**.
- Al abrir la app, se verifica en segundo plano el checksum de tareas conocidas.
- Si falla un archivo, se intenta re-descargar **solo ese adjunto** (no descarga masiva).
- Si no se puede mapear el archivo fallido a un adjunto remoto, se registra error y se conserva el resto del estado.

</details>

<details>
<summary><strong>Cambio de ruta base en caliente</strong></summary>

- Al cambiar la ruta base desde la UI, se aplica **sin reiniciar**.
- La siguiente sincronización usa inmediatamente la ruta nueva.
- No se mueven ni se borran datos de la ruta anterior de forma automática.
- Si existe estado previo en `sync_state.json` apuntando a otra ruta, solo se reutilizan rutas previas que estén dentro de la base activa.

</details>

<details>
<summary><strong>Reconstrucción segura de índice local</strong></summary>

- Menú de cuenta → **Reconstruir índice local**.
- Hace backup de `sync_state.json` como `sync_state.json.bak.<timestamp>`.
- Limpia y reconstruye el estado interno desde Classroom + disco.
- No borra carpetas de tareas, no borra `Adjuntos/`, no borra archivos personales del usuario.
- Es una herramienta de recuperación, no parte del flujo normal de sincronización.

</details>

<details>
<summary><strong>Eventos de log incremental y contadores en GUI</strong></summary>

**Eventos:**

| Evento | Significado |
|---|---|
| `NEW` | Tarea nueva detectada. |
| `SAME` | Sin cambios de metadata. |
| `UPD` | Metadata actualizada. |
| `SKIP` | Adjunto sin cambios. |
| `MISS` | Faltante local detectado. |
| `ERR` | Error de sincronización/descarga/exportación. |

**Contadores en GUI:**
- Archivos descargados (binarios de Drive)
- Google exportados (Workspace exportado)
- Links guardados (`.url`)
- Adjuntos omitidos
- Errores adjuntos

</details>

<details>
<summary><strong>Preview con metadata</strong></summary>

- La vista de detalle de tarea usa `metadata.json` como fuente principal.
- Si no existe `metadata.json`, la app construye un preview de respaldo usando `sync_state.json` y datos cargados.
- Los adjuntos se muestran como tarjetas con acciones: abrir archivo local, abrir carpeta, abrir enlace original.

</details>

---

## Troubleshooting

<details>
<summary><strong>Drive devuelve 403 / la app pide permisos viejos</strong></summary>

**Problema:** si `token.json` fue creado antes de agregar `drive.readonly` (o el scope de submissions), puede faltar permiso para adjuntos.

**Solución:**
1. Cierra la app.
2. Borra `~/.config/ClassroomVault/token.json` (o usa el botón **Cerrar sesión** en la GUI).
3. Abre la app e inicia sesión de nuevo.
4. Acepta los permisos actualizados.

Mensaje esperado cuando falta permiso de Drive:

> `Se requiere permiso de lectura de Drive. Borra token.json o cierra sesión y vuelve a iniciar sesión para autorizar Drive.`

</details>

---

## Limitaciones actuales

- No hay cola paralela avanzada de descargas (procesamiento secuencial).
- No hay selector fino de política de versionado (actualmente reemplaza cuando detecta cambio).
- Si `files.export` falla por límites/permisos y existe enlace de vista, se guarda `.url` como respaldo.
