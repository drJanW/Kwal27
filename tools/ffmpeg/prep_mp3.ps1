<#
.SYNOPSIS
  prep.ps1 — Normalize, split, and stream MP3s to SD card dirs

.DESCRIPTION
  Normalizes source MP3s (mono, 44100, 128k, loudnorm, no metadata).
  Files <15s: skipped. Files <105s: normalized as-is. Files >=105s: split into 42s chunks.

  Output goes to D:\sounds\prepped\ numbered 001-099.
  When 99 files are reached, they are moved to V:\Kwal\SDcard\<dir>\ and
  numbering restarts at 001 for the next directory.

.PARAMETER Source
  Directory with source MP3 files (default: D:\sounds\new)

.PARAMETER Seg
  Segment length in seconds for splitting (default: 42)

.PARAMETER Threshold
  Duration threshold — files shorter than this are not split (default: 105)

.PARAMETER MinDur
  Minimum duration — files shorter than this are skipped (default: 15)

.PARAMETER SdDir
  First SD card directory number (required)

.PARAMETER SdRoot
  SD card base path (default: V:\Kwal\SDcard)

.PARAMETER MaxPerDir
  Maximum files per SD card directory (default: 99)

.EXAMPLE
  .\prep.ps1 -SdDir 127
  .\prep.ps1 -Source "D:\other" -Seg 30 -SdDir 200
#>
param(
    [string]$Source    = "D:\sounds\new",
    [int]   $Seg       = 42,
    [int]   $Threshold = 105,
    [int]   $MinDur    = 15,
    [Parameter(Mandatory=$true)]
    [int]   $SdDir,
    [string]$SdRoot    = "V:\Kwal\SDcard",
    [int]   $MaxPerDir = 99
)

$ErrorActionPreference = "Stop"

# --- Tools ---
$ffDir   = "D:\Programs\FFmpeg\bin"
$ffmpeg  = Join-Path $ffDir "ffmpeg.exe"
$ffprobe = Join-Path $ffDir "ffprobe.exe"

if (!(Test-Path $ffmpeg))  { Write-Host "[ERR] ffmpeg not found: $ffmpeg"  -ForegroundColor Red; exit 1 }
if (!(Test-Path $ffprobe)) { Write-Host "[ERR] ffprobe not found: $ffprobe" -ForegroundColor Red; exit 1 }
if (!(Test-Path $Source))  { Write-Host "[ERR] Source not found: $Source"   -ForegroundColor Red; exit 1 }
if (!(Test-Path $SdRoot))  { Write-Host "[ERR] SD root not found: $SdRoot" -ForegroundColor Red; exit 1 }

$outDir = "D:\sounds\prepped"
if (!(Test-Path $outDir)) { New-Item -ItemType Directory -Path $outDir -Force | Out-Null }

# --- Gather source files ---
$files = Get-ChildItem -Path $Source -Filter "*.mp3" -File | Sort-Object Name
if ($files.Count -eq 0) {
    Write-Host "[WARN] No MP3 files in $Source" -ForegroundColor Yellow
    exit 0
}

Write-Host ""
Write-Host "[INFO] Source   : $Source ($($files.Count) files)" -ForegroundColor Cyan
Write-Host "[INFO] Staging  : $outDir" -ForegroundColor Cyan
Write-Host "[INFO] Target   : $SdRoot\$SdDir+" -ForegroundColor Cyan
Write-Host "[INFO] Segment  : ${Seg}s (split >= ${Threshold}s, skip < ${MinDur}s)" -ForegroundColor Cyan
Write-Host "[INFO] Max/dir  : $MaxPerDir" -ForegroundColor Cyan
Write-Host ""

# --- Common ffmpeg args ---
$ffArgs = @(
    "-loglevel", "error", "-y",
    "-map", "0:a:0", "-vn", "-sn", "-dn",
    "-af", "loudnorm=I=-16:TP=-1.5:LRA=11",
    "-ac", "1", "-ar", "44100",
    "-c:a", "libmp3lame", "-b:a", "128k",
    "-write_xing", "0", "-map_metadata", "-1", "-id3v2_version", "0", "-write_id3v1", "0"
)

# --- Flush function: move prepped files to NAS dir, renumbered 001..N ---
function Flush-ToNas {
    param([int]$dirNum)
    $staged = Get-ChildItem -Path $outDir -Filter "*.mp3" -File | Sort-Object Name
    if ($staged.Count -eq 0) { return }

    $tgtDir = Join-Path $SdRoot "$dirNum"
    if (!(Test-Path $tgtDir)) { New-Item -ItemType Directory -Path $tgtDir -Force | Out-Null }

    Write-Host "[MOVE] $($staged.Count) files -> $SdRoot\$dirNum\" -ForegroundColor Magenta
    $n = 1
    foreach ($sf in $staged) {
        $dst = Join-Path $tgtDir ("{0:D3}.mp3" -f $n)
        Move-Item -Path $sf.FullName -Destination $dst -Force
        $n++
    }
}

