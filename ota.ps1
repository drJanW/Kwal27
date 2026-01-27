# OTA firmware upload script
# Usage: .\ota.ps1 [lastOctet]  (default: 188)

param(
    [int]$lastOctet = 188
)

$ip = "192.168.2.$lastOctet"

Write-Host "[OTA] Target: $ip" -ForegroundColor Cyan
Write-Host "[OTA] Building firmware for esp32_v3_ota..." -ForegroundColor Cyan
pio run -e esp32_v3_ota
if ($LASTEXITCODE -ne 0) {
    Write-Host "[OTA] Build failed, aborting upload." -ForegroundColor Red
    exit $LASTEXITCODE
}

Write-Host ""
Write-Host "[OTA] Build complete." -ForegroundColor Green
Write-Host "[OTA] STAPPEN:" -ForegroundColor Yellow
Write-Host "[OTA]   1. Open WebGUI -> Dev -> OTA -> klik 'Start OTA'" -ForegroundColor Yellow
Write-Host "[OTA]   2. Wacht 30 seconden (reboot + WiFi + OTA init)" -ForegroundColor Yellow
Write-Host "[OTA]   3. Druk dan hier Enter" -ForegroundColor Yellow
Read-Host "Druk Enter als ESP klaar is voor OTA"

Write-Host "[OTA] Uploaden via OTA naar $ip..." -ForegroundColor Cyan
pio run -e esp32_v3_ota -t nobuild -t upload --upload-port $ip
exit $LASTEXITCODE
