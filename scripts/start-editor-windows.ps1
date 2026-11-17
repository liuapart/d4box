[CmdletBinding(PositionalBinding = $false)]
param(
    [string]$QtPrefix,
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$EditorArgs = @()
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot

if ($env:OS -ne 'Windows_NT') {
    throw 'This launcher requires Windows and the editor development prerequisites.'
}

# Reuse the same official VS 2022 discovery and environment setup as release CI.
# -products '*' includes Build Tools installations without the Visual Studio IDE.
if ($env:VSCMD_ARG_TGT_ARCH -notin @('x64', 'amd64') -or
    -not (Get-Command cl.exe -ErrorAction SilentlyContinue)) {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path -LiteralPath $vswhere -PathType Leaf)) {
        throw 'Install Visual Studio 2022 Build Tools: Desktop development with C++, MSVC v143 x64/x86 tools, and a Windows SDK. The full VS IDE is not required.'
    }
    $vsPaths = @(& $vswhere -latest -version '[17.0,18.0)' -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath)
    if ($LASTEXITCODE -ne 0 -or $vsPaths.Count -ne 1) {
        throw 'A complete Visual Studio 2022 C++ toolchain was not found. Finish or modify the Build Tools installation in Visual Studio Installer.'
    }
    $devShell = Join-Path $vsPaths[0] 'Common7\Tools\Launch-VsDevShell.ps1'
    if (-not (Test-Path -LiteralPath $devShell -PathType Leaf)) {
        throw "Missing Developer PowerShell entry: $devShell"
    }
    & $devShell -Arch amd64 -HostArch amd64 -SkipAutomaticLocation
    if ($env:VSCMD_ARG_TGT_ARCH -notin @('x64', 'amd64') -or
        -not (Get-Command cl.exe -ErrorAction SilentlyContinue)) {
        throw 'Visual Studio could not initialize the x64 C++ environment.'
    }
}

$python = Get-Command python.exe -ErrorAction SilentlyContinue
if (-not $python) {
    throw 'Python 3.10+ was not found. Install Python and ensure python.exe is available on PATH.'
}

$devScript = Join-Path $projectRoot 'scripts\dev_editor.py'
if (-not (Test-Path -LiteralPath $devScript -PathType Leaf)) {
    throw "Missing editor development entry: $devScript"
}

$arguments = @('-X', 'utf8', $devScript)
if ($QtPrefix) {
    $arguments += @('--qt-prefix', $QtPrefix)
}
if ($EditorArgs.Count -gt 0) {
    if ($EditorArgs[0] -eq '--') {
        $EditorArgs = @($EditorArgs | Select-Object -Skip 1)
    }
}
if ($EditorArgs.Count -gt 0) {
    $arguments += '--'
    $arguments += $EditorArgs
}

$previousInputEncoding = [Console]::InputEncoding
$previousOutputEncoding = [Console]::OutputEncoding
Push-Location $projectRoot
try {
    # CMake and MSVC must agree on the code page for Ninja header dependencies.
    [Console]::InputEncoding = [Text.UTF8Encoding]::new($false)
    [Console]::OutputEncoding = [Text.UTF8Encoding]::new($false)
    & $python.Source @arguments
    if ($LASTEXITCODE -ne 0) {
        throw "Editor build or launch failed with exit code $LASTEXITCODE."
    }
} finally {
    [Console]::InputEncoding = $previousInputEncoding
    [Console]::OutputEncoding = $previousOutputEncoding
    Pop-Location
}
