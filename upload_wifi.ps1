# Upload wifi.txt naar de SD-kaart van een ESP32
# Usage: .\upload_wifi.ps1 <IP-adres>
# Voorbeeld: .\upload_wifi.ps1 192.168.2.189

param(
    [Parameter(Mandatory=$true)][string]$ip
)

$file = Join-Path $PSScriptRoot "sdroot\wifi.txt"

if (-not (Test-Path $file)) {
    Write-Host "Bestand niet gevonden: $file" -ForegroundColor Red
    exit 1
}

$url = "http://$ip/api/sd/upload"
Write-Host "Uploading $file als wifi.txt naar $ip..." -ForegroundColor Cyan

try {
    curl.exe -s -X POST -F "file=@$file;filename=wifi.txt" $url
    if ($LASTEXITCODE -eq 0) {
        Write-Host " OK" -ForegroundColor Green
    } else {
        Write-Host " FAILED" -ForegroundColor Red
    }
}
catch {
    Write-Host "ERROR: $_" -ForegroundColor Red
}
