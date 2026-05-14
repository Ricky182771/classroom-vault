# Classroom Vault / TareaSync

Aplicacion de escritorio en **C++20 + Qt6** para sincronizar Google Classroom y organizar tareas por semestre/materia, con descarga opcional de adjuntos.

## Estado actual

- OAuth 2.0 para app de escritorio con **login automatico por navegador + loopback local** (`127.0.0.1`).
- Lectura de cursos activos y tareas desde Classroom API.
- Creacion y actualizacion de carpetas/`metadata.json`.
- Estado persistente en `sync_state.json` para evitar duplicados.
- Restauracion de estado local al iniciar (cursos/tareas desde `sync_state.json`).
- Carga automatica de Classroom al abrir si existe sesion valida (sin abrir navegador automaticamente).
- Deteccion de `materials` en tareas.
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
- `Ruta`: controles globales (cambiar ruta, abrir respaldo, sincronizar, descargar adjuntos).

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

## Compilar

```bash
mkdir -p build
cd build
cmake .. -G Ninja
ninja
```

## Ejecutar GUI

```bash
./classroom-vault
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

`token.json` se guarda localmente y no debe subirse al repositorio.

## Descarga de adjuntos

- Boton GUI: **Descargar adjuntos**.
- Opcion GUI: **Descargar adjuntos al sincronizar**.
- Para `driveFile`:
  - metadata: `files.get`
  - binarios: `files.get?alt=media`
  - Google Workspace: `files.export`
- Para `link`, `youtubeVideo`, `form`:
  - se guarda `.url` en carpeta `Adjuntos/`.

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
