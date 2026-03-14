<#
.SYNOPSIS
    Upload a single file from sdroot/ to ESP32 SD card.
.EXAMPLE
    .\upload_file.ps1 audioShifts.csv 192.168.2.189
#>
# Upload a single file from sdroot/ to ESP32 SD card
# Usage: .\upload_file.ps1 <filename> <IP>
# Example: .\upload_file.ps1 audioShifts.csv 192.168.2.189

param(
    [Parameter(Mandatory=$true)][string]$filename,
    [Parameter(Mandatory=$true)][string]$ip
)

$sdroot = Join-Path $PSScriptRoot "sdroot"
$filePath = Join-Path $sdroot $filename

if (-not (Test-Path $filePath)) {
    Write-Host "Not found: $filePath" -ForegroundColor Red
    exit 1
}

$url = "http://$ip/api/sd/upload"
Write-Host "Uploading $filename to $ip..." -NoNewline

curl -s -X POST -F "file=@$filePath" -F "path=/" $url | Out-Null

if ($LASTEXITCODE -eq 0) {
    Write-Host " OK" -ForegroundColor Green
} else {
    Write-Host " FAILED (exit $LASTEXITCODE)" -ForegroundColor Red
    exit 1
}
