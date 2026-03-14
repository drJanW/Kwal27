<#
.SYNOPSIS
    Serial monitor for ESP32 debug output.
.DESCRIPTION
    Opens PlatformIO serial monitor at 115200 baud.
.EXAMPLE
    .\trace.ps1
    .\trace.ps1 12
#>
param(
    [string]$ComPort = ""
)

$ErrorActionPreference = "Stop"
$pio = Join-Path $env:USERPROFILE ".platformio\penv\Scripts\platformio.exe"
if (-not (Test-Path -LiteralPath $pio)) {
    throw "PlatformIO executable not found: $pio"
}

$args = @("device", "monitor", "--baud", "115200", "--filter", "default")
if ($ComPort -ne "") {
    $args += @("--port", "COM$ComPort")
}

& $pio @args
