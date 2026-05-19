# Classroom Vault / TareaSync

Aplicacion de escritorio en **C++20 + Qt6** para sincronizar Google Classroom y organizar tareas por semestre/materia, incluyendo adjuntos dentro del mismo flujo de sincronizacion.

## Estado actual

- OAuth 2.0 para app de escritorio con **login automatico por navegador + loopback local** (`127.0.0.1`).
- Lectura de cursos activos y tareas desde Classroom API.
- Creacion y actualizacion de carpetas/`metadata.json`.
- Estado persistente en `sync_state.json` para evitar duplicados.
- Restauracion de estado local al iniciar (cursos/tareas desde `sync_state.json`).
- Carga automatica de Classroom al abrir si existe sesion valida (sin abrir navegador automaticamente).
- Selector global de semestre (Todos/Sin semestre/Semestre 1..6) y selector manual por materia.
- Reconstruccion segura de indice local con backup `.bak` de `sync_state.json` (sin borrar trabajos del usuario).
- Deteccion de `materials` en tareas.
- Sincronizacion por **staging de metadata fresca** (`~/.cache/.../sync_staging`) antes de tocar estado persistente.
- `Sincronizar materia` sincroniza solo el `courseId` seleccionado (no ejecuta sync global).
- Deteccion de tareas eliminadas solo cuando el fetch del curso fue completo.
- Nueva interfaz Qt Widgets modular en modo oscuro (inspirada en Classroom, enfocada a respaldo historico):
  - Flujo jerarquico principal: **Inicio -> Materia -> Tarea**.
  - Grid de cursos responsive con **maximo 4 columnas**.
  - Navegacion contextual con `TopBar`, `PathBar`, `Breadcrumb`, cards de cursos y cards/lista de tareas.
  - Preview de tarea desde `metadata.json` con **scroll vertical unico** (header + descripcion + evidencia + adjuntos).
- Descarga de adjuntos (fase actual):
  - `driveFile`: metadata + descarga binaria
  - Google Docs/Sheets/Slides/Drawings: exportacion
  - Links/YouTube/Forms: guardado como `.url`

## Navegacion UI

- `Inicio`: resumen de respaldo, KPIs, filtros y grid de materias.
- `Materia`: lista de tareas del curso seleccionado, estado de metadata/adjuntos y acciones.
- `Tarea`: previsualizacion local (titulo, descripcion, fecha, estado, evidencia, adjuntos).
- `Actividad`: panel desplegable para logs recientes.
- `Ruta`: controles globales (cambiar ruta, abrir respaldo, sincronizar todo).

## Estados visuales de curso

- `Completo`: todas las tareas detectadas con respaldo local.
- `Pendiente`: faltan tareas por respaldar.
- `Error`: inconsistencias o errores detectados durante procesos previos.
- `Sin sync`: curso sin sincronizacion util aun.

Estados locales usados en incrementalidad:

- `MissingLocal`: existia estado en `sync_state.json` pero faltan carpetas/archivos en disco.
- `NotSynced`: elemento sin respaldo local aun.

## Preview con metadata

- La vista de detalle de tarea usa `metadata.json` como fuente principal.
- Si no existe `metadata.json`, la app construye un preview de respaldo usando `sync_state.json` y datos cargados.
- Los adjuntos se muestran como tarjetas con acciones:
  - abrir archivo local
  - abrir carpeta
  - abrir enlace original

## Dependencias (Fedora)

```bash
sudo dnf install qt6-qtbase-devel cmake gcc-c++ ninja-build
```

## Compilar (desarrollo local)

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## Ejecutar desde build

```bash
./build/classroom-vault
```

## Instalacion en Fedora/Linux

Instalar en el sistema (binario + desktop entry + icono + metainfo):

```bash
sudo cmake --install build
```

Alternativa equivalente:

```bash
cd build
sudo ninja install
```

Despues de instalar:

- Ejecutable disponible en terminal como:

```bash
classroom-vault
```

- Entrada de menu: **Classroom Vault** (KDE/GNOME).

## Desinstalacion (opcional)

Si compilaste con Ninja:

```bash
cd build
sudo ninja uninstall
```

## Modo CLI

Sincronizar carpetas con sample local:

```bash
./classroom-vault --cli-sync --base-path /ruta/del/disco --sample ../sample_classroom_data.json
```

Sincronizar y luego descargar adjuntos en CLI:

```bash
./classroom-vault --cli-sync --cli-download-attachments --base-path /ruta/del/disco --sample ../sample_classroom_data.json
```

## APIs y scopes

### APIs

- Google Classroom API
- Google Drive API

Debes habilitar ambas en tu proyecto de Google Cloud Console.

### Scopes recomendados

- `https://www.googleapis.com/auth/classroom.courses.readonly`
- `https://www.googleapis.com/auth/classroom.coursework.me.readonly`
- `https://www.googleapis.com/auth/classroom.student-submissions.me.readonly`
- `https://www.googleapis.com/auth/drive.readonly`

