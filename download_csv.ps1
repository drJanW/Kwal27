# Download CSV files from ESP32 SD card
# Usage: .\download_csv.ps1 [marmer|hout] [-dest ".\sd_downloads"]
# API: GET /api/sd/file?path=/<filename>

param(
    [string]$device = "marmer",
    [string]$dest = ".\sd_downloads"
)

$ips = @{ marmer = "192.168.2.188"; hout = "192.168.2.189" }
$ip = $ips[$device.ToLower()]
if (-not $ip) {
    Write-Host "Unknown device '$device'. Use 'marmer' or 'hout'." -ForegroundColor Red
    exit 1
}

$baseUrl = "http://$ip"

# Known CSV files on the SD card
$knownFiles = @(
    "audioShifts.csv",
    "calendar.csv",
    "colorsShifts.csv",
    "globals.csv",
    "light_colors.csv",
    "light_patterns.csv",
    "patternShifts.csv",
    "theme_boxes.csv"
)

# config.txt is per-device: download as config_<device>.txt
$downloadConfig = $true

# Create destination folder
if (!(Test-Path $dest)) {
    New-Item -ItemType Directory -Path $dest | Out-Null
    Write-Host "Created folder: $dest"
}

# Check SD status first
Write-Host "Checking SD status on $ip ..."
try {
    $status = Invoke-RestMethod -Uri "$baseUrl/api/sd/status" -TimeoutSec 10
    if (-not $status.ready) {
        Write-Host "SD card not ready!" -ForegroundColor Red
        exit 1
    }
    if ($status.busy) {
        Write-Host "SD card is busy, try again later" -ForegroundColor Yellow
        exit 1
    }
    Write-Host "SD ready" -ForegroundColor Green
}
catch {
    Write-Host "Cannot reach device at $ip : $($_.Exception.Message)" -ForegroundColor Red
    exit 1
}

$ok = 0
$fail = 0

foreach ($fileName in $knownFiles) {
    $downloadUrl = "$baseUrl/api/sd/file?path=/$fileName"
    $localPath = Join-Path $dest $fileName

    Write-Host "Downloading $fileName ... " -NoNewline
    try {
        Invoke-WebRequest -Uri $downloadUrl -OutFile $localPath -TimeoutSec 60
        $size = (Get-Item $localPath).Length
        Write-Host "OK ($size bytes)" -ForegroundColor Green
        $ok++
    }
    catch {
        Write-Host "FAILED" -ForegroundColor Yellow
        $fail++
    }
}

Write-Host "`nDone: $ok downloaded, $fail skipped. Files in: $dest"

# Download config.txt and save as config_<device>.txt
if ($downloadConfig) {
    $configUrl = "$baseUrl/api/sd/file?path=/config.txt"
    $configLocal = Join-Path $dest "config_$($device.ToLower()).txt"
    Write-Host "Downloading config.txt as config_$($device.ToLower()).txt ... " -NoNewline
    try {
        Invoke-WebRequest -Uri $configUrl -OutFile $configLocal -TimeoutSec 60
        $size = (Get-Item $configLocal).Length
        Write-Host "OK ($size bytes)" -ForegroundColor Green
    }
    catch {
        Write-Host "FAILED" -ForegroundColor Yellow
    }
}
