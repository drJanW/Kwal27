# upload_web.ps1
# Uploads web files (index.html, styles.css, kwal.js) to ESP32 SD card
# Usage: .\upload_web.ps1 [lastOctet]  or  .\upload_web.ps1 -ip <full-ip>

param(
    [string]$target = "hout",
    [string]$ip = ""
)

if ($ip -eq "") {
    if ($target -eq "marmer") {
        $ip = "192.168.2.188"
    } else {
        $ip = "192.168.2.189"
    }
}
$ESP32_IP = $ip
$UPLOAD_URL = "http://$ESP32_IP/api/sd/upload"

$targetLabel = if ($ESP32_IP -eq "192.168.2.188") { "marmer" } elseif ($ESP32_IP -eq "192.168.2.189") { "hout" } else { $target }

$projectRoot = $PSScriptRoot
if (-not $projectRoot) {
    $projectRoot = Get-Location
}

$webDir = Join-Path $projectRoot "sdroot"

$files = @(
    "index.html",
    "styles.css",
    "kwal.js"
)

Write-Host "Uploading web files to $ESP32_IP ($targetLabel)..." -ForegroundColor Cyan

foreach ($file in $files) {
    $filePath = Join-Path $webDir $file
    if (-not (Test-Path $filePath)) {
        Write-Host "  SKIP: $file not found" -ForegroundColor Yellow
        continue
    }
    
    Write-Host "  Uploading $file..." -NoNewline
    try {
        curl -X POST -F "file=@$filePath" -F "path=/" $UPLOAD_URL --silent --show-error
        Write-Host " OK" -ForegroundColor Green
    }
    catch {
        Write-Host " FAILED" -ForegroundColor Red
    }
}

Write-Host "`nDone." -ForegroundColor Cyan
