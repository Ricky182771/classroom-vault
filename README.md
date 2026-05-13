# Classroom Vault / TareaSync

Aplicacion de escritorio en **C++20 + Qt6** para sincronizar Google Classroom y organizar tareas por semestre/materia, con descarga opcional de adjuntos.

## Estado actual

- OAuth 2.0 para app de escritorio (flujo manual copiar/pegar codigo).
- Lectura de cursos activos y tareas desde Classroom API.
- Creacion y actualizacion de carpetas/`metadata.json`.
- Estado persistente en `sync_state.json` para evitar duplicados.
- Deteccion de `materials` en tareas.
- Descarga de adjuntos (fase actual):
  - `driveFile`: metadata + descarga binaria
  - Google Docs/Sheets/Slides/Drawings: exportacion
  - Links/YouTube/Forms: guardado como `.url`

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

## Si ya tenias sesion previa sin Drive

Si `token.json` fue creado antes de agregar `drive.readonly`, puede faltar permiso para adjuntos.

1. Cierra la app.
2. Borra `~/.config/ClassroomVault/token.json`.
3. Abre la app.
4. Inicia sesion de nuevo.
5. Acepta permiso de Drive.

Mensaje esperado cuando falta permiso:

`Se requiere permiso de lectura de Drive. Cierra sesion y vuelve a iniciar sesion para autorizar Drive.`

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

## Descarga de adjuntos

- Boton GUI: **Descargar adjuntos**.
- Opcion GUI: **Descargar adjuntos al sincronizar**.
- Para `driveFile`:
  - metadata: `files.get`
  - binarios: `files.get?alt=media`
  - Google Workspace: `files.export`
- Para `link`, `youtubeVideo`, `form`:
  - se guarda `.url` en carpeta `Adjuntos/`.

## Reglas de deduplicacion (actual)

- Si el archivo ya existe y coincide (`md5Checksum` cuando existe, o tamano), se omite.
- Si cambia, se vuelve a descargar/reemplazar.
- Estado de adjuntos se guarda por tarea en `sync_state.json`.

## Limitaciones actuales

- No hay cola paralela avanzada de descargas (procesamiento secuencial).
- No hay selector fino de politica de versionado (actualmente reemplaza cuando detecta cambio).
- Flujo OAuth sigue en modo manual (sin localhost callback automatico).
