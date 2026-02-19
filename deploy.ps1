# deploy.ps1
# Unified deployment script — one firmware binary, deploy via USB or OTA
# Usage: .\deploy.ps1 usb                 - USB upload to HOUT (192.168.2.189)
#        .\deploy.ps1 usb +               - USB + SD files
#        .\deploy.ps1 192.168.2.188        - OTA upload to that IP
#        .\deploy.ps1 192.168.2.188 +      - OTA + SD files
#        .\deploy.ps1 192.168.2.188 -SkipBuild  - OTA with existing firmware.bin

param(
    [Parameter(Position=0)]
    [string]$Target,
    
    [Parameter(Position=1)]
    [string]$Arg1,
    
    [switch]$SkipBuild
)

if (-not $Target) {
    Write-Host ""
    Write-Host "  deploy.ps1 — Kwal27 deployment" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "  Usage:" -ForegroundColor White
    Write-Host "    .\deploy.ps1 usb                 Firmware via USB (HOUT)" -ForegroundColor Gray
    Write-Host "    .\deploy.ps1 usb +               Firmware + SD files via USB" -ForegroundColor Gray
    Write-Host "    .\deploy.ps1 192.168.2.188        Firmware via OTA" -ForegroundColor Gray
    Write-Host "    .\deploy.ps1 192.168.2.188 +      Firmware + SD files via OTA" -ForegroundColor Gray
    Write-Host "    .\deploy.ps1 192.168.2.188 -SkipBuild   OTA zonder rebuild" -ForegroundColor Gray
    Write-Host ""
    Write-Host "  Devices:" -ForegroundColor White
    Write-Host "    HOUT   = 192.168.2.189 (USB via COM12)" -ForegroundColor Gray
    Write-Host "    MARMER = 192.168.2.188 (OTA only)" -ForegroundColor Gray
    Write-Host ""
    exit 0
}

# Determine method and IP from target
if ($Target -eq "usb") {
    $Method = "USB"
    $targetIP = "192.168.2.189"
    $SkipSD = $Arg1 -ne "+"
} elseif ($Target -match '^\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}$') {
    $Method = "OTA"
    $targetIP = $Target
    $SkipSD = $Arg1 -ne "+"
} else {
    Write-Host "Invalid target: $Target" -ForegroundColor Red
    Write-Host "Use 'usb' or a full IP address (e.g. 192.168.2.188)" -ForegroundColor Yellow
    exit 1
}

# Derive device name from IP
$deviceName = switch ($targetIP) {
    "192.168.2.189" { "HOUT" }
    "192.168.2.188" { "MARMER" }
    default         { $targetIP }
}

$sdroot = Join-Path $PSScriptRoot "sdroot"
$envName = "esp32"

# --- NAS CSV server settings ---
$NAS_CSV_URL = "http://192.168.2.23:8081"
$NAS_CSV_FILES = @("light_patterns.csv", "light_colors.csv")

Write-Host "========================================" -ForegroundColor Cyan
Write-Host " $deviceName Deployment ($Method)" -ForegroundColor Cyan
Write-Host " Target: $targetIP" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# --- Sync updated CSVs from NAS to sdroot ---
Write-Host "Checking NAS for updated CSVs..." -ForegroundColor Gray
foreach ($csvFile in $NAS_CSV_FILES) {
    try {
        $response = Invoke-WebRequest -Uri "$NAS_CSV_URL/csv/$csvFile" -TimeoutSec 3 -ErrorAction Stop
        if ($response.StatusCode -eq 200 -and $response.Content.Length -gt 0) {
            $localPath = Join-Path $sdroot $csvFile
            $nasContent = $response.Content
            $localContent = ""
            if (Test-Path $localPath) {
                $localContent = Get-Content -Path $localPath -Raw
            }
            if ($nasContent -ne $localContent) {
                Set-Content -Path $localPath -Value $nasContent -NoNewline
                Write-Host "  $csvFile → sdroot (updated from NAS)" -ForegroundColor Green
                Invoke-WebRequest -Uri "$NAS_CSV_URL/api/move?file=$csvFile" -TimeoutSec 3 -ErrorAction SilentlyContinue | Out-Null
            } else {
                Write-Host "  $csvFile → already current (cleaning NAS)" -ForegroundColor DarkGray
                Invoke-WebRequest -Uri "$NAS_CSV_URL/api/move?file=$csvFile" -TimeoutSec 3 -ErrorAction SilentlyContinue | Out-Null
            }
        }
    }
    catch {
        if ($_.Exception.Response.StatusCode.value__ -eq 404) {
            Write-Host "  $csvFile → not on NAS (up to date)" -ForegroundColor DarkGray
        }
    }
}
Write-Host ""

