# Classroom Vault (TareaSync)

Aplicación de escritorio en C++20 + Qt6 para sincronizar cursos y tareas de Google Classroom y organizar carpetas en un disco externo.

## Estado actual

- Version inicial funcional con datos de prueba locales.
- GUI minima con Qt Widgets.
- Preparado para OAuth 2.0 y Google Classroom API REST.

## Dependencias en Fedora

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

## Ejecutar

```bash
./classroom-vault
```

## Modo CLI (v0.1)

Sin abrir interfaz grafica:

```bash
./classroom-vault --cli-sync --base-path /ruta/del/disco --sample ../sample_classroom_data.json
```

## Flujo recomendado (v0.1-v0.2)

1. Seleccionar carpeta base del respaldo.
2. Presionar **Cargar datos de prueba**.
3. Revisar cursos, tareas y asignar semestre en la tabla de cursos.
4. Presionar **Sincronizar carpetas**.

## Estructura creada

```text
Disco/Tareas/Semestre/Materia/Tarea/metadata.json
```

## Archivos de configuracion

- `~/.config/ClassroomVault/config.json`
- `~/.config/ClassroomVault/sync_state.json`
- `~/.config/ClassroomVault/token.json`

Puedes partir de `config.example.json` para agregar `clientId` y `clientSecret` de OAuth.

## Notas OAuth

- El boton **Iniciar sesion** usa modo manual (copiar/pegar authorization code).
- Se abre navegador externo para login.
- Se intercambia codigo por `access_token` y `refresh_token`.

## API Classroom preparada

- `courses.list` (`/v1/courses?courseStates=ACTIVE`)
- `courses.courseWork.list` (`/v1/courses/{courseId}/courseWork`)
