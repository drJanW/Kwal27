# deploy.ps1
# Unified deployment script for HOUT and MARMER devices
# Usage: .\deploy.ps1 hout      - USB upload to HOUT (189), firmware only
#        .\deploy.ps1 hout +    - USB upload with CSV/JS
#        .\deploy.ps1 marmer    - OTA upload to MARMER (188), firmware only
#        .\deploy.ps1 marmer +  - OTA upload with CSV/JS
#        .\deploy.ps1 marmer -SkipBuild  - OTA upload only (use existing firmware)

param(
    [Parameter(Mandatory=$true, Position=0)]
    [ValidateSet("hout", "marmer")]
    [string]$Device,
    
    [Parameter(Position=1)]
    [string]$Plus,
    
    [switch]$SkipBuild
)

# "+" as second param enables SD upload
$SkipSD = $Plus -ne "+"

$HOUT_IP = "192.168.2.189"
$MARMER_IP = "192.168.2.188"
$sdroot = Join-Path $PSScriptRoot "sdroot"

$deviceUpper = $Device.ToUpper()
$targetIP = if ($Device -eq "marmer") { $MARMER_IP } else { $HOUT_IP }
$envName = $Device.ToLower()

Write-Host "========================================" -ForegroundColor Cyan
Write-Host " $deviceUpper Deployment Script" -ForegroundColor Cyan
Write-Host " Target: $targetIP" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# --- Serial port warning for HOUT ---
if ($Device -eq "hout") {
    Write-Host "  Close serial monitor (Ctrl+C in trace terminal)" -ForegroundColor Yellow
    Write-Host ""
    $confirm = Read-Host "Is COM port free? (y/N)"
    if ($confirm -ne 'y' -and $confirm -ne 'Y') {
        Write-Host "Aborted. Free serial port first." -ForegroundColor Red
        exit 1
    }
    Write-Host ""
}

$step = 1
$totalSteps = 3  # SD upload, Build+Upload, Reboot wait
if ($SkipSD) { $totalSteps-- }

# --- Upload SD files FIRST (before firmware, so new firmware has updated files) ---
if (-not $SkipSD) {
    Write-Host "[$step/$totalSteps] Uploading SD files to $deviceUpper ($targetIP)..." -ForegroundColor Yellow
    
    # Build JS first
    Write-Host "  Building kwal.js..." -ForegroundColor Gray
    & "$PSScriptRoot\build_js.ps1" 2>$null
    
    # Upload CSV files
    Write-Host "  Uploading CSV files..." -ForegroundColor Gray
    & "$PSScriptRoot\upload_csv.ps1" -ip $targetIP 2>$null
    
    # Upload web files
    Write-Host "  Uploading web files..." -ForegroundColor Gray
    & "$PSScriptRoot\upload_web.ps1" -ip $targetIP 2>$null
    
    Write-Host "  SD upload complete" -ForegroundColor Green
    $step++
}

# --- OTA mode warning for MARMER (after SD upload, before firmware build+upload) ---
if ($Device -eq "marmer") {
    Write-Host ""
    Write-Host "  MARMER must be in OTA mode for firmware upload!" -ForegroundColor Yellow
    Write-Host "  Click 'start OTA' button in WebGUI" -ForegroundColor Gray
    Write-Host ""
    $confirm = Read-Host "Is MARMER OTA mode started? (y/N)"
    if ($confirm -ne 'y' -and $confirm -ne 'Y') {
        Write-Host "Aborted. Put MARMER in OTA mode first." -ForegroundColor Red
        exit 1
    }
}

# --- Build and Upload firmware (combined - pio run -t upload does both) ---
Write-Host ""
if ($SkipBuild) {
    Write-Host "[$step/$totalSteps] Uploading firmware to $deviceUpper ($targetIP) (skip build)..." -ForegroundColor Yellow
    pio run -e $envName -t nobuild -t upload
} else {
    Write-Host "[$step/$totalSteps] Building and uploading firmware to $deviceUpper ($targetIP)..." -ForegroundColor Yellow
    pio run -e $envName -t upload
}
if ($LASTEXITCODE -ne 0) {
    Write-Host "  Upload failed!" -ForegroundColor Red
    exit $LASTEXITCODE
}
Write-Host "  Upload complete" -ForegroundColor Green
$step++

# --- Wait for reboot ---
Write-Host ""
Write-Host "[$step/$totalSteps] Waiting for $deviceUpper to reboot..." -ForegroundColor Yellow
Start-Sleep -Seconds 8

# Ping check
$pingResult = Test-Connection -ComputerName $targetIP -Count 1 -Quiet
if (-not $pingResult) {
    Write-Host "  WARNING: Device not responding at $targetIP" -ForegroundColor Yellow
    Write-Host "  Waiting additional 10 seconds..." -ForegroundColor Yellow
    Start-Sleep -Seconds 10
}
Write-Host "  Device online" -ForegroundColor Green

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host " $deviceUpper deployment complete!" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Cyan
