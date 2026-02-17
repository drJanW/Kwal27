# sync_mp3.ps1
# Syncs MP3 directories from NAS to ESP32 SD card (vote-preserving)
# Usage: .\sync_mp3.ps1              # sync to HOUT (189)
#        .\sync_mp3.ps1 188          # sync to MARMER (188)
#        .\sync_mp3.ps1 -ip 192.168.2.188

param(
    [Parameter(Position=0)][string]$lastOctet = "189",
    [string]$ip = ""
)

if ($ip -eq "") {
    $ip = "192.168.2.$lastOctet"
}

$nasRoot = "V:\kwal\sdcard"
$baseUrl = "http://$ip"
$timeout = 10  # seconds per HTTP request

# ── Helpers ──────────────────────────────────────────────────────

function Test-DeviceReachable {
    try {
        $r = Invoke-WebRequest -Uri "$baseUrl/api/health" -TimeoutSec 3 -ErrorAction Stop
        $json = $r.Content | ConvertFrom-Json
        Write-Host "Device: $($json.device) — firmware $($json.firmware)" -ForegroundColor Cyan
        return $true
    } catch {
        Write-Host "ERROR: Cannot reach $ip" -ForegroundColor Red
        return $false
    }
}

function Get-EspFileList([int]$dirNum) {
    $dirStr = "{0:d3}" -f $dirNum
    try {
        $r = Invoke-WebRequest -Uri "$baseUrl/api/sd/list?path=/$dirStr" -TimeoutSec $timeout -ErrorAction Stop
        $json = $r.Content | ConvertFrom-Json
        $result = @{}
        foreach ($f in $json.files) {
            # Skip index files
            if ($f.name -match '^\.' ) { continue }
            $result[$f.name] = [long]$f.size
        }
        return $result
    } catch {
        return @{}
    }
}

function Get-EspHighestDir {
    try {
        $r = Invoke-WebRequest -Uri "$baseUrl/api/audio/grid" -TimeoutSec $timeout -ErrorAction Stop
        $json = $r.Content | ConvertFrom-Json
        return [int]$json.highest
    } catch {
        Write-Host "WARNING: Cannot read grid, using NAS max only" -ForegroundColor Yellow
        return 0
    }
}

function Send-Delete([string]$sdPath) {
    try {
        $encoded = [Uri]::EscapeDataString($sdPath)
        $r = Invoke-WebRequest -Uri "$baseUrl/api/sd/delete?path=$encoded" -Method POST -TimeoutSec $timeout -ErrorAction Stop
        return $true
    } catch {
        Write-Host "  FAIL delete $sdPath : $_" -ForegroundColor Red
        return $false
    }
}

function Send-Upload([string]$localPath, [string]$sdDir) {
    try {
        # Use curl for multipart upload (Invoke-WebRequest multipart is verbose)
        $result = curl -s -X POST -F "file=@$localPath" "$baseUrl/api/sd/upload?path=$sdDir" 2>&1
        $json = $result | ConvertFrom-Json -ErrorAction Stop
        return ($json.status -eq "ok")
    } catch {
        Write-Host "  FAIL upload $localPath : $_" -ForegroundColor Red
        return $false
    }
}

function Send-SyncDir([int]$dirNum) {
    try {
        $r = Invoke-WebRequest -Uri "$baseUrl/api/sd/syncdir?dir=$dirNum" -Method POST -TimeoutSec $timeout -ErrorAction Stop
        return $true
    } catch {
        Write-Host "  FAIL syncdir $dirNum : $_" -ForegroundColor Red
        return $false
    }
}

# ── Pre-flight ───────────────────────────────────────────────────

if (-not (Test-Path $nasRoot)) {
    Write-Host "ERROR: NAS not mounted at $nasRoot" -ForegroundColor Red
    exit 1
}

if (-not (Test-DeviceReachable)) { exit 1 }

# ── Phase 1: Scan NAS ───────────────────────────────────────────

Write-Host "`nScanning NAS..." -ForegroundColor Cyan
$nasDirs = @{}  # dirNum → @{ filename → size }

$subDirs = Get-ChildItem -Path $nasRoot -Directory | Where-Object { $_.Name -match '^\d{3}$' }
foreach ($d in $subDirs) {
    $dirNum = [int]$d.Name
    if ($dirNum -eq 0) { continue }  # skip 000 (words/speak)
    $files = @{}
    Get-ChildItem -Path $d.FullName -Filter "*.mp3" | ForEach-Object {
        $files[$_.Name] = $_.Length
    }
    if ($files.Count -gt 0) {
        $nasDirs[$dirNum] = $files
    }
}

$nasCount = ($nasDirs.Values | ForEach-Object { $_.Count } | Measure-Object -Sum).Sum
Write-Host "  NAS: $($nasDirs.Count) dirs, $nasCount files" -ForegroundColor White

# ── Phase 2: Scan ESP32 ─────────────────────────────────────────

Write-Host "Scanning ESP32 at $ip..." -ForegroundColor Cyan
$espDirs = @{}  # dirNum → @{ filename → size }

# Scan all NAS dirs on ESP
$allDirNums = [System.Collections.Generic.HashSet[int]]::new()
foreach ($k in $nasDirs.Keys) { [void]$allDirNums.Add($k) }

# Also check ESP dirs beyond NAS range (detect orphans)
$espHighest = Get-EspHighestDir
for ($i = 1; $i -le $espHighest; $i++) {
    [void]$allDirNums.Add($i)
}

