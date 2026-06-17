@echo off
chcp 65001 >nul 2>&1
setlocal

:: ============================================================
::  Classroom Vault - Compilacion e instalacion para Windows
::  Ejecutar directamente: doble clic o desde terminal.
:: ============================================================

set "SELF=%~f0"
set "SELF_DIR=%~dp0"
set "TEMP_PS=%TEMP%\cv_build_%RANDOM%.ps1"

:: Extraer el codigo PowerShell embebido y ejecutarlo
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$c=[IO.File]::ReadAllText($env:SELF,[Text.Encoding]::UTF8);" ^
  "$m='__PS_BEGIN__';" ^
  "$i=$c.IndexOf($m);" ^
  "if($i -ge 0){" ^
  "  $ps=$c.Substring($i+$m.Length).TrimStart([char[]]@(13,10));" ^
  "  [IO.File]::WriteAllText($env:TEMP_PS,$ps,(New-Object Text.UTF8Encoding $false));" ^
  "  & $env:TEMP_PS;" ^
  "  $ec=$LASTEXITCODE;" ^
  "  Remove-Item $env:TEMP_PS -Force -EA SilentlyContinue;" ^
  "  exit $ec" ^
  "}"

exit /b %ERRORLEVEL%
__PS_BEGIN__


$ErrorActionPreference = 'Continue'

# --- Configuracion -----------------------------------------------------------
$QtVersion  = "6.8.0"
$ScriptDir  = $env:SELF_DIR.TrimEnd('\')
$QtLocalDir = Join-Path $ScriptDir ".qt"
$BuildDir   = Join-Path $ScriptDir "build"
$InstallDir = Join-Path $env:ProgramFiles "ClassroomVault"
# ------------------------------------------------------------------------------

# --- Helpers ------------------------------------------------------------------
function Write-Step { param([string]$m) Write-Host "`n==> $m" -ForegroundColor Cyan }
function Write-OK   { param([string]$m) Write-Host "    [OK]  $m" -ForegroundColor Green }
function Write-Warn { param([string]$m) Write-Host "    [!]   $m" -ForegroundColor Yellow }
function Write-Err  { param([string]$m) Write-Host "    [X]   $m" -ForegroundColor Red }
function Write-Info { param([string]$m) Write-Host "          $m" -ForegroundColor Gray }

function Refresh-Path {
    $env:Path = [Environment]::GetEnvironmentVariable('Path','Machine') + ';' +
                [Environment]::GetEnvironmentVariable('Path','User')
}

function Test-Cmd {
    param([string]$cmd)
    [bool](Get-Command $cmd -ErrorAction SilentlyContinue)
}

function Exit-OnError {
    param([string]$msg)
    Write-Err $msg
    Write-Host ""
    Read-Host "Presiona Enter para cerrar"
    exit 1
}
# ------------------------------------------------------------------------------

# --- Banner -------------------------------------------------------------------
$host.UI.RawUI.WindowTitle = "Classroom Vault - Build & Install"
Write-Host ""
Write-Host "  ================================================" -ForegroundColor Magenta
Write-Host "   Classroom Vault - Build & Install  (Windows)   " -ForegroundColor Magenta
Write-Host "  ================================================" -ForegroundColor Magenta
Write-Host ""
Write-Host "  Directorio del proyecto: $ScriptDir" -ForegroundColor DarkGray
Write-Host "  Version de Qt objetivo:  $QtVersion" -ForegroundColor DarkGray
# ------------------------------------------------------------------------------

# --- Verificar permisos de administrador --------------------------------------
$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()
           ).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)

if (-not $isAdmin) {
    Write-Warn "Se requieren permisos de administrador."
    Write-Info "Reabriendo como administrador..."
    Start-Process -FilePath $env:SELF -Verb RunAs
    exit 0
}
Write-Info "Ejecutando como administrador."
Set-Location $ScriptDir
# ------------------------------------------------------------------------------

# ==============================================================================
#  PASO 1 : winget
# ==============================================================================
Write-Step "Verificando winget..."
if (Test-Cmd winget) {
    Write-OK "winget disponible."
} else {
    Exit-OnError "winget no encontrado. Instalalo desde Microsoft Store: https://aka.ms/getwinget"
}

