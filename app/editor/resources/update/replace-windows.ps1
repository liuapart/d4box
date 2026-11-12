param(
    [Parameter(Mandatory=$true)][int]$PidToWait,
    [Parameter(Mandatory=$true)][string]$InstallRoot,
    [Parameter(Mandatory=$true)][string]$Payload,
    [Parameter(Mandatory=$true)][string]$Installed,
    [string]$ConfigPath = ''
)
$ErrorActionPreference = 'Stop'
$log = Join-Path $PSScriptRoot 'update-error.txt'
try {
    for ($attempt = 0; $attempt -lt 240; $attempt++) {
        if (!(Get-Process -Id $PidToWait -ErrorAction SilentlyContinue)) { break }
        Start-Sleep -Milliseconds 500
    }
    if (Get-Process -Id $PidToWait -ErrorAction SilentlyContinue) {
        throw 'Editor did not exit within two minutes.'
    }
    if ($Installed -eq '1') {
        $installer = Start-Process -FilePath $Payload -Wait -PassThru -ArgumentList @(
            '/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', '/SP-', '/CLOSEAPPLICATIONS'
        )
        if ($installer.ExitCode -ne 0) { throw "Installer exit code: $($installer.ExitCode)" }
    } else {
        if (!(Test-Path -LiteralPath (Join-Path $Payload 'D4BoxEditor.exe'))) {
            throw 'Verified portable payload is missing.'
        }
        $candidate = "$InstallRoot.updating-$PidToWait"
        $backup = "$InstallRoot.previous-$PidToWait"
        if ((Test-Path -LiteralPath $candidate) -or (Test-Path -LiteralPath $backup)) {
            throw 'Backup or update candidate already exists.'
        }
        Copy-Item -LiteralPath $Payload -Destination $candidate -Recurse
        # Keep user-created files beside a portable copy, including workflow JSON.
        Get-ChildItem -LiteralPath $InstallRoot -File -Recurse | ForEach-Object {
            if (($_.Attributes -band [IO.FileAttributes]::ReparsePoint) -eq 0) {
                $relative = $_.FullName.Substring($InstallRoot.Length).TrimStart('\')
                $newFile = Join-Path $candidate $relative
                if (!(Test-Path -LiteralPath $newFile)) {
                    New-Item -ItemType Directory -Force -Path (Split-Path $newFile) | Out-Null
                    Copy-Item -LiteralPath $_.FullName -Destination $newFile
                }
            }
        }
        Rename-Item -LiteralPath $InstallRoot -NewName (Split-Path $backup -Leaf)
        try {
            Rename-Item -LiteralPath $candidate -NewName (Split-Path $InstallRoot -Leaf)
        } catch {
            Rename-Item -LiteralPath $backup -NewName (Split-Path $InstallRoot -Leaf)
            throw
        }
    }
    $editor = Join-Path $InstallRoot 'D4BoxEditor.exe'
    if (!(Test-Path -LiteralPath $editor)) { throw 'Updated editor executable is missing.' }
    if ($env:D4BOX_UPDATE_TEST_ONLY -eq '1') {
        $test = Start-Process -FilePath $editor -ArgumentList '--smoke-test' -Wait -PassThru
        if ($test.ExitCode -ne 0) { throw "Updated editor smoke exit code: $($test.ExitCode)" }
    } elseif ($ConfigPath) {
        $quoted = '"' + $ConfigPath.Replace('"', '') + '"'
        Start-Process -FilePath $editor -ArgumentList $quoted
    } else {
        Start-Process -FilePath $editor
    }
} catch {
    $_ | Out-String | Set-Content -LiteralPath $log -Encoding UTF8
    if ($env:D4BOX_UPDATE_TEST_ONLY -ne '1') {
        $oldEditor = Join-Path $InstallRoot 'D4BoxEditor.exe'
        if (Test-Path -LiteralPath $oldEditor) {
            if ($ConfigPath) {
                Start-Process -FilePath $oldEditor -ArgumentList ('"' + $ConfigPath.Replace('"', '') + '"')
            } else {
                Start-Process -FilePath $oldEditor
            }
        }
    }
    exit 1
}
