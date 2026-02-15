# Upload all CSV files from sdroot to ESP32 SD card
# Usage: .\upload_csv.ps1 [lastOctet]  or  .\upload_csv.ps1 -ip <full-ip>

param(
    [int]$lastOctet = 188,
    [string]$ip = ""
)

if ($ip -eq "") {
    $ip = "192.168.2.$lastOctet"
}
$baseUrl = "http://$ip/api/sd/upload"
$sdroot = Join-Path $PSScriptRoot "sdroot"

# Determine device name from IP for wifi file selection
$device = if ($lastOctet -eq 189 -or $ip -eq "192.168.2.189") { "hout" } else { "marmer" }

$csvFiles = Get-ChildItem -Path $sdroot -Filter "*.csv" -File
$allFiles = @($csvFiles)

if ($allFiles.Count -eq 0) {
    Write-Host "No CSV files found in $sdroot" -ForegroundColor Yellow
    exit 0
}

Write-Host "Uploading $($allFiles.Count) CSV files + wifi to $ip ($device)..." -ForegroundColor Cyan

$success = 0
$failed = 0

foreach ($file in $allFiles) {
    Write-Host -NoNewline "  $($file.Name)... "
    
    try {
        $result = curl.exe -s -X POST -F "file=@$($file.FullName)" $baseUrl
        
        if ($LASTEXITCODE -eq 0) {
            Write-Host "OK" -ForegroundColor Green
            $success++
        } else {
            Write-Host "FAILED (curl exit $LASTEXITCODE)" -ForegroundColor Red
            $failed++
        }
    }
    catch {
        Write-Host "ERROR: $_" -ForegroundColor Red
        $failed++
    }
}

Write-Host ""
Write-Host "Done: $success uploaded, $failed failed" -ForegroundColor $(if ($failed -eq 0) { "Green" } else { "Yellow" })

# Upload device-specific wifi file as wifi.txt
$wifiSource = Join-Path $sdroot "wifi_$device.txt"
if (Test-Path $wifiSource) {
    Write-Host "  wifi_$device.txt as wifi.txt..." -NoNewline
    try {
        $result = curl.exe -s -X POST -F "file=@$wifiSource;filename=wifi.txt" $baseUrl
        if ($LASTEXITCODE -eq 0) {
            Write-Host " OK" -ForegroundColor Green
        } else {
            Write-Host " FAILED" -ForegroundColor Red
        }
    }
    catch {
        Write-Host " ERROR: $_" -ForegroundColor Red
    }
} else {
    Write-Host "  WARNING: wifi_$device.txt not found" -ForegroundColor Yellow
}
