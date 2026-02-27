<#
.SYNOPSIS
    Checks for architecture violations in the Kwal27 codebase.

.DESCRIPTION
    Enforces layer boundaries defined in copilot-instructions.md:
    - Controllers must NOT include/reference other Controllers
    - cb_* functions must only exist in lib/RunManager/
    - Policy files must NOT include hardware/controller headers
    - Forward declarations of other subsystem namespaces are flagged

.NOTES
    Run from project root: .\tools\check_architecture.ps1
#>

$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent
$libDir = Join-Path $root "lib"

# ── Layer definitions ──────────────────────────────────────────

# Controller libraries — each is an isolated hardware subsystem
$controllerLibs = @(
    "AudioManager"
    "ClockController"
    "LightController"
    "SDController"
    "SensorController"
    "WiFiController"
)

# Shared libraries — any layer may include these
$sharedLibs = @(
    "Globals"
    "TimerManager"
    "ContextController"
)

# Headers from each controller lib (basename only, e.g. "PlaySentence.h")
$controllerHeaders = @{}
foreach ($lib in $controllerLibs) {
    $dir = Join-Path $libDir $lib
    if (Test-Path $dir) {
        $controllerHeaders[$lib] = @(
            Get-ChildItem -Path $dir -Filter "*.h" -File |
            ForEach-Object { $_.Name }
        )
    }
}

# Headers that Policy files must NOT include (hardware/driver headers)
$hardwareHeaders = @(
    "FastLED.h", "SD.h", "SPI.h", "Wire.h", "I2S.h",
    "AudioGeneratorMP3.h", "AudioFileSourceSD.h", "AudioFileSourceHTTPStream.h",
    "AudioOutputI2S.h", "HTTPClient.h"
)

$violations = @()
$warnings = @()

# ── Check 1: Cross-controller includes ────────────────────────
Write-Host "`n=== Check 1: Cross-controller includes ===" -ForegroundColor Cyan

foreach ($lib in $controllerLibs) {
    $dir = Join-Path $libDir $lib
    if (!(Test-Path $dir)) { continue }

    $files = Get-ChildItem -Path $dir -Include "*.cpp","*.h" -Recurse -File
    foreach ($file in $files) {
        $content = Get-Content $file.FullName -Raw
        $lineNum = 0
        foreach ($line in (Get-Content $file.FullName)) {
            $lineNum++
            # Match #include "SomeHeader.h" (not angle-bracket system includes)
            if ($line -match '^\s*#include\s+"([^"]+)"') {
                $included = $Matches[1]
                $baseName = Split-Path $included -Leaf

                # Check if this header belongs to a DIFFERENT controller lib
                foreach ($otherLib in $controllerLibs) {
                    if ($otherLib -eq $lib) { continue }
                    if ($controllerHeaders[$otherLib] -contains $baseName) {
                        $rel = $file.FullName.Substring($root.Length + 1)
                        $violations += "[CROSS-CTRL] $rel`:$lineNum includes $baseName (from $otherLib)"
                    }
                }
            }

            # Check for forward declarations of other controller namespaces
            foreach ($otherLib in $controllerLibs) {
                if ($otherLib -eq $lib) { continue }
                # Match "namespace PlayAudioFragment {" or "PlayAudioFragment::" etc.
                foreach ($header in $controllerHeaders[$otherLib]) {
                    $ns = $header -replace '\.h$',''
                    if ($line -match "namespace\s+$ns\s*\{" -or
                        ($line -match "${ns}::" -and $line -notmatch '^\s*//')) {
                        $rel = $file.FullName.Substring($root.Length + 1)
                        $violations += "[CROSS-CTRL] $rel`:$lineNum references $ns namespace (from $otherLib)"
                    }
                }
            }
        }
    }
}

# ── Check 2: cb_* outside RunManager ──────────────────────────
Write-Host "=== Check 2: cb_* functions outside RunManager ===" -ForegroundColor Cyan

