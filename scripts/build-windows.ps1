[CmdletBinding()]
param(
    [string]$Tag = ''
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
if ($env:OS -ne 'Windows_NT') { throw 'The AHK compiler requires Windows.' }
$versionSource = Get-Content -Raw -LiteralPath (Join-Path $projectRoot 'app\ahk\core\Version.ahk')
$versionMatch = [regex]::Match($versionSource, 'return "((?:0|[1-9][0-9]{0,3})\.(?:0|[1-9][0-9]{0,3})\.(?:0|[1-9][0-9]{0,3}))"')
if (-not $versionMatch.Success) { throw 'Missing stable version in core/Version.ahk' }
$version = $versionMatch.Groups[1].Value
if ($Tag -and $Tag -cne "d4box-v$version") { throw "Tag must match source version: d4box-v$version" }

& (Join-Path $PSScriptRoot 'prepare-ahk-tools.ps1')
$toolsRoot = Join-Path $projectRoot '.runtime\ahk-build-tools'
$compiler = Join-Path $toolsRoot 'Ahk2Exe-1.1.37.02a2\Ahk2Exe.exe'
$base = Join-Path $toolsRoot 'AutoHotkey-2.0.31\AutoHotkey64.exe'
if (-not (Test-Path -LiteralPath $compiler) -or -not (Test-Path -LiteralPath $base)) { throw 'Compiler or x64 base missing' }

# A fresh staging directory prevents personal INI files, templates or logs entering a package.
$stage = Join-Path $projectRoot ('.runtime\package\' + [guid]::NewGuid().ToString('N'))
$source = Join-Path $stage 'source'
$portable = Join-Path $stage 'portable'
$probe = Join-Path $stage 'standalone probe'
New-Item -ItemType Directory -Force -Path $source,$portable,$probe | Out-Null
$ahkRoot = Join-Path $projectRoot 'app\ahk'
Copy-Item -LiteralPath (Join-Path $ahkRoot 'D4Box.ahk') -Destination $source
foreach ($folder in @('core','functions','gui','hotkeys','utils','config')) {
    Copy-Item -LiteralPath (Join-Path $ahkRoot $folder) -Destination $source -Recurse
}
$licenses = Join-Path $source 'licenses'
New-Item -ItemType Directory -Path $licenses | Out-Null
Copy-Item -LiteralPath (Join-Path $projectRoot 'LICENSES\D4KeyHelp-MIT.txt') -Destination $licenses
Copy-Item -LiteralPath (Join-Path $projectRoot 'THIRD_PARTY_NOTICES.md') -Destination $licenses
Copy-Item -LiteralPath (Join-Path $toolsRoot 'AutoHotkey-2.0.31\license.txt') -Destination (Join-Path $licenses 'AutoHotkey-license.txt')

$entry = Join-Path $source 'D4Box.ahk'
$directives = ";@Ahk2Exe-SetVersion $version.0`r`n;@Ahk2Exe-SetProductVersion $version`r`n;@Ahk2Exe-SetProductName d4box`r`n;@Ahk2Exe-SetDescription d4box workflow and macro tool`r`n"
[IO.File]::WriteAllText($entry, $directives + [IO.File]::ReadAllText($entry), [Text.UTF8Encoding]::new($true))
$exe = Join-Path $portable 'D4Box.exe'
$arguments = '/in "' + $entry + '" /out "' + $exe + '" /base "' + $base + '" /compress 0 /silent verbose'
$compile = Start-Process -FilePath $compiler -ArgumentList $arguments -WorkingDirectory $source -WindowStyle Hidden -PassThru
if (-not $compile.WaitForExit(120000)) { $compile.Kill(); throw 'Ahk2Exe timed out' }
if ($compile.ExitCode -ne 0 -or -not (Test-Path -LiteralPath $exe -PathType Leaf)) { throw "Ahk2Exe failed: $($compile.ExitCode)" }
$info = (Get-Item -LiteralPath $exe).VersionInfo
if ($info.ProductVersion -ne $version -or $info.FileVersion -ne "$version.0") { throw 'Compiled version metadata mismatch' }
$bytes = [IO.File]::ReadAllBytes($exe)
$peOffset = [BitConverter]::ToInt32($bytes, 0x3c)
if ([BitConverter]::ToUInt16($bytes, $peOffset + 4) -ne 0x8664) { throw 'Expected x64 PE executable' }

# Only the EXE is copied. This checks embedded defaults/licenses with no source tree beside it.
$probeExe = Join-Path $probe 'D4Box.exe'
Copy-Item -LiteralPath $exe -Destination $probeExe
$process = Start-Process -FilePath $probeExe -ArgumentList '--self-test' -WorkingDirectory $probe -WindowStyle Hidden -PassThru
if (-not $process.WaitForExit(30000)) { $process.Kill(); throw 'Standalone EXE probe timed out' }
$report = Join-Path $probe '.runtime\self-test.txt'
if ($process.ExitCode -ne 0 -or -not (Test-Path -LiteralPath $report)) { throw "Standalone EXE probe failed: $($process.ExitCode); see $report" }
if ((Get-Content -Raw -LiteralPath $report).Trim() -ne "PASS $version compiled=1") { throw "Unexpected EXE probe report: $report" }

Copy-Item -LiteralPath $licenses -Destination $portable -Recurse
Copy-Item -LiteralPath (Join-Path $projectRoot 'docs\portable-windows.txt') -Destination (Join-Path $portable 'README.txt')
$out = Join-Path $projectRoot '.runtime\dist'
New-Item -ItemType Directory -Force -Path $out | Out-Null
$stem = "D4Box-v$version-windows-x64"
Copy-Item -LiteralPath $exe -Destination (Join-Path $out "$stem.exe") -Force
Compress-Archive -Path (Join-Path $portable '*') -DestinationPath (Join-Path $out "$stem.zip") -Force
$sums = foreach ($name in @("$stem.exe", "$stem.zip")) {
    (Get-FileHash -LiteralPath (Join-Path $out $name) -Algorithm SHA256).Hash.ToLowerInvariant() + '  ' + $name
}
[IO.File]::WriteAllLines((Join-Path $out 'SHA256SUMS.txt'), $sums, [Text.UTF8Encoding]::new($false))
Write-Output "Built and verified d4box $version : $out"
