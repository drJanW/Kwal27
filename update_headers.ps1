<# 
    update_headers.ps1
    Add/update standard Doxygen headers in .cpp and .h files
    Uses git log date for version, or filesystem date as fallback
#>

param(
    [switch]$DryRun,      # Show what would change without modifying
    [switch]$Force        # Overwrite existing headers
)

$projectRoot = "d:\My Documents\PlatformIO\ESP32\Projects\Kwal27"
$excludePatterns = @('\.pio', '\.git', 'node_modules')

function Get-FileVersion {
    param([string]$FilePath)
    
    # Try git date first
    $gitDate = git -C $projectRoot log -1 --format="%ci" -- $FilePath 2>$null
    if ($gitDate) {
        $date = [DateTime]::Parse($gitDate.Substring(0, 10))
    } else {
        # Fallback to filesystem
        $date = (Get-Item $FilePath).LastWriteTime
    }
    
    $version = $date.ToString("yyMMdd") + "A"
    $dateStr = $date.ToString("yyyy-MM-dd")
    
    return @{ Version = $version; Date = $dateStr }
}

function Get-BriefFromFile {
    param([string]$FilePath)
    
    $filename = [System.IO.Path]::GetFileNameWithoutExtension($FilePath)
    $ext = [System.IO.Path]::GetExtension($FilePath)
    
    # Try to extract from existing @brief or first meaningful comment
    $content = Get-Content $FilePath -Raw -ErrorAction SilentlyContinue
    if ($content -match '@brief\s+(.+?)[\r\n]') {
        return $matches[1].Trim()
    }
    
    # Default based on filename
    return "$filename implementation"
}

function Has-DoxygenHeader {
    param([string]$Content)
    return $Content -match '^\s*/\*\*[\s\S]*?@file'
}

function Update-FileHeader {
    param(
        [string]$FilePath,
        [switch]$DryRun
    )
    
    $content = Get-Content $FilePath -Raw
    $filename = [System.IO.Path]::GetFileName($FilePath)
    $relPath = $FilePath -replace [regex]::Escape("$projectRoot\"), ""
    
    $versionInfo = Get-FileVersion -FilePath $FilePath
    $brief = Get-BriefFromFile -FilePath $FilePath
    
    $newHeader = @"
/**
 * @file $filename
 * @brief $brief
 * @version $($versionInfo.Version)
 * @date $($versionInfo.Date)
 */

"@

    if (Has-DoxygenHeader -Content $content) {
        # Update existing header
        $pattern = '^\s*/\*\*[\s\S]*?@date\s+[\d-]+[\s\S]*?\*/\s*'
        if ($content -match $pattern) {
            $newContent = $content -replace $pattern, $newHeader
            $action = "UPDATE"
        } else {
            Write-Host "  SKIP (malformed header): $relPath" -ForegroundColor Yellow
            return
        }
    } else {
        # Add new header
        # Skip #pragma once or #ifndef guards - insert after
        if ($content -match '^(\s*#pragma once\s*[\r\n]+)') {
            $newContent = $matches[1] + $newHeader + ($content -replace '^(\s*#pragma once\s*[\r\n]+)', '')
        } elseif ($content -match '^(\s*#ifndef\s+\w+\s*[\r\n]+\s*#define\s+\w+\s*[\r\n]+)') {
            $newContent = $matches[1] + $newHeader + ($content -replace '^(\s*#ifndef\s+\w+\s*[\r\n]+\s*#define\s+\w+\s*[\r\n]+)', '')
        } else {
            $newContent = $newHeader + $content
        }
        $action = "ADD"
    }
    
    if ($DryRun) {
        Write-Host "  $action $relPath -> v$($versionInfo.Version)" -ForegroundColor Cyan
    } else {
        Set-Content -Path $FilePath -Value $newContent -NoNewline
        Write-Host "  $action $relPath -> v$($versionInfo.Version)" -ForegroundColor Green
    }
}

# Main
Write-Host "`nScanning for .cpp and .h files..." -ForegroundColor White

$files = Get-ChildItem -Path $projectRoot -Recurse -Include "*.cpp","*.h" | 
    Where-Object { 
        $path = $_.FullName
        -not ($excludePatterns | Where-Object { $path -match $_ })
    }

Write-Host "Found $($files.Count) files`n" -ForegroundColor White

if ($DryRun) {
    Write-Host "=== DRY RUN ===" -ForegroundColor Yellow
}

$files | ForEach-Object {
    Update-FileHeader -FilePath $_.FullName -DryRun:$DryRun
}

Write-Host "`nDone." -ForegroundColor Green
if ($DryRun) {
    Write-Host "Run without -DryRun to apply changes." -ForegroundColor Yellow
}
