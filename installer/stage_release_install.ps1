# Копирует engine_gui.exe и все *.dll из каталога Release-сборки в build/install
# для последующей упаковки Inno Setup. Запуск из корня репозитория или через CMake.

param(
    [Parameter(Mandatory = $false)]
    [string] $RepoRoot = "",
    [Parameter(Mandatory = $false)]
    [string] $BuildDir = ""
)

$ErrorActionPreference = "Stop"

if (-not $RepoRoot) {
    $RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
}
if (-not $BuildDir) {
    $BuildDir = Join-Path $RepoRoot "build"
}

$dst = Join-Path $BuildDir "install"
$releaseDir = Join-Path $BuildDir "Release"
$exeRelease = Join-Path $releaseDir "engine_gui.exe"
$exeFlat = Join-Path $BuildDir "engine_gui.exe"

if (Test-Path $exeRelease) {
    $src = $releaseDir
    $exe = $exeRelease
}
elseif (Test-Path $exeFlat) {
    $src = $BuildDir
    $exe = $exeFlat
}
else {
    Write-Error "Не найден exe. Ожидалось $exeRelease или $exeFlat. Соберите Release (или Ninja в каталоге сборки)."
}

New-Item -ItemType Directory -Force -Path $dst | Out-Null
Copy-Item $exe (Join-Path $dst "engine_gui.exe") -Force
Get-ChildItem -Path $src -Filter "*.dll" -File -ErrorAction SilentlyContinue | ForEach-Object {
    Copy-Item $_.FullName $dst -Force
}

Write-Host "Установочный каталог обновлён: $dst"
