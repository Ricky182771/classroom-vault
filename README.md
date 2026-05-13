# Classroom Vault / TareaSync

Aplicacion de escritorio en **C++20 + Qt6** para sincronizar Google Classroom y organizar tareas por semestre/materia, con descarga opcional de adjuntos.

## Estado actual

- OAuth 2.0 para app de escritorio con **login automatico por navegador + loopback local** (`127.0.0.1`).
- Lectura de cursos activos y tareas desde Classroom API.
- Creacion y actualizacion de carpetas/`metadata.json`.
- Estado persistente en `sync_state.json` para evitar duplicados.
- Deteccion de `materials` en tareas.
- Nueva interfaz Qt Widgets modular en modo oscuro (inspirada en Classroom, enfocada a respaldo historico):
  - Sidebar + TopBar + PathBar + Home Dashboard + vistas secundarias.
  - Home con KPIs, cards de curso, actividad reciente y estado de almacenamiento.
  - Vistas base para Tareas, Adjuntos, Historial y Configuracion.
- Descarga de adjuntos (fase actual):
  - `driveFile`: metadata + descarga binaria
  - Google Docs/Sheets/Slides/Drawings: exportacion
  - Links/YouTube/Forms: guardado como `.url`

## Navegacion UI

- `Inicio`: resumen de respaldo, cards de cursos, actividad reciente.
- `Cursos`: tabla de materias y asignacion de semestre por `courseId`.
- `Tareas`: tabla de tareas sincronizadas y apertura por doble clic.
- `Adjuntos`: tabla de archivos/enlaces guardados localmente.
- `Historial`: logs con filtros y acciones (limpiar/copiar/guardar).
- `Configuracion`: ruta base, preferencias de sync y estado de cuenta.

## Estados visuales de curso

- `Completo`: todas las tareas detectadas con respaldo local.
- `Pendiente`: faltan tareas por respaldar.
- `Error`: inconsistencias o errores detectados durante procesos previos.
- `Sin sync`: curso sin sincronizacion util aun.

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
