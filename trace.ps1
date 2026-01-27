param(
    [string]$ComPort = "6"
)

$ErrorActionPreference = "Stop"
$pio = Join-Path $env:USERPROFILE ".platformio\penv\Scripts\platformio.exe"
if (-not (Test-Path -LiteralPath $pio)) {
    throw "PlatformIO executable not found: $pio"
}

$portArg = "COM$ComPort"
& $pio device monitor --port $portArg --baud 115200 --filter default
