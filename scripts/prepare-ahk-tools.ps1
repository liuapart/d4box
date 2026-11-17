[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$toolsRoot = Join-Path $projectRoot '.runtime\ahk-build-tools'
New-Item -ItemType Directory -Force -Path $toolsRoot | Out-Null

# Official release archives; hashes pinned to the assets reviewed for this build.
$archives = @(
    @{
        Name = 'AutoHotkey_2.0.31.zip'; Folder = 'AutoHotkey-2.0.31'
        Url = 'https://github.com/AutoHotkey/AutoHotkey/releases/download/v2.0.31/AutoHotkey_2.0.31.zip'
        Hash = 'ca1a595aca4528755d8d4cd7fed23d497a37bd9a0b6892072612d68dee1bd00a'
    },
    @{
        Name = 'Ahk2Exe1.1.37.02a2.zip'; Folder = 'Ahk2Exe-1.1.37.02a2'
        Url = 'https://github.com/AutoHotkey/Ahk2Exe/releases/download/Ahk2Exe1.1.37.02a2/Ahk2Exe1.1.37.02a2.zip'
        Hash = 'c29b8c3a5124850d79fc9e66e2ca79677c377d7f31631ad3022ba159c5d9e3be'
    }
)
foreach ($archive in $archives) {
    $zip = Join-Path $toolsRoot $archive.Name
    if (-not (Test-Path -LiteralPath $zip -PathType Leaf)) {
        Invoke-WebRequest -Uri $archive.Url -OutFile $zip -TimeoutSec 120
    }
    if ((Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash.ToLowerInvariant() -ne $archive.Hash) {
        throw "Tool archive checksum mismatch: $zip. Remove the damaged archive and retry."
    }
    # Re-expand the verified archive so stale/modified local binaries cannot be used.
    Expand-Archive -LiteralPath $zip -DestinationPath (Join-Path $toolsRoot $archive.Folder) -Force
}
Write-Output "Verified AHK and Ahk2Exe tools: $toolsRoot"
