@echo off
setlocal enabledelayedexpansion
:: ---------------------------------------------------------------
:: prep.bat — Normalize (+ split if long) MP3s into numbered files
::
:: Usage:  prep.bat [mp3_dir] [segment_seconds] [start_number]
::
::   mp3_dir          Directory with source MP3 files (default: D:\sounds\new)
::   segment_seconds  Chunk length in seconds (default: 42)
::   start_number     First output number (default: 1)
::
:: Files shorter than 105s: normalize only, single output file
:: Files >= 105s: normalize + split into segment_seconds chunks
::
:: Output: D:\sound\prepped\001.mp3, 002.mp3, ... (flat, sequential)
:: Numbering continues across source files.
::
:: Audio: mono, 44100 Hz, 128k, loudnorm, no metadata/headers
:: ---------------------------------------------------------------

:: --- Tools ---
set "FFDIR=D:\Programs\FFmpeg"
set "FFMPEG=%FFDIR%\ffmpeg.exe"
set "FFPROBE=%FFDIR%\ffprobe.exe"
if not exist "%FFMPEG%" (echo [ERR] ffmpeg not found: %FFMPEG% & exit /b 1)
if not exist "%FFPROBE%" (echo [ERR] ffprobe not found: %FFPROBE% & exit /b 1)

:: --- Args ---
set "INDIR=%~1"
if not defined INDIR set "INDIR=D:\sounds\new"
set "SEG=%~2"
if not defined SEG set "SEG=42"
set "START=%~3"
if not defined START set "START=1"

:: --- Config ---
set "SPLIT_THRESHOLD=105"

:: --- Setup ---
pushd "%INDIR%" || (echo [ERR] Directory does not exist: %INDIR% & exit /b 1)
set "ROOT=%CD%"
set "OUTDIR=D:\sound\prepped"
if not exist "%OUTDIR%" mkdir "%OUTDIR%"

echo.
echo [INFO] Source   : %ROOT%
echo [INFO] Output   : %OUTDIR%
echo [INFO] Segment  : %SEG%s (split threshold: %SPLIT_THRESHOLD%s)
echo [INFO] Start at : %START%
echo.

set /a TOTAL=0
set /a OK=0
set /a FAIL=0
set /a SHORT=0
set /a SPLIT=0
set /a NEXT=%START%

:: --- Process each mp3 ---
for %%F in ("%ROOT%\*.mp3") do (
    set /a TOTAL+=1
    set "IN=%%~fF"
    set "NAME=%%~nF"

    :: Get duration in seconds (integer)
    set "DUR=0"
    for /F "usebackq tokens=*" %%D in (`"%FFPROBE%" -v error -show_entries format^=duration -of csv^=p^=0 "!IN!" 2^>nul`) do (
        for /F "tokens=1 delims=." %%I in ("%%D") do set "DUR=%%I"
    )

    :: Format current number for display
    set "DNUM=00!NEXT!"
    set "DNUM=!DNUM:~-3!"

    if !DUR! lss %SPLIT_THRESHOLD% (
        :: --- SHORT: normalize only, single file ---
        set /a SHORT+=1
        echo [NORM] !NAME!.mp3 ^(!DUR!s^) -^> !DNUM!.mp3

        "%FFMPEG%" -loglevel error -y -i "!IN!" ^
          -map 0:a:0 -vn -sn -dn ^
          -af "loudnorm=I=-16:TP=-1.5:LRA=11" ^
          -ac 1 -ar 44100 ^
          -c:a libmp3lame -b:a 128k ^
          -write_xing 0 -map_metadata -1 -id3v2_version 0 -write_id3v1 0 ^
          "%OUTDIR%\!DNUM!.mp3"

        if !errorlevel! equ 0 (
            set /a OK+=1
            set /a NEXT+=1
        ) else (
            set /a FAIL+=1
            echo [FAIL] !NAME!
        )
    ) else (
        :: --- LONG: normalize + split ---
        set /a SPLIT+=1
        echo [SPLIT] !NAME!.mp3 ^(!DUR!s^) -^> %SEG%s chunks from !DNUM!

        "%FFMPEG%" -loglevel error -y -i "!IN!" ^
          -map 0:a:0 -vn -sn -dn ^
          -af "loudnorm=I=-16:TP=-1.5:LRA=11" ^
          -ac 1 -ar 44100 ^
          -c:a libmp3lame -b:a 128k ^
          -write_xing 0 -map_metadata -1 -id3v2_version 0 -write_id3v1 0 ^
          -f segment -segment_time %SEG% -segment_start_number !NEXT! ^
          -reset_timestamps 1 ^
          "%OUTDIR%\%%03d.mp3"

        if !errorlevel! equ 0 (
            set /a OK+=1
            rem Count total files in output to advance NEXT
            set /a COUNT=0
            for %%S in ("%OUTDIR%\*.mp3") do set /a COUNT+=1
            set /a NEXT=COUNT+%START%
        ) else (
            set /a FAIL+=1
            echo [FAIL] !NAME!
        )
    )
    echo.
)

set /a PRODUCED=NEXT-START
echo ========================================
echo [DONE] %TOTAL% sources: %OK% OK, %FAIL% failed
echo        %SHORT% normalized, %SPLIT% split
echo        %PRODUCED% output files: %START%..!NEXT! in %OUTDIR%
echo ========================================
popd
pause