# ==============================================================================
#  PASO 2 : Visual Studio Build Tools 2022
# ==============================================================================
Write-Step "Verificando Visual Studio Build Tools 2022..."
$vsWherePath = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vsInstallPath = $null

if (Test-Path $vsWherePath) {
    $vsInstallPath = & $vsWherePath -latest `
        -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
        -property installationPath 2>$null
}

if ($vsInstallPath -and (Test-Path $vsInstallPath)) {
    Write-OK "Encontrado: $vsInstallPath"
} else {
    Write-Warn "No encontrado. Instalando (puede tardar 10-15 minutos)..."
    Write-Info "Descargando e instalando el componente C++ Desktop..."
    winget install Microsoft.VisualStudio.2022.BuildTools `
        --silent --accept-source-agreements --accept-package-agreements `
        --override "--quiet --wait --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"

    Refresh-Path

    # Reintentar deteccion
    if (Test-Path $vsWherePath) {
        $vsInstallPath = & $vsWherePath -latest `
            -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
            -property installationPath 2>$null
    }
    if (-not $vsInstallPath -or -not (Test-Path $vsInstallPath)) {
        Exit-OnError "No se pudo verificar VS Build Tools despues de instalar. Reinicia y vuelve a ejecutar."
    }
    Write-OK "Instalado: $vsInstallPath"
}

# ==============================================================================
#  PASO 3 : CMake
# ==============================================================================
Write-Step "Verificando CMake..."
Refresh-Path
if (Test-Cmd cmake) {
    Write-OK "$(cmake --version 2>&1 | Select-Object -First 1)"
} else {
    Write-Warn "Instalando CMake..."
    winget install Kitware.CMake --silent --accept-source-agreements --accept-package-agreements
    Refresh-Path
    if (-not (Test-Cmd cmake)) {
        Exit-OnError "CMake no disponible tras instalar. Reinicia y vuelve a ejecutar."
    }
    Write-OK "CMake instalado."
}

# ==============================================================================
#  PASO 4 : Ninja
# ==============================================================================
Write-Step "Verificando Ninja..."
Refresh-Path
if (Test-Cmd ninja) {
    Write-OK "ninja $(ninja --version 2>&1)"
} else {
    Write-Warn "Instalando Ninja..."
    winget install Ninja-build.Ninja --silent --accept-source-agreements --accept-package-agreements
    Refresh-Path
    if (-not (Test-Cmd ninja)) {
        Exit-OnError "Ninja no disponible tras instalar. Reinicia y vuelve a ejecutar."
    }
    Write-OK "Ninja instalado."
}

# ==============================================================================
#  PASO 5 : Python (necesario para descargar Qt)
# ==============================================================================
Write-Step "Verificando Python..."
Refresh-Path

$hasPython = $false
if (Test-Cmd python) {
    $pyVer = python --version 2>&1
    if ($LASTEXITCODE -eq 0 -and "$pyVer" -match 'Python \d') {
        Write-OK "$pyVer"
        $hasPython = $true
    }
}

if (-not $hasPython) {
    Write-Warn "Instalando Python 3.12..."
    winget install Python.Python.3.12 --silent --accept-source-agreements --accept-package-agreements
    Refresh-Path
    $pyVer = python --version 2>&1
    if ($LASTEXITCODE -ne 0 -or "$pyVer" -notmatch 'Python \d') {
        Exit-OnError "Python no disponible tras instalar. Reinicia y vuelve a ejecutar."
    }
    Write-OK "Python instalado: $pyVer"
}

# ==============================================================================
#  PASO 6 : Descargar Qt via aqtinstall
# ==============================================================================
Write-Step "Verificando Qt $QtVersion..."

$QtArch   = $null
$QtPrefix = $null

# Buscar si ya existe una descarga previa
foreach ($arch in @('win64_msvc2019_64', 'win64_msvc2022_64')) {
    $subDir    = $arch -replace 'win64_', ''
    $candidate = Join-Path $QtLocalDir "$QtVersion\$subDir"
    if (Test-Path (Join-Path $candidate "bin\qmake.exe")) {
        $QtArch   = $arch
        $QtPrefix = $candidate
        break
    }
}

