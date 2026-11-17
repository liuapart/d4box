[CmdletBinding()]
param([string]$AhkPath)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
if (-not $AhkPath) { $AhkPath = Join-Path $projectRoot '.runtime\autohotkey\v2\AutoHotkey64.exe' }
if (-not (Test-Path -LiteralPath $AhkPath -PathType Leaf)) { throw 'Provide -AhkPath with an AutoHotkey v2 executable.' }
$AhkPath = (Resolve-Path -LiteralPath $AhkPath).Path
$stage = Join-Path $projectRoot ('.runtime\help-capture\' + [guid]::NewGuid().ToString('N'))
$app = Join-Path $stage 'app'
$raw = Join-Path $stage 'captures'
New-Item -ItemType Directory -Force -Path $app,$raw | Out-Null
foreach ($folder in @('core','functions','gui','hotkeys','utils','config')) {
    Copy-Item -LiteralPath (Join-Path $projectRoot "app\ahk\$folder") -Destination $app -Recurse
}
$includes = @(Get-Content -LiteralPath (Join-Path $projectRoot 'app\ahk\D4Box.ahk') | Where-Object { $_ -match '^#Include' -and $_ -notmatch 'GameHotkeys' })
$body = Get-Content -Raw -LiteralPath (Join-Path $PSScriptRoot 'help\CaptureHelp.ahk')
$entry = Join-Path $app 'CaptureHelp.ahk'
$source = "#Requires AutoHotkey v2.0`r`n#SingleInstance Off`r`n#NoTrayIcon`r`n" + ($includes -join "`r`n") + "`r`n" + $body
[IO.File]::WriteAllText($entry, $source, [Text.UTF8Encoding]::new($true))
$process = Start-Process -FilePath $AhkPath -ArgumentList ('/ErrorStdOut "' + $entry + '" "' + $raw + '"') -WorkingDirectory $app -WindowStyle Hidden -PassThru
if (-not $process.WaitForExit(65000)) { $process.Kill(); throw 'Documentation capture timed out' }
$report = Join-Path $raw 'capture-result.txt'
if ($process.ExitCode -ne 0 -or -not (Test-Path -LiteralPath $report)) {
    if (Test-Path -LiteralPath $report) { Get-Content -LiteralPath $report }
    throw "Documentation capture failed: $stage"
}
$images = @(Get-ChildItem -LiteralPath $raw -Filter '*.bmp')
if ($images.Count -ne 18) { throw "Expected 18 screenshots; found $($images.Count)" }
$destination = Join-Path $projectRoot 'docs\guide\images'
New-Item -ItemType Directory -Force -Path $destination | Out-Null
Add-Type -AssemblyName System.Drawing
foreach ($file in $images) {
    $bitmap = [Drawing.Image]::FromFile($file.FullName)
    try {
        $bitmap.Save((Join-Path $destination ($file.BaseName + '.png')), [Drawing.Imaging.ImageFormat]::Png)
    } finally { $bitmap.Dispose() }
}
Get-Content -LiteralPath $report
Write-Output "Published 18 screenshots to $destination. Only isolated demonstration data was used."
