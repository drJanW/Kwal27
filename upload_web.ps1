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

# Device-specific wifi file → uploaded as wifi.txt
$wifiSource = Join-Path $webDir "wifi_$target.txt"
if (-not (Test-Path $wifiSource)) {
    Write-Host "  WARNING: wifi_$target.txt not found" -ForegroundColor Yellow
}

Write-Host "Uploading web files to $ESP32_IP ($target)..." -ForegroundColor Cyan

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

# Upload wifi file as wifi.txt
if (Test-Path $wifiSource) {
    Write-Host "  Uploading wifi_$target.txt as wifi.txt..." -NoNewline
    try {
        curl -X POST -F "file=@$wifiSource;filename=wifi.txt" -F "path=/" $UPLOAD_URL --silent --show-error
        Write-Host " OK" -ForegroundColor Green
    }
    catch {
        Write-Host " FAILED" -ForegroundColor Red
    }
}

Write-Host "`nDone." -ForegroundColor Cyan
