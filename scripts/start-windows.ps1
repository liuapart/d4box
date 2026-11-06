[CmdletBinding()]
param(
    [string]$AhkPath,
    [switch]$SkipUpstream
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot

if ($env:OS -ne 'Windows_NT') {
    throw 'This launcher requires Windows and AutoHotkey v2.'
}

if (-not $AhkPath) {
    $candidates = @(
        (Join-Path $env:ProgramFiles 'AutoHotkey\v2\AutoHotkey64.exe'),
        (Join-Path $env:ProgramFiles 'AutoHotkey\v2\AutoHotkey32.exe'),
        (Join-Path $env:LOCALAPPDATA 'Programs\AutoHotkey\v2\AutoHotkey64.exe')
    )
    $AhkPath = $candidates | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } | Select-Object -First 1
}

if (-not $AhkPath -or -not (Test-Path -LiteralPath $AhkPath -PathType Leaf)) {
    throw 'Install AutoHotkey v2 or pass -AhkPath with its executable path.'
}
$AhkPath = (Resolve-Path -LiteralPath $AhkPath).Path
if ((Get-Item -LiteralPath $AhkPath).VersionInfo.FileMajorPart -ne 2) {
    throw 'The selected executable must be the AutoHotkey v2 interpreter.'
}

$extensionScript = Join-Path $projectRoot 'extensions\ahk\Main.ahk'
$upstreamScript = Join-Path $projectRoot 'vendor\D4keyHelp\D4keyHelp.ahk'
$scriptsToStart = @($extensionScript)
if (-not $SkipUpstream) {
    $scriptsToStart = @($upstreamScript, $extensionScript)
}

foreach ($script in $scriptsToStart) {
    if (-not (Test-Path -LiteralPath $script -PathType Leaf)) {
        throw "Missing script: $script. Run git submodule update --init --recursive."
    }
}

foreach ($script in $scriptsToStart) {
    $process = Start-Process -FilePath $AhkPath -ArgumentList @('"' + $script + '"') `
        -WorkingDirectory (Split-Path -Parent $script) -PassThru
    Write-Host "Started PID $($process.Id): $script"
}

Write-Host 'In Diablo IV, press Ctrl+Alt+F8 for window diagnostics. Exit each app through its tray menu.'