# --- Add a single file to staging, flush when full ---
function Add-ToStaging {
    param([string]$srcFile)

    $script:seq++
    $numStr = "{0:D3}" -f $script:seq
    $dst = Join-Path $outDir "$numStr.mp3"
    Move-Item -Path $srcFile -Destination $dst -Force
    $script:totalOut++

    if ($script:seq -ge $MaxPerDir) {
        Flush-ToNas -dirNum $script:curDir
        $script:curDir++
        $script:seq = 0
    }
}

$seq     = 0       # current file count in staging (0-based, incremented in Add-ToStaging)
$curDir  = $SdDir  # current NAS directory number
$ok      = 0
$fail    = 0
$skipped = 0
$normCnt = 0
$splitCnt = 0
$totalOut = 0

# Temp dir for splits
$tmpDir = "D:\sounds\prep_tmp"
if (!(Test-Path $tmpDir)) { New-Item -ItemType Directory -Path $tmpDir -Force | Out-Null }

foreach ($f in $files) {
    # Get duration
    $durRaw = & $ffprobe -v error -show_entries format=duration -of csv=p=0 $f.FullName 2>$null
    $dur = [math]::Floor([double]$durRaw)

    # --- TOO SHORT: skip ---
    if ($dur -lt $MinDur) {
        $skipped++
        Write-Host "[SKIP]  $($f.Name) (${dur}s < ${MinDur}s)" -ForegroundColor DarkGray
        continue
    }

    if ($dur -lt $Threshold) {
        # --- SHORT: normalize only ---
        $normCnt++
        $tmpFile = Join-Path $tmpDir "norm_tmp.mp3"
        Write-Host "[NORM]  $($f.Name) (${dur}s)" -ForegroundColor Green

        & $ffmpeg -i $f.FullName @ffArgs $tmpFile 2>&1
        if ($LASTEXITCODE -eq 0) {
            $ok++
            Add-ToStaging -srcFile $tmpFile
        } else {
            $fail++
            Write-Host "[FAIL]  $($f.Name)" -ForegroundColor Red
        }
    } else {
        # --- LONG: normalize + split to temp dir ---
        $splitCnt++
        $chunks = [math]::Ceiling($dur / $Seg)
        Write-Host "[SPLIT] $($f.Name) (${dur}s) -> ~$chunks chunks" -ForegroundColor Yellow

        # Clean temp dir
        Get-ChildItem -Path $tmpDir -Filter "*.mp3" -File | Remove-Item -Force

        $pattern = Join-Path $tmpDir "%03d.mp3"
        & $ffmpeg -i $f.FullName @ffArgs `
            -f segment -segment_time $Seg -segment_start_number 1 `
            -reset_timestamps 1 $pattern 2>&1

        if ($LASTEXITCODE -eq 0) {
            $ok++
            # Add each chunk, skip tail stubs < MinDur
            $chunkFiles = Get-ChildItem -Path $tmpDir -Filter "*.mp3" -File | Sort-Object Name
            foreach ($cf in $chunkFiles) {
                $cfDur = & $ffprobe -v error -show_entries format=duration -of csv=p=0 $cf.FullName 2>$null
                $cfSec = [math]::Floor([double]$cfDur)
                if ($cfSec -lt $MinDur) {
                    Write-Host "[TRIM]  chunk (${cfSec}s) removed" -ForegroundColor DarkGray
                    Remove-Item $cf.FullName -Force
                } else {
                    Add-ToStaging -srcFile $cf.FullName
                }
            }
        } else {
            $fail++
            Write-Host "[FAIL]  $($f.Name)" -ForegroundColor Red
        }
    }
}

# --- Flush remaining files ---
Flush-ToNas -dirNum $curDir

# --- Cleanup temp dir ---
if (Test-Path $tmpDir) { Remove-Item $tmpDir -Recurse -Force }

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "[DONE] $($files.Count) sources: $ok OK, $fail failed, $skipped skipped" -ForegroundColor Cyan
Write-Host "       $normCnt normalized, $splitCnt split" -ForegroundColor Cyan
Write-Host "       $totalOut output files -> dirs $SdDir..$curDir" -ForegroundColor Cyan
Write-Host "       in $SdRoot" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