# --- Auto-close serial monitor for USB ---
if ($Method -eq "USB") {
    $monitors = Get-Process -Name "platformio" -ErrorAction SilentlyContinue
    if ($monitors) {
        Write-Host "  Closing serial monitor..." -ForegroundColor Yellow
        $monitors | Stop-Process -Force
        Start-Sleep -Seconds 1
    }
}

$step = 1
$totalSteps = 3
if ($SkipSD) { $totalSteps-- }

# --- Upload SD files FIRST ---
if (-not $SkipSD) {
    Write-Host "[$step/$totalSteps] Uploading SD files to $deviceName ($targetIP)..." -ForegroundColor Yellow
    
    Write-Host "  Building kwal.js..." -ForegroundColor Gray
    & "$PSScriptRoot\build_js.ps1"
    if ($LASTEXITCODE -ne 0) { Write-Host "  WARNING: JS build failed!" -ForegroundColor Red }
    
    Write-Host "  Uploading CSV files..." -ForegroundColor Gray
    & "$PSScriptRoot\upload_csv.ps1" -ip $targetIP
    if ($LASTEXITCODE -ne 0) { Write-Host "  WARNING: CSV upload failed!" -ForegroundColor Red }
    
    Write-Host "  Uploading web files..." -ForegroundColor Gray
    & "$PSScriptRoot\upload_web.ps1" -ip $targetIP
    if ($LASTEXITCODE -ne 0) { Write-Host "  WARNING: Web upload failed!" -ForegroundColor Red }
    
    Write-Host "  SD upload complete" -ForegroundColor Green
    $step++
}

# --- Build and Upload firmware ---
Write-Host ""
if ($Method -eq "OTA") {
    if (-not $SkipBuild) {
        Write-Host "[$step/$totalSteps] Building firmware for $deviceName..." -ForegroundColor Yellow
        pio run -e $envName
        if ($LASTEXITCODE -ne 0) {
            Write-Host "  Build failed!" -ForegroundColor Red
            exit $LASTEXITCODE
        }
        Write-Host "  Build complete" -ForegroundColor Green
    }
    $step++

    $binPath = Join-Path $PSScriptRoot ".pio\build\esp32\firmware.bin"
    if (-not (Test-Path $binPath)) {
        Write-Host "  firmware.bin not found at $binPath" -ForegroundColor Red
        exit 1
    }
    $binSize = (Get-Item $binPath).Length
    Write-Host "[$step/$totalSteps] Uploading firmware via HTTP OTA ($([math]::Round($binSize/1024)) KB)..." -ForegroundColor Yellow
    
    $uploadUrl = "http://$targetIP/ota/upload"
    $result = curl -X POST -F "firmware=@$binPath" $uploadUrl --silent --show-error --max-time 120 2>&1
    
    if ($result -match '"status"\s*:\s*"ok"') {
        Write-Host "  HTTP OTA upload OK - device rebooting" -ForegroundColor Green
    } else {
        Write-Host "  HTTP OTA upload failed: $result" -ForegroundColor Red
        exit 1
    }
} else {
    if ($SkipBuild) {
        Write-Host "[$step/$totalSteps] Uploading firmware to $deviceName (skip build)..." -ForegroundColor Yellow
        pio run -e $envName -t nobuild -t upload
    } else {
        Write-Host "[$step/$totalSteps] Building and uploading firmware to $deviceName..." -ForegroundColor Yellow
        pio run -e $envName -t upload
    }
    if ($LASTEXITCODE -ne 0) {
        Write-Host "  Upload failed!" -ForegroundColor Red
        exit $LASTEXITCODE
    }
    Write-Host "  Upload complete" -ForegroundColor Green
}
$step++

# --- Wait for reboot ---
Write-Host ""
Write-Host "[$step/$totalSteps] Waiting for $deviceName to reboot..." -ForegroundColor Yellow
Start-Sleep -Seconds 8

$pingResult = Test-Connection -ComputerName $targetIP -Count 1 -Quiet
if (-not $pingResult) {
    Write-Host "  WARNING: Device not responding at $targetIP" -ForegroundColor Yellow
    Write-Host "  Waiting additional 10 seconds..." -ForegroundColor Yellow
    Start-Sleep -Seconds 10
}
Write-Host "  Device online" -ForegroundColor Green

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host " $deviceName deployment complete!" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Cyan

# --- Auto-start serial monitor for USB ---
if ($Method -eq "USB") {
    Write-Host ""
    Write-Host "Starting serial monitor..." -ForegroundColor Yellow
    & "$PSScriptRoot\trace.ps1"
}
