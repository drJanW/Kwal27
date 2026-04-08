<#
.SYNOPSIS
    Quick USB upload of existing firmware build.
.DESCRIPTION
    Uploads the current build to HOUT via USB without recompiling.
.PARAMETER ComPort
    COM port number (e.g. 12 for COM12). If omitted, PlatformIO auto-detects.
#>
param(
    [string]$ComPort
)
$ErrorActionPreference = "Stop"

Push-Location -Path $PSScriptRoot
try {
    $uploadArgs = @("run", "-e", "esp32", "-t", "nobuild", "-t", "upload")
    if ($ComPort) {
        $uploadArgs += "--upload-port"
        $uploadArgs += "COM$ComPort"
        Write-Host "Uploading existing build via USB on COM$ComPort..." -ForegroundColor Cyan
    } else {
        Write-Host "Uploading existing build via USB (auto-detect)..." -ForegroundColor Cyan
    }
    platformio @uploadArgs
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
}
finally {
    Pop-Location
}

