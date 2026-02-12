$ErrorActionPreference = "Stop"

Push-Location -Path $PSScriptRoot
try {
    Write-Host "Uploading existing build for hout..." -ForegroundColor Cyan
    platformio run -e hout -t nobuild -t upload
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
}
finally {
    Pop-Location
}

