@echo off
setlocal enabledelayedexpansion
:: Gebruik: norm.bat "mp3_dir"

:: Args
set "INDIR=%~1"
if not defined INDIR (
    echo Gebruik: %~nx0 "mp3_dir"
    exit /b 1
)

:: Tools
set "SCRIPT_DIR=%~dp0"
set "FFMPEG=%SCRIPT_DIR%bin\ffmpeg.exe"
if not exist "%FFMPEG%" set "FFMPEG=ffmpeg"

:: Setup
pushd "%INDIR%" || (echo [ERR] map bestaat niet & exit /b 1)
set "ROOT=%CD%"
set "OUTROOT=%ROOT%\converted"

if not exist "%OUTROOT%" mkdir "%OUTROOT%"

echo [INFO] Normaliseren zonder splits

:: Process
for /R "%ROOT%" %%F in (*.mp3) do (
    set "IN=%%~fF"

    rem skip alles onder converted\
    if /I "!IN:%OUTROOT%\=!" neq "!IN!" (
        echo [SKIP] !IN!
    ) else (
        rem DIRECTORY
        set "DIR=%%~dpF"

        rem Maak echte relative path:
        set "RELDIR=!DIR:%ROOT%=!"
        rem strip leading backslash
        if "!RELDIR!"=="\" set "RELDIR="

        set "OUTDIR=%OUTROOT%!RELDIR!"
        if not exist "!OUTDIR!" mkdir "!OUTDIR!"

        rem Output naam
        set "NAME=%%~nF"
        set "OUT=!OUTDIR!\!NAME!.mp3"

        echo [PROC] !IN!

        "%FFMPEG%" -loglevel error -y -i "!IN!" ^
          -map 0:a:0 -vn -sn -dn ^
          -af "loudnorm=I=-16:TP=-1.5:LRA=11" ^
          -ac 1 -ar 44100 ^
          -c:a libmp3lame -b:a 128k ^
          -write_xing 0 -map_metadata -1 -id3v2_version 0 -write_id3v1 0 ^
          "!OUT!"
    )
)

echo [DONE]
popd
pause
