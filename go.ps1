<#
.SYNOPSIS
    Quick USB upload of existing firmware build.
.DESCRIPTION
    Uploads the current build to HOUT via USB without recompiling.
#>
$ErrorActionPreference = "Stop"

Push-Location -Path $PSScriptRoot
try {
    Write-Host "Uploading existing build via USB..." -ForegroundColor Cyan
    platformio run -e esp32 -t nobuild -t upload
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
}
finally {
    Pop-Location
}

