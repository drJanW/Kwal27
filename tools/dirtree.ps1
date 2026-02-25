<#
.SYNOPSIS
    Generate directory tree listing (skips dot-folders like .git)
.EXAMPLE
    .\dirtree.ps1                     # Current directory
    .\dirtree.ps1 lib                 # Specific folder
    .\dirtree.ps1 -Files              # Include files
    .\dirtree.ps1 -Out tree.txt       # Save to file
#>

param(
    [string]$Path = ".",
    [switch]$Files,
    [string]$Out
)

function Write-Tree {
    param(
        [string]$CurrentPath,
        [string]$Indent = "  "
    )
    
    # Subdirectories (skip names starting with .)
    Get-ChildItem -Path $CurrentPath -Directory -ErrorAction SilentlyContinue | 
        Where-Object { $_.Name -notmatch '^\.' } |
        Sort-Object Name |
        ForEach-Object {
            "$Indent$($_.Name)/"
            Write-Tree -CurrentPath $_.FullName -Indent "  $Indent"
        }
    
    # Optionally show files
    if ($Files) {
        Get-ChildItem -Path $CurrentPath -File -ErrorAction SilentlyContinue |
            Sort-Object Name |
            ForEach-Object {
                "$Indent$($_.Name)"
            }
    }
}

# Resolve path
$ResolvedPath = Resolve-Path $Path -ErrorAction Stop
$RootName = Split-Path $ResolvedPath -Leaf

# Build output
$output = @(
    "Folder PATH listing"
    $ResolvedPath.Path
    Write-Tree -CurrentPath $ResolvedPath.Path
) | Where-Object { $_ }  # Remove nulls

# Output
if ($Out) {
    $output | Out-File -FilePath $Out -Encoding UTF8
    Write-Host "Wrote tree to `"$Out`""
} else {
    $output | ForEach-Object { Write-Host $_ }
}