## Autenticacion OAuth automatica

- Al presionar **Iniciar sesion**, la app levanta un servidor temporal local en `127.0.0.1` con puerto dinamico.
- La app abre el navegador del sistema y envia a Google OAuth.
- Google redirige a `http://127.0.0.1:<puerto>/callback?...`.
- Classroom Vault captura el `code` automaticamente y cierra el servidor local.
- Se muestra una pagina de exito en el navegador:
  - `Autorizacion completada. Ya puedes volver a Classroom Vault.`
- La app intercambia el `code` por `access_token` y `refresh_token` y guarda `token.json`.
- En ejecuciones siguientes, si el token expiro y hay `refresh_token`, se refresca automaticamente sin abrir navegador.
- Si falla el refresco, se solicita iniciar sesion nuevamente.
- Timeout de autenticacion: `180` segundos.

### Comportamiento al abrir la app

- Primero restaura estado local desde `sync_state.json` para mostrar informacion aun sin red.
- Luego revisa sesion guardada:
  - si hay token valido, carga Classroom automaticamente;
  - si el access token vencio y hay refresh token, intenta refrescar y cargar;
  - si no hay sesion, queda en `No conectado`.
- **No abre navegador automaticamente** si no hay sesion. El navegador solo se abre al presionar **Iniciar sesion**.

### Configuracion de credenciales

En `config.json` puedes usar:

- `oauth.clientId` y `oauth.clientSecret` directamente, o
- `oauth.credentialsFile` apuntando a un `credentials.json` de Google (`installed`).

Si usas `credentialsFile`, la app toma automaticamente:

- `client_id`
- `client_secret`
- `token_uri`
- `redirect_uris` (cuando aplica)

### Fallback manual

Si por alguna razon no se puede iniciar el servidor local (`QTcpServer`), la app cambia a modo manual:

1. Abre el navegador.
2. Solicita pegar el `authorization code`.
3. Continua con el intercambio de tokens.

## Si ya tenias sesion previa sin Drive

Si `token.json` fue creado antes de agregar `drive.readonly`, puede faltar permiso para adjuntos.

1. Cierra la app.
2. Borra `~/.config/ClassroomVault/token.json`.
3. Abre la app.
4. Inicia sesion de nuevo.
5. Acepta permiso de Drive.

Mensaje esperado cuando falta permiso:

`Se requiere permiso de lectura de Drive. Borra token.json o cierra sesion y vuelve a iniciar sesion para autorizar Drive.`

Tambien puedes usar el boton **Cerrar sesion** en la GUI para limpiar sesion local y volver a autorizar.

Si agregaste el scope de submissions y ya tenias token viejo:

1. Cierra la app.
2. Borra `~/.config/ClassroomVault/token.json` (o usa **Cerrar sesion**).
3. Inicia sesion de nuevo.
4. Acepta el permiso actualizado.

## Estructura de salida

```text
Ruta base/
└── Tareas/
    └── Semestre/
        └── Materia/
            └── YYYY-MM-DD - Nombre de tarea/
                ├── metadata.json
                └── Adjuntos/
                    ├── archivo.pdf
                    ├── documento.docx
                    └── Enlace - Referencia.url
```

## Archivos de configuracion

- `~/.config/ClassroomVault/config.json`
- `~/.config/ClassroomVault/token.json`
- `~/.config/ClassroomVault/sync_state.json`
- `~/.cache/ClassroomVault/` (cache temporal y staging)

`token.json` se guarda localmente y no debe subirse al repositorio.

Notas importantes:

- La instalacion **no** incluye `credentials.json`, `token.json`, `sync_state.json` ni otros datos privados del usuario.
- `credentials.json` debe colocarse manualmente en `~/.config/ClassroomVault/` (o configurarse desde la app, si aplica).
- Cambiar `CMAKE_INSTALL_PREFIX` permite instalar en `/usr` o rutas personalizadas.

## Descarga de adjuntos

- **Sincronizar todo** ahora tambien procesa adjuntos automaticamente.
- Los adjuntos se guardan siempre en `../Tarea/Adjuntos/`.
- Para `driveFile`:
  - metadata: `files.get`
  - binarios: `files.get?alt=media`
  - Google Workspace: `files.export`
- Para `link`, `youtubeVideo`, `form`:
  - se guarda `.url` en carpeta `Adjuntos/`.

## Sincronizacion con staging

Flujo de `Sincronizar todo`:

1. Fetch remoto de Classroom.
2. Guardado de metadata nueva en staging temporal.
3. Manifest por curso con estado `complete/incomplete`.
4. Diff staging vs `sync_state.json` (`new/updated/same/deleted_archived/restored`).
5. Aplicacion de cambios en `metadata.json` y `sync_state.json`.
6. Procesamiento de adjuntos.

Flujo de `Sincronizar materia`:

- Solo consulta y aplica cambios para ese curso.
- No modifica otras materias.
- No descarga adjuntos de otras materias.