if ($QtPrefix) {
    Write-OK "Qt $QtVersion ya descargado en $QtPrefix"
} else {
    # Instalar aqtinstall
    Write-Info "Instalando aqtinstall..."
    python -m pip install --upgrade pip --quiet 2>$null
    python -m pip install aqtinstall --quiet 2>$null
    if (-not $?) {
        Exit-OnError "Error instalando aqtinstall (pip)."
    }

    # Detectar arquitectura disponible
    Write-Info "Consultando arquitecturas disponibles para Qt $QtVersion..."
    $archList = python -m aqt list-qt windows desktop --arch $QtVersion 2>&1
    $archStr  = "$archList"

    if ($archStr -match 'win64_msvc2019_64') {
        $QtArch = 'win64_msvc2019_64'
    } elseif ($archStr -match 'win64_msvc2022_64') {
        $QtArch = 'win64_msvc2022_64'
    } else {
        Write-Err "No se encontro arquitectura MSVC 64-bit para Qt $QtVersion."
        Write-Err "Disponibles: $archStr"
        Write-Err "Edita `$QtVersion al inicio del script y reintenta."
        Exit-OnError "Arquitectura Qt no encontrada."
    }

    Write-Info "Descargando Qt $QtVersion ($QtArch)..."
    Write-Info "Destino: $QtLocalDir  (puede tardar unos minutos)"
    python -m aqt install-qt windows desktop $QtVersion $QtArch -O "$QtLocalDir"
    if ($LASTEXITCODE -and $LASTEXITCODE -ne 0) {
        Exit-OnError "Error descargando Qt $QtVersion."
    }

    $QtSubDir = $QtArch -replace 'win64_', ''
    $QtPrefix = Join-Path $QtLocalDir "$QtVersion\$QtSubDir"

    if (-not (Test-Path (Join-Path $QtPrefix "bin\qmake.exe"))) {
        Exit-OnError "Qt no se instalo correctamente en $QtPrefix."
    }

    Write-OK "Qt $QtVersion descargado en $QtPrefix"
}

# ==============================================================================
#  PASO 7 : Configurar entorno MSVC
# ==============================================================================
Write-Step "Configurando entorno MSVC (amd64)..."
$vcvarsall = Join-Path $vsInstallPath "VC\Auxiliary\Build\vcvarsall.bat"
if (-not (Test-Path $vcvarsall)) {
    Exit-OnError "No se encontro vcvarsall.bat en: $vcvarsall"
}

