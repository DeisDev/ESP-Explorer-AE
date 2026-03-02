$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $PSScriptRoot
$distRoot = Join-Path $projectRoot 'dist'
$packageRoot = Join-Path $distRoot 'package\Data'

$pluginsDir = Join-Path $packageRoot 'F4SE\Plugins'
$langDir = Join-Path $packageRoot 'Interface\ESPExplorerAE\lang'
$fontsDir = Join-Path $packageRoot 'Interface\ESPExplorerAE\fonts'

if (Test-Path $packageRoot) {
    Remove-Item $packageRoot -Recurse -Force
}

New-Item -ItemType Directory -Path $pluginsDir -Force | Out-Null
New-Item -ItemType Directory -Path $langDir -Force | Out-Null
New-Item -ItemType Directory -Path $fontsDir -Force | Out-Null

Copy-Item (Join-Path $distRoot 'lang\*.ini') $langDir -Force
Copy-Item (Join-Path $distRoot 'fonts\*.ttf') $fontsDir -Force

$dllCandidates = @(
    (Join-Path $projectRoot 'build\windows\x64\release\ESPExplorerAE.dll')
    (Join-Path $projectRoot 'build\windows\x64\releasedbg\ESPExplorerAE.dll')
)

$dllPath = $dllCandidates |
    Where-Object { Test-Path $_ } |
    ForEach-Object { Get-Item $_ } |
    Sort-Object LastWriteTime -Descending |
    Select-Object -First 1

if ($dllPath) {
    Copy-Item $dllPath.FullName $pluginsDir -Force
    Write-Host ("Using DLL: {0} ({1})" -f $dllPath.FullName, $dllPath.LastWriteTime)
}

Write-Host "Packaged files at: $packageRoot"
if (-not $dllPath) {
    Write-Host 'DLL not found in expected build output paths; copy ESPExplorerAE.dll manually into dist\package\Data\F4SE\Plugins\'
}
