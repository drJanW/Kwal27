# Upload config.txt naar de SD-kaart van een ESP32
# Usage: .\upload_config.ps1 <IP-adres>
# Voorbeeld: .\upload_config.ps1 192.168.2.189

param(
    [Parameter(Mandatory=$true)][string]$ip
)

$file = Join-Path $PSScriptRoot "sdroot\config.txt"

if (-not (Test-Path $file)) {
    Write-Host "Bestand niet gevonden: $file" -ForegroundColor Red
    exit 1
}

$url = "http://$ip/api/sd/upload"
Write-Host "Uploading $file als config.txt naar $ip..." -ForegroundColor Cyan

try {
    curl.exe -s -X POST -F "file=@$file;filename=config.txt" $url
    if ($LASTEXITCODE -eq 0) {
        Write-Host " OK" -ForegroundColor Green
    } else {
        Write-Host " FAILED" -ForegroundColor Red
    }
}
catch {
    Write-Host "ERROR: $_" -ForegroundColor Red
}
