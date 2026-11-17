[CmdletBinding()]
param(
    [string]$AhkPath
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot

if ($env:OS -ne 'Windows_NT') {
    throw 'This launcher requires Windows and AutoHotkey v2.'
}

if (-not $AhkPath) {
    $candidates = @(
        (Join-Path $projectRoot '.runtime\autohotkey\v2\AutoHotkey64.exe'),
        (Join-Path $env:ProgramFiles 'AutoHotkey\v2\AutoHotkey64.exe'),
        (Join-Path $env:ProgramFiles 'AutoHotkey\v2\AutoHotkey32.exe'),
        (Join-Path $env:LOCALAPPDATA 'Programs\AutoHotkey\v2\AutoHotkey64.exe')
    )
    $AhkPath = $candidates | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } | Select-Object -First 1
}

if (-not $AhkPath -or -not (Test-Path -LiteralPath $AhkPath -PathType Leaf)) {
    if ($AhkPath) { throw "AutoHotkey v2 executable not found: $AhkPath" }
    throw 'Extract AutoHotkey v2 to .runtime\autohotkey\v2, install it system-wide, or pass -AhkPath with an existing executable path.'
}
$AhkPath = (Resolve-Path -LiteralPath $AhkPath).Path
if ((Get-Item -LiteralPath $AhkPath).VersionInfo.FileMajorPart -ne 2) {
    throw 'The selected executable must be the AutoHotkey v2 interpreter.'
}

$appScript = Join-Path $projectRoot 'app\ahk\D4Box.ahk'
if (-not (Test-Path -LiteralPath $appScript -PathType Leaf)) {
    throw "Missing application entry: $appScript"
}

$process = Start-Process -FilePath $AhkPath -ArgumentList @('"' + $appScript + '"') `
    -WorkingDirectory (Split-Path -Parent $appScript) -PassThru
Write-Host "Started d4box PID $($process.Id): $appScript"
Write-Host 'In Diablo IV, F1 toggles the macro; Ctrl+Alt+F8 shows window diagnostics.'
