<#
.SYNOPSIS
    Scans web route handlers for hard-stop #6 violations:
    "Web handlers: memory only — no SD I/O, no network I/O, no blocking calls."

.DESCRIPTION
    Finds route handler functions in WebInterfaceController/routes/*.cpp,
    then greps for dangerous direct calls that should be deferred to timer callbacks.

.NOTES
    Run from project root: .\tools\check_web_handler_safety.ps1
#>

$routeDir = "lib\WebInterfaceController\routes"
$dangerousPatterns = @(
    # Audio hardware
    'PlayAudioFragment::(stop|start|abortImmediate|updateVolume)'
    'PlaySentence::(stop|start|speak)'
    'audio\.(audioMp3Decoder|audioFile|audioOutput)'
    'audio\.setVolume'
    # Light hardware
    'PlayLightShow\('
    'FastLED\.(show|clear)'
    'applyToLights\(\)'
    # SD I/O (flagged but may be accepted with NOCHECK guard)
    'SD\.(open|remove|rename|mkdir)'
    'file\.(write|read|close)'
)

$combinedPattern = ($dangerousPatterns -join '|')
$violations = 0
$accepted = 0

Write-Host "`n=== Web Handler Safety Check ===" -ForegroundColor Cyan
Write-Host "Scanning $routeDir for hard-stop #6 violations...`n"

$routeFiles = Get-ChildItem -Path $routeDir -Filter "*.cpp" -ErrorAction SilentlyContinue
if (-not $routeFiles) {
    Write-Host "ERROR: No route files found in $routeDir" -ForegroundColor Red
    exit 1
}

foreach ($file in $routeFiles) {
    $lines = Get-Content $file.FullName
    $inFunction = $false
    $funcName = ""
    $braceDepth = 0
    $fileViolations = @()

    for ($i = 0; $i -lt $lines.Count; $i++) {
        $line = $lines[$i]
        $lineNum = $i + 1

        # Detect function start (route handlers and lambda bodies)
        if ($line -match '^\s*void\s+(route\w+)\s*\(') {
            $inFunction = $true
            $funcName = $Matches[1]
            $braceDepth = 0
        }
        elseif ($line -match 'onRequest\s*\(\s*\[') {
            $inFunction = $true
            $funcName = "lambda@L$lineNum"
            $braceDepth = 0
        }

        if ($inFunction) {
            # Track brace depth
            $braceDepth += ($line.ToCharArray() | Where-Object { $_ -eq '{' }).Count
            $braceDepth -= ($line.ToCharArray() | Where-Object { $_ -eq '}' }).Count

            # Check for dangerous calls
            if ($line -match $combinedPattern) {
                $isAccepted = $line -match 'NOCHECK'
                $matchText = $line.Trim()
                if ($isAccepted) {
                    $accepted++
                    $fileViolations += @{
                        Line = $lineNum; Func = $funcName; Code = $matchText; Accepted = $true
                    }
                } else {
                    $violations++
                    $fileViolations += @{
                        Line = $lineNum; Func = $funcName; Code = $matchText; Accepted = $false
                    }
                }
            }

            # End of function
            if ($braceDepth -le 0 -and $braceDepth -ne 0) {
                $inFunction = $false
            }
            if ($braceDepth -eq 0 -and $line -match '^\s*\}') {
                $inFunction = $false
            }
        }
    }

    if ($fileViolations.Count -gt 0) {
        Write-Host "$($file.Name):" -ForegroundColor Yellow
        foreach ($v in $fileViolations) {
            $color = if ($v.Accepted) { "DarkYellow" } else { "Red" }
            $tag = if ($v.Accepted) { "ACCEPTED" } else { "VIOLATION" }
            Write-Host "  L$($v.Line) [$tag] $($v.Func): $($v.Code)" -ForegroundColor $color
        }
        Write-Host ""
    }
}

# Also check RunManager request* functions for direct dangerous calls
Write-Host "--- RunManager request* functions ---" -ForegroundColor Cyan
$rmFile = "lib\RunManager\RunManager.cpp"
if (Test-Path $rmFile) {
    $lines = Get-Content $rmFile
    $inRequest = $false
    $funcName = ""
    $braceDepth = 0
    $dangerousInRequest = @(
        'PlayAudioFragment::(stop|start|abortImmediate|updateVolume)'
        'PlaySentence::(stop|start|speak)'
        'audio\.setVolume'
        'PlayLightShow\('
        'applyToLights\(\)'
    )
    $reqPattern = ($dangerousInRequest -join '|')

    for ($i = 0; $i -lt $lines.Count; $i++) {
        $line = $lines[$i]
        $lineNum = $i + 1

        # Only match RunManager::request* function definitions
        if ($line -match '^\s*(void|bool)\s+(RunManager::request\w+)\s*\(') {
            $inRequest = $true
            $funcName = $Matches[2]
            $braceDepth = 0
        }
        # Any other function definition ends tracking
        elseif ($inRequest -and $line -match '^\s*(void|bool|float|int|uint)\s+\w+(::\w+)?\s*\(' -and $line -notmatch 'RunManager::request') {
            $inRequest = $false
        }

        if ($inRequest) {
            $braceDepth += ($line.ToCharArray() | Where-Object { $_ -eq '{' }).Count
            $braceDepth -= ($line.ToCharArray() | Where-Object { $_ -eq '}' }).Count

            if ($line -match $reqPattern) {
                $violations++
                $trimmed = $line.Trim()
                Write-Host "  L$lineNum [VIOLATION] ${funcName}: $trimmed" -ForegroundColor Red
            }

            if ($braceDepth -eq 0 -and $line -match '^\s*\}') {
                $inRequest = $false
            }
        }
    }
}

Write-Host "`n=== Summary ===" -ForegroundColor Cyan
if ($violations -eq 0) {
    Write-Host "OK: No violations found." -ForegroundColor Green
} else {
    Write-Host "FAIL: $violations violation(s) found." -ForegroundColor Red
}
if ($accepted -gt 0) {
    Write-Host "INFO: $accepted accepted exception(s) (NOCHECK)." -ForegroundColor DarkYellow
}
Write-Host ""