Regla clave:

- La metadata nueva en staging tiene prioridad para esa sesion de sync.
- Si el fetch de un curso es `incomplete`, no se detectan eliminadas para ese curso.
- Los cambios normales (tareas nuevas o actualizadas) deben reflejarse sin flush manual.

## Cambio de ruta base en caliente

- Al cambiar la ruta base desde la UI, se aplica **sin reiniciar**.
- La siguiente sincronizacion usa inmediatamente la ruta nueva.
- No se mueven ni se borran datos de la ruta anterior de forma automatica.
- Si existe estado previo en `sync_state.json` apuntando a otra ruta, solo se reutilizan rutas previas que esten dentro de la base activa.

## Estado de entrega de tareas

- Classroom Vault usa `studentSubmissions` cuando estan disponibles para determinar entrega real.
- `TURNED_IN` y `RETURNED` se muestran como:
  - `Entregada` (vigente)
  - `Expirada y entregada` (si ya vencio)
- `No entregada` solo se muestra con evidencia confiable de no entrega (`NEW`, `CREATED`, `RECLAIMED_BY_STUDENT`) y tarea vencida.
- Si no hay evidencia confiable de submissions, la app evita marcar en rojo por inferencia y usa estado neutral (`Expirada`).

### Exportaciones Workspace

- Google Docs (`application/vnd.google-apps.document`) -> `.docx`
- Google Sheets (`application/vnd.google-apps.spreadsheet`) -> `.xlsx`
- Google Slides (`application/vnd.google-apps.presentation`) -> `.pptx`
- Google Drawings (`application/vnd.google-apps.drawing`) -> `.png`
- Google Forms (`application/vnd.google-apps.form`) -> `.url` (enlace, no export binario)
- Otros `application/vnd.google-apps.*` no exportables -> `.url` con `webViewLink` cuando exista

## Reglas de deduplicacion (actual)

- Si `sync_state.json` indica mismo `modifiedTime` y existe `localPath`, se omite.
- Si hay `md5Checksum`, se compara hash local para omitir/reemplazar.
- Si no hay hash pero hay tamano remoto, se compara tamano para omitir/reemplazar.
- Si cambia, se vuelve a descargar/exportar y reemplaza el archivo local.
- Estado de adjuntos se guarda por tarea en `sync_state.json`.
- `metadata.json` de cada tarea se actualiza con una seccion `attachments` basada en el estado sincronizado.
- `metadata.json` no se reescribe si su hash (`metadataHash`) no cambio.
- Si falta carpeta/metadata/adjunto local previamente registrado, se marca como `MISS` y se repara en la siguiente sincronizacion.

## Checksums de adjuntos

- Despues de procesar adjuntos, la app genera `../Tarea/Adjuntos/.checksum` con `SHA256`.
- Al abrir la app, se verifica en segundo plano el checksum de tareas conocidas.
- Si falla un archivo, se intenta re-descargar solo ese adjunto (no descarga masiva).
- Si no se puede mapear el archivo fallido a un adjunto remoto, se registra error y se conserva el resto del estado.

## Reconstruccion segura de indice local

- Menu de cuenta -> **Reconstruir indice local**.
- Hace backup de `sync_state.json` como `sync_state.json.bak.<timestamp>`.
- Limpia y reconstruye el estado interno desde Classroom + disco.
- No borra carpetas de tareas, no borra `Adjuntos/`, no borra archivos personales del usuario.
- Es una herramienta de recuperacion, no parte del flujo normal de sincronizacion.

## Columna "Tu trabajo"

- En detalle de tarea existe panel lateral **Tu trabajo**.
- Lista archivos y carpetas propios dentro de `../Tarea/`.
- Ignora `metadata.json` y `Adjuntos/`.
- Doble clic abre archivo/carpeta local.

### Eventos de log incremental

- `NEW`: tarea nueva detectada.
- `SAME`: sin cambios de metadata.
- `UPD`: metadata actualizada.
- `SKIP`: adjunto sin cambios.
- `MISS`: faltante local detectado.
- `ERR`: error de sincronizacion/descarga/exportacion.

### Contadores en GUI

- Archivos descargados (binarios de Drive)
- Google exportados (Workspace exportado)
- Links guardados (`.url`)
- Adjuntos omitidos
- Errores adjuntos

## Limitaciones actuales

- No hay cola paralela avanzada de descargas (procesamiento secuencial).
- No hay selector fino de politica de versionado (actualmente reemplaza cuando detecta cambio).
- Si `files.export` falla por limites/permisos y existe enlace de vista, se guarda `.url` como respaldo.

## Troubleshooting OAuth

Problema: la app sigue pidiendo permisos viejos o Drive devuelve 403.

Solucion:

1. Cierra la app.
2. Borra `~/.config/ClassroomVault/token.json` o usa **Cerrar sesion**.
3. Abre la app.
4. Inicia sesion de nuevo.
5. Acepta los permisos actualizados.