$sorted = $allDirNums | Sort-Object
$progress = 0
foreach ($dirNum in $sorted) {
    $progress++
    $pct = [math]::Round(($progress / $sorted.Count) * 100)
    Write-Host "`r  Scanning dir $("{0:d3}" -f $dirNum) ($pct%)..." -NoNewline
    $files = Get-EspFileList $dirNum
    if ($files.Count -gt 0) {
        $espDirs[$dirNum] = $files
    }
}
Write-Host ""  # newline after progress

$espCount = ($espDirs.Values | ForEach-Object { $_.Count } | Measure-Object -Sum).Sum
Write-Host "  ESP: $($espDirs.Count) dirs, $espCount files" -ForegroundColor White

# ── Phase 3: Compare ────────────────────────────────────────────

$toUpload = @()   # @{ dirNum, filename, nasPath }
$toDelete = @()   # sdPath strings
$changedDirs = [System.Collections.Generic.HashSet[int]]::new()

foreach ($dirNum in $sorted) {
    $nasFiles = if ($nasDirs.ContainsKey($dirNum)) { $nasDirs[$dirNum] } else { @{} }
    $espFiles = if ($espDirs.ContainsKey($dirNum)) { $espDirs[$dirNum] } else { @{} }
    $dirStr = "{0:d3}" -f $dirNum

    # Files on NAS but not on ESP (or different size) → upload
    foreach ($fname in $nasFiles.Keys) {
        $nasSize = $nasFiles[$fname]
        $espSize = if ($espFiles.ContainsKey($fname)) { $espFiles[$fname] } else { -1 }
        if ($espSize -ne $nasSize) {
            $toUpload += @{
                dirNum  = $dirNum
                filename = $fname
                nasPath = Join-Path $nasRoot "$dirStr\$fname"
            }
            [void]$changedDirs.Add($dirNum)
        }
    }

    # Files on ESP but not on NAS → delete
    foreach ($fname in $espFiles.Keys) {
        if (-not $nasFiles.ContainsKey($fname)) {
            $toDelete += "/$dirStr/$fname"
            [void]$changedDirs.Add($dirNum)
        }
    }
}

# ── Phase 4: Summary & Confirm ──────────────────────────────────

Write-Host ""
if ($toUpload.Count -eq 0 -and $toDelete.Count -eq 0) {
    Write-Host "Already in sync." -ForegroundColor Green
    exit 0
}

$uploadSize = ($toUpload | ForEach-Object {
    (Get-Item $_.nasPath).Length
} | Measure-Object -Sum).Sum
$uploadMB = [math]::Round($uploadSize / 1MB, 1)

Write-Host "Changes:" -ForegroundColor Yellow
Write-Host "  Upload: $($toUpload.Count) files ($uploadMB MB)" -ForegroundColor White
Write-Host "  Delete: $($toDelete.Count) files" -ForegroundColor White
Write-Host "  Reindex: $($changedDirs.Count) dirs" -ForegroundColor White

# Show details for small change sets
if ($toUpload.Count -le 20) {
    foreach ($u in $toUpload) {
        Write-Host "    + /$("{0:d3}" -f $u.dirNum)/$($u.filename)" -ForegroundColor Green
    }
}
if ($toDelete.Count -le 20) {
    foreach ($d in $toDelete) {
        Write-Host "    - $d" -ForegroundColor Red
    }
}

Write-Host ""
$confirm = Read-Host "Continue? [Y/n]"
if ($confirm -eq "n" -or $confirm -eq "N") {
    Write-Host "Aborted." -ForegroundColor Yellow
    exit 0
}

# ── Phase 5: Execute deletions ───────────────────────────────────

$deleted = 0
if ($toDelete.Count -gt 0) {
    Write-Host "`nDeleting $($toDelete.Count) files..." -ForegroundColor Cyan
    foreach ($path in $toDelete) {
        Write-Host "  DEL $path" -NoNewline
        if (Send-Delete $path) {
            Write-Host " OK" -ForegroundColor Green
            $deleted++
        }
    }
}

# ── Phase 6: Execute uploads ────────────────────────────────────

$uploaded = 0
if ($toUpload.Count -gt 0) {
    Write-Host "`nUploading $($toUpload.Count) files..." -ForegroundColor Cyan
    $i = 0
    foreach ($u in $toUpload) {
        $i++
        $dirStr = "/{0:d3}" -f $u.dirNum
        $pct = [math]::Round(($i / $toUpload.Count) * 100)
        Write-Host "  [$pct%] $dirStr/$($u.filename)" -NoNewline
        if (Send-Upload $u.nasPath $dirStr) {
            Write-Host " OK" -ForegroundColor Green
            $uploaded++
        }
    }
}

# ── Phase 7: Reindex changed dirs ───────────────────────────────

if ($changedDirs.Count -gt 0) {
    Write-Host "`nReindexing $($changedDirs.Count) dirs..." -ForegroundColor Cyan
    foreach ($dirNum in ($changedDirs | Sort-Object)) {
        Write-Host "  SYNC $("{0:d3}" -f $dirNum)" -NoNewline
        if (Send-SyncDir $dirNum) {
            Write-Host " OK" -ForegroundColor Green
        }
        # Small delay to let timer callback complete before next
        Start-Sleep -Milliseconds 2000
    }
}

# ── Phase 8: Summary ────────────────────────────────────────────

Write-Host "`nDone: $uploaded uploaded, $deleted deleted, $($changedDirs.Count) dirs reindexed." -ForegroundColor Cyan
