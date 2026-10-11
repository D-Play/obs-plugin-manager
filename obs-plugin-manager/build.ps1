# PowerShell Build-Skript für obs-plugin-manager
param(
    [string]$QtDir = "",
    [string]$ObsDir = "",
    [string]$Config = "Release"
)

# 1. CMake auffinden
$cmakeCmd = "cmake"
if (-not (Get-Command "cmake" -ErrorAction SilentlyContinue)) {
    $vsCmake = "A:\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
    if (Test-Path $vsCmake) {
        $cmakeCmd = $vsCmake
        Write-Host "Verwende CMake aus Visual Studio: $vsCmake" -ForegroundColor Cyan
    } else {
        Write-Error "CMake wurde weder im PATH noch in Visual Studio gefunden. Bitte installiere CMake oder trage es in PATH ein."
        exit 1
    }
}

# 2. CMAKE_PREFIX_PATH vorbereiten
$prefixPaths = @()
if ($QtDir -and (Test-Path $QtDir)) {
    $prefixPaths += $QtDir
}
if ($ObsDir -and (Test-Path $ObsDir)) {
    $prefixPaths += $ObsDir
}

$cmakeArgs = @("-B", "build", "-S", ".")
if ($prefixPaths.Count -gt 0) {
    $prefixArg = ($prefixPaths -join ";")
    $cmakeArgs += "-DCMAKE_PREFIX_PATH=$prefixArg"
}

Write-Host "==> Konfiguriere Projekt mit CMake..." -ForegroundColor Green
& $cmakeCmd @cmakeArgs

if ($LASTEXITCODE -ne 0) {
    Write-Host ""
    Write-Host "[!] Hinweis zur Konfiguration:" -ForegroundColor Yellow
    Write-Host "Falls Qt6 oder OBS Studio SDK fehlen, uebergebe deren Pfade z. B. so:" -ForegroundColor Yellow
    Write-Host "  .\build.ps1 -QtDir 'C:\Qt\6.x\msvc2019_64' -ObsDir 'C:\Program Files\obs-studio'" -ForegroundColor Yellow
    exit $LASTEXITCODE
}

Write-Host "==> Kompiliere ($Config)..." -ForegroundColor Green
& $cmakeCmd --build build --config $Config

if ($LASTEXITCODE -eq 0) {
    Write-Host "==> Build erfolgreich abgeschlossen!" -ForegroundColor Green
}
