@echo off
setlocal enabledelayedexpansion
:: ---------------------------------------------------------------
:: tosd.bat — Distribute prepped MP3s into numbered SD card dirs
::
:: Usage:  tosd.bat <start_dir> [source] [target] [max_per_dir]
::
::   start_dir      First directory number (e.g. 127)
::   source         Source folder (default: D:\sound\prepped)
::   target         SD card base (default: V:\Kwal\SDcard)
::   max_per_dir    Files per directory (default: 99)
::
:: Example: tosd.bat 127
::   D:\sound\prepped\001.mp3..088.mp3
::     -> V:\Kwal\SDcard\127\001.mp3..088.mp3
::
:: Example: tosd.bat 127  (with 150 files)
::   001-099 -> V:\Kwal\SDcard\127\001.mp3..099.mp3
::   100-150 -> V:\Kwal\SDcard\128\001.mp3..051.mp3
:: ---------------------------------------------------------------

:: --- Args ---
set "STARTDIR=%~1"
if not defined STARTDIR (
    echo.
    echo   Usage: %~nx0 ^<start_dir_number^> [source] [target] [max]
    echo   Example: %~nx0 127
    echo.
    exit /b 1
)

set "SRC=%~2"
if not defined SRC set "SRC=D:\sound\prepped"
set "DST=%~3"
if not defined DST set "DST=V:\Kwal\SDcard"
set "MAX=%~4"
if not defined MAX set "MAX=99"

:: --- Validate ---
if not exist "%SRC%" (echo [ERR] Source not found: %SRC% & exit /b 1)
if not exist "%DST%" (echo [ERR] Target not found: %DST% & exit /b 1)

echo.
echo [INFO] Source : %SRC%
echo [INFO] Target : %DST%
echo [INFO] Start  : dir %STARTDIR%, max %MAX% per dir
echo.

set /a CURDIR=%STARTDIR%
set /a SEQ=1
set /a TOTAL=0

:: --- Process files in sorted order ---
for /F "tokens=*" %%F in ('dir /b /on "%SRC%\*.mp3" 2^>nul') do (
    set /a TOTAL+=1

    :: Create target dir if needed
    set "TGTDIR=%DST%\!CURDIR!"
    if not exist "!TGTDIR!" mkdir "!TGTDIR!"

    :: Format sequence as 3-digit
    set "NUM=00!SEQ!"
    set "NUM=!NUM:~-3!"

    echo [COPY] %%F -^> !CURDIR!\!NUM!.mp3

    copy /y "%SRC%\%%F" "!TGTDIR!\!NUM!.mp3" >nul
    if !errorlevel! neq 0 (
        echo [FAIL] %%F
    )

    :: Advance
    set /a SEQ+=1
    if !SEQ! gtr %MAX% (
        set /a SEQ=1
        set /a CURDIR+=1
        echo [INFO] Dir !CURDIR! starting...
    )
)

if %TOTAL% equ 0 (
    echo [WARN] No MP3 files found in %SRC%
    popd
    pause
    exit /b 1
)

set /a DIRS=CURDIR-STARTDIR+1
echo.
echo ========================================
echo [DONE] %TOTAL% files -^> %DIRS% dirs (%STARTDIR%..%CURDIR%)
echo        in %DST%
echo ========================================
pause