# Importar variables de entorno de MSVC al proceso PowerShell actual
$envDump = cmd /c "`"$vcvarsall`" amd64 >nul 2>&1 && set" 2>&1
foreach ($line in $envDump) {
    if ($line -is [string] -and $line -match '^([^=]+)=(.*)$') {
        [Environment]::SetEnvironmentVariable($matches[1], $matches[2], 'Process')
    }
}
Write-OK "Entorno MSVC amd64 importado."

# ==============================================================================
#  PASO 8 : Configurar proyecto con CMake
# ==============================================================================
Write-Step "Configurando proyecto con CMake..."

# Limpiar build si el cache apunta a otra ruta (proyecto movido / Qt diferente)
$cacheFile = Join-Path $BuildDir "CMakeCache.txt"
if (Test-Path $cacheFile) {
    $cacheContent = Get-Content $cacheFile -Raw -ErrorAction SilentlyContinue
    $qtEscaped  = [regex]::Escape($QtPrefix)
    $srcEscaped = [regex]::Escape($ScriptDir)
    if ($cacheContent -and (($cacheContent -notmatch $qtEscaped) -or ($cacheContent -notmatch $srcEscaped))) {
        Write-Warn "Cache de build anterior incompatible. Limpiando build/..."
        Remove-Item $BuildDir -Recurse -Force
    }
}

Write-Info "cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release"
Write-Info "  -DCMAKE_PREFIX_PATH=$QtPrefix"

cmake -S $ScriptDir -B $BuildDir -G Ninja `
    -DCMAKE_BUILD_TYPE=Release `
    "-DCMAKE_PREFIX_PATH=$QtPrefix"

if ($LASTEXITCODE -and $LASTEXITCODE -ne 0) {
    Exit-OnError "CMake configure fallo (codigo $LASTEXITCODE)."
}
Write-OK "Proyecto configurado."

# ==============================================================================
#  PASO 9 : Compilar
# ==============================================================================
Write-Step "Compilando Classroom Vault..."
cmake --build $BuildDir
if ($LASTEXITCODE -and $LASTEXITCODE -ne 0) {
    Exit-OnError "Compilacion fallo (codigo $LASTEXITCODE)."
}

$exeInBuild = Join-Path $BuildDir "classroom-vault.exe"
if (-not (Test-Path $exeInBuild)) {
    Exit-OnError "No se genero classroom-vault.exe en $BuildDir."
}
Write-OK "Compilacion exitosa."

# ==============================================================================
#  PASO 10 : Instalar en Archivos de programa
# ==============================================================================
Write-Step "Instalando en $InstallDir..."
New-Item -Path $InstallDir -ItemType Directory -Force | Out-Null
Copy-Item $exeInBuild "$InstallDir\classroom-vault.exe" -Force

# windeployqt — copiar DLLs de Qt junto al ejecutable
$windeployqt = Join-Path $QtPrefix "bin\windeployqt.exe"
if (Test-Path $windeployqt) {
    Write-Info "Ejecutando windeployqt (copiando DLLs de Qt)..."
    & $windeployqt --no-translations --no-quick-import --no-system-d3d-compiler `
        "$InstallDir\classroom-vault.exe" 2>&1 | Out-Null
    if ($LASTEXITCODE -and $LASTEXITCODE -ne 0) {
        Write-Warn "windeployqt reporto advertencias, pero la instalacion podria funcionar."
    } else {
        Write-OK "DLLs de Qt desplegadas."
    }
} else {
    Write-Warn "windeployqt no encontrado. Puede requerir copiar DLLs manualmente."
}

# Copiar icono si existe
$iconSrc = Join-Path $ScriptDir "resources\icons\classroom-vault.svg"
if (Test-Path $iconSrc) {
    Copy-Item $iconSrc "$InstallDir\" -Force
}

# Acceso directo en Menu Inicio
Write-Info "Creando acceso directo en Menu Inicio..."
$startMenu    = Join-Path $env:ProgramData "Microsoft\Windows\Start Menu\Programs"
$shortcutPath = Join-Path $startMenu "Classroom Vault.lnk"
try {
    $wshell   = New-Object -ComObject WScript.Shell
    $shortcut = $wshell.CreateShortcut($shortcutPath)
    $shortcut.TargetPath       = "$InstallDir\classroom-vault.exe"
    $shortcut.WorkingDirectory = $InstallDir
    $shortcut.Description      = "Classroom Vault - Respaldo local de Google Classroom"
    $shortcut.Save()
    Write-OK "Acceso directo: $shortcutPath"
} catch {
    Write-Warn "No se pudo crear el acceso directo: $_"
}

# ==============================================================================
#  LISTO
# ==============================================================================
Write-Host ""
Write-Host "  ================================================" -ForegroundColor Green
Write-Host "   Instalacion completada exitosamente!            " -ForegroundColor Green
Write-Host "  ================================================" -ForegroundColor Green
Write-Host ""
Write-Host "  Ejecutable : $InstallDir\classroom-vault.exe" -ForegroundColor White
Write-Host "  Menu Inicio: Classroom Vault" -ForegroundColor White
Write-Host ""
Write-Host "  Siguiente paso: configura tus credenciales OAuth en:" -ForegroundColor Yellow
Write-Host "    %APPDATA%\ClassroomVault\config.json" -ForegroundColor Yellow
Write-Host "    (ver config.example.json como referencia)" -ForegroundColor Yellow
Write-Host ""
Read-Host "Presiona Enter para cerrar"
