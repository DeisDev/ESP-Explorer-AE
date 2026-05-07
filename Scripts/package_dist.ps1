$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $PSScriptRoot
$distRoot = Join-Path $projectRoot 'dist'
$packageRoot = Join-Path $distRoot 'package\Data'

$pluginsDir = Join-Path $packageRoot 'F4SE\Plugins'
$langDir = Join-Path $packageRoot 'Interface\ESPExplorerAE\lang'
$fontsDir = Join-Path $packageRoot 'Interface\ESPExplorerAE\fonts'
$themesDir = Join-Path $packageRoot 'Interface\ESPExplorerAE\themes'

if (Test-Path $packageRoot) {
    Remove-Item $packageRoot -Recurse -Force
}

New-Item -ItemType Directory -Path $pluginsDir -Force | Out-Null
New-Item -ItemType Directory -Path $langDir -Force | Out-Null
New-Item -ItemType Directory -Path $fontsDir -Force | Out-Null
New-Item -ItemType Directory -Path $themesDir -Force | Out-Null

Copy-Item (Join-Path $distRoot 'lang\*.ini') $langDir -Force
Copy-Item (Join-Path $distRoot 'fonts\*.ttf') $fontsDir -Force
Copy-Item (Join-Path $distRoot 'themes\*.ini') $themesDir -Force

$dllCandidates = @(
    (Join-Path $projectRoot 'build\windows\x64\release\ESPExplorerAE.dll')
    (Join-Path $projectRoot 'build\windows\x64\releasedbg\ESPExplorerAE.dll')
    (Join-Path $projectRoot 'build\windows\x64\release\ESPExplorerPreNG.dll')
    (Join-Path $projectRoot 'build\windows\x64\releasedbg\ESPExplorerPreNG.dll')
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
    Write-Host 'DLL not found in expected build output paths; copy the plugin DLL manually into dist\package\Data\F4SE\Plugins\'
}

$dllVersion = $null
if ($dllPath) {
    $vi = [System.Diagnostics.FileVersionInfo]::GetVersionInfo($dllPath.FullName)
    if ($vi.FileVersion) {
        $parts = $vi.FileVersion -split '[,.\s]' | Where-Object { $_ -ne '' }
        if ($parts.Count -ge 3) {
            $dllVersion = "$($parts[0])-$($parts[1])-$($parts[2])"
        }
    }
}

$branch = $null
try {
    Push-Location $projectRoot
    $branch = (git rev-parse --abbrev-ref HEAD 2>$null)
    Pop-Location
} catch {
    Pop-Location
}

if ($branch -eq 'pre-ng') {
    $archiveName = 'ESP Explorer Pre-NG'
} else {
    $archiveName = 'ESP Explorer AE'
}

if ($dllVersion) {
    $archiveName = "$archiveName $dllVersion"
}

$archivePath = Join-Path $distRoot "package\$archiveName.7z"

$sevenZipCommand = @('7z', '7za', '7zr') |
    ForEach-Object { Get-Command $_ -ErrorAction SilentlyContinue } |
    Select-Object -First 1

if (-not $sevenZipCommand) {
    $tarCommand = Get-Command tar -ErrorAction SilentlyContinue
    if (-not $tarCommand) {
        Write-Host '7-Zip CLI not found on PATH; skipping .7z creation.'
        Write-Host 'Install 7-Zip and ensure 7z.exe is on PATH to enable archive output.'
        return
    }

    Get-ChildItem (Join-Path $distRoot 'package') -Filter '*.7z' | Remove-Item -Force

    Push-Location $distRoot
    try {
        & $tarCommand.Source -a -c -f $archivePath -C (Join-Path $distRoot 'package') 'Data'
        if ($LASTEXITCODE -ne 0) {
            throw "tar failed with exit code $LASTEXITCODE"
        }
    }
    finally {
        Pop-Location
    }

    if (Test-Path $archivePath) {
        Write-Host "Created archive: $archivePath"
        return
    }

    Write-Host 'Failed to create .7z with tar fallback.'
    return
}

Get-ChildItem (Join-Path $distRoot 'package') -Filter '*.7z' | Remove-Item -Force

Push-Location (Join-Path $distRoot 'package')
try {
    & $sevenZipCommand.Source a -t7z $archivePath 'Data' -mx=9
    if ($LASTEXITCODE -ne 0) {
        throw "7-Zip failed with exit code $LASTEXITCODE"
    }
}
finally {
    Pop-Location
}

Write-Host "Created archive: $archivePath"
