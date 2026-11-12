param(
    [Parameter(Mandatory=$true)][string]$Archive,
    [Parameter(Mandatory=$true)][string]$Destination
)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression.FileSystem
$zip = [System.IO.Compression.ZipFile]::OpenRead($Archive)
try {
    $seen = [System.Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach ($entry in $zip.Entries) {
        $name = $entry.FullName
        $parts = $name -split '/'
        if (!$name.StartsWith('D4BoxEditor/', [StringComparison]::Ordinal) -or
            $name.Contains('\') -or $name.Contains(':') -or
            $parts -contains '..' -or $parts -contains '.' -or
            (($entry.ExternalAttributes -shr 16) -band 0xF000) -eq 0xA000 -or
            !$seen.Add($name)) {
            throw "Unsafe or duplicate archive member: $name"
        }
    }
} finally {
    $zip.Dispose()
}
[System.IO.Compression.ZipFile]::ExtractToDirectory($Archive, $Destination)
