<#
build_js.ps1

Current WebGUI source-of-truth lives in sdroot/webgui-src/js/*.js.
Run from project root:
  .\build_js.ps1

This delegates to:
  sdroot\webgui-src\build.ps1
#>

$projectRoot = $PSScriptRoot
if (-not $projectRoot) {
    $projectRoot = (Get-Location).Path
}

$webguiSrcDir = Join-Path $projectRoot "sdroot\webgui-src"
$scriptPath = Join-Path $webguiSrcDir "build.ps1"
if (-not (Test-Path $scriptPath)) {
    Write-Host "ERROR: WebGUI build script not found: $scriptPath" -ForegroundColor Red
    exit 1
}

Write-Host "Building WebGUI (delegating to sdroot\\webgui-src\\build.ps1)..." -ForegroundColor Cyan
Push-Location $webguiSrcDir
try {
  & .\build.ps1
  exit 0
}
finally {
  Pop-Location
}