$allCppFiles = Get-ChildItem -Path $libDir -Include "*.cpp" -Recurse -File
foreach ($file in $allCppFiles) {
    # Skip RunManager — that's where cb_* belongs
    if ($file.FullName -like "*\RunManager\*") { continue }

    $lineNum = 0
    foreach ($line in (Get-Content $file.FullName)) {
        $lineNum++
        # Match function definitions: "void cb_something() {"
        # But skip forward declarations and comments
        if ($line -match '^\s*void\s+(cb_\w+)\s*\(' -and $line -notmatch '^\s*//') {
            $cbName = $Matches[1]
            $rel = $file.FullName.Substring($root.Length + 1)
            # Allow cb_ in anonymous namespaces within controllers (internal callbacks for timers)
            # But flag them as warnings, not violations
            $warnings += "[CB-OUTSIDE] $rel`:$lineNum defines $cbName (cb_* should be in RunManager)"
        }
    }
}

# ── Check 3: Policy files with hardware includes ─────────────
Write-Host "=== Check 3: Policy files with hardware includes ===" -ForegroundColor Cyan

$policyFiles = Get-ChildItem -Path $libDir -Include "*Policy.cpp","*Policy.h" -Recurse -File
foreach ($file in $policyFiles) {
    $lineNum = 0
    foreach ($line in (Get-Content $file.FullName)) {
        $lineNum++
        if ($line -match '^\s*#include\s+[<"]([^">]+)[">]') {
            $included = $Matches[1]
            $baseName = Split-Path $included -Leaf
            if ($hardwareHeaders -contains $baseName) {
                $rel = $file.FullName.Substring($root.Length + 1)
                $violations += "[POLICY-HW] $rel`:$lineNum includes hardware header $baseName"
            }
        }
    }
}

# ── Check 4: WebInterface handlers with SD/blocking I/O ──────
Write-Host "=== Check 4: Web handlers with blocking calls ===" -ForegroundColor Cyan

$webDir = Join-Path $libDir "WebInterfaceController"
if (Test-Path $webDir) {
    $webFiles = Get-ChildItem -Path $webDir -Include "*.cpp" -Recurse -File
    foreach ($file in $webFiles) {
        $lineNum = 0
        foreach ($line in (Get-Content $file.FullName)) {
            $lineNum++
            # Flag direct SD writes in web handler files
            if ($line -match '\bSD\.(open|write|remove|rename|mkdir)\b' -and $line -notmatch '^\s*//') {
                $rel = $file.FullName.Substring($root.Length + 1)
                $warnings += "[WEB-IO] $rel`:$lineNum direct SD I/O in web handler"
            }
            # Flag delay() calls
            if ($line -match '\bdelay\s*\(' -and $line -notmatch '^\s*//') {
                $rel = $file.FullName.Substring($root.Length + 1)
                $violations += "[WEB-BLOCK] $rel`:$lineNum delay() in web handler"
            }
        }
    }
}

# ── Results ───────────────────────────────────────────────────
Write-Host "`n=== Results ===" -ForegroundColor Yellow

if ($violations.Count -eq 0 -and $warnings.Count -eq 0) {
    Write-Host "`n  ✓ No architecture violations found" -ForegroundColor Green
    exit 0
}

if ($violations.Count -gt 0) {
    Write-Host "`n  VIOLATIONS ($($violations.Count)):" -ForegroundColor Red
    foreach ($v in $violations) {
        Write-Host "    $v" -ForegroundColor Red
    }
}

if ($warnings.Count -gt 0) {
    Write-Host "`n  WARNINGS ($($warnings.Count)):" -ForegroundColor DarkYellow
    foreach ($w in $warnings) {
        Write-Host "    $w" -ForegroundColor DarkYellow
    }
}

Write-Host ""
if ($violations.Count -gt 0) {
    Write-Host "  ✗ $($violations.Count) violation(s), $($warnings.Count) warning(s)" -ForegroundColor Red
    exit 1
} else {
    Write-Host "  ~ $($warnings.Count) warning(s), 0 violations" -ForegroundColor DarkYellow
    exit 0
}
