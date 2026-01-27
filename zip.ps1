# makezip.ps1 — volledig dynamisch, alleen .gitignore bepaalt filtering

$ErrorActionPreference = "Stop"

# Project-root
$rootPath = (Get-Location).ProviderPath
$dirName  = Split-Path -Leaf $rootPath
$ts       = Get-Date -Format "yyyyMMdd_HHmmss"

# Zip één niveau hoger
$parentPath = Split-Path $rootPath -Parent
if (-not $parentPath) { $parentPath = $rootPath }
$outZip = Join-Path $parentPath ("{0}_{1}.zip" -f $dirName, $ts)

# .gitignore lezen
$gitignorePath = Join-Path $rootPath ".gitignore"
if (-not (Test-Path $gitignorePath)) {
    Write-Error ".gitignore niet gevonden in $rootPath"
    exit 1
}

# Regels inladen
$patterns =
    Get-Content $gitignorePath |
    Where-Object { $_ -and -not $_.Trim().StartsWith("#") } |
    ForEach-Object { $_.Trim() }

function Test-Ignored {
    param(
        [string] $relPath,
        [string[]] $patterns
    )

    # normaliseer pad
    $rel = $relPath -replace "\\", "/"

    foreach ($raw in $patterns) {
        $p = $raw.Trim()
        if (-not $p) { continue }

        $anchorRoot = $false
        if ($p.StartsWith("/")) {
            $anchorRoot = $true
            $p = $p.TrimStart("/")
        }

        $isDirPattern = $p.EndsWith("/")
        if ($isDirPattern) {
            # b.v. ".vscode/" → ".vscode"
            $dirName = $p.TrimEnd("/")

            if ($anchorRoot) {
                if ($rel -eq $dirName -or $rel -like "$dirName/*") {
                    return $true
                }
            } else {
                if ($rel -eq $dirName -or
                    $rel -like "$dirName/*" -or
                    $rel -like "*/$dirName" -or
                    $rel -like "*/$dirName/*") {
                    return $true
                }
            }
        }
        else {
            # bestands-patterns, b.v. "*.pyc", "*.exe"
            if ($anchorRoot) {
                if ($rel -like $p -or $rel -like "$p*") {
                    return $true
                }
            } else {
                if ($rel -like $p -or $rel -like "*/$p") {
                    return $true
                }
            }
        }
    }

    return $false
}

# Alle bestanden verzamelen
$allFiles =
    Get-ChildItem -Path $rootPath -Recurse -File |
    ForEach-Object {
        $rel = $_.FullName.Substring($rootPath.Length + 1)
        [PSCustomObject]@{
            Full = $_.FullName
            Rel  = $rel
        }
    }

# Filter toepassen
$files =
    $allFiles |
    Where-Object {
        -not (Test-Ignored -relPath $_.Rel -patterns $patterns)
    }

if ($files.Count -eq 0) {
    Write-Error "Geen bestanden over na filtering (.gitignore)."
    exit 1
}

# Zip maken
Add-Type -AssemblyName System.IO.Compression.FileSystem
if (Test-Path $outZip) { Remove-Item $outZip }
$zip = [System.IO.Compression.ZipFile]::Open($outZip, 'Create')

foreach ($f in $files) {
    [System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile(
        $zip,
        $f.Full,
        $f.Rel
    )
}

$zip.Dispose()

Write-Output "OK: $outZip"
