$ErrorActionPreference = "Stop"

Push-Location -Path $PSScriptRoot
try {
    Write-Host "Uploading existing build for esp32_v3..." -ForegroundColor Cyan
    platformio run -e esp32_v3 -t nobuild -t upload
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
}
finally {
    Pop-Location
}

