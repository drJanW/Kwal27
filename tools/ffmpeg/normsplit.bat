@echo off
setlocal enabledelayedexpansion
:: Gebruik: norsp.bat "mp3_dir" [segment_s=30]

:: Args
set "INDIR=%~1"
if not defined INDIR (echo Gebruik: %~nx0 "mp3_dir" [segment_s]&exit /b 1)
set "SEG=%~2"
if not defined SEG set "SEG=30"

:: Tools (relatief naast .bat, anders PATH)
set "SCRIPT_DIR=%~dp0"
set "FFMPEG=%SCRIPT_DIR%bin\ffmpeg.exe"
if not exist "%FFMPEG%" set "FFMPEG=ffmpeg"

:: Setup
pushd "%INDIR%" || (echo [ERR] map bestaat niet & exit /b 1)
set "ROOT=%CD%"
set "OUTROOT=%ROOT%\split"
if not exist "%OUTROOT%" mkdir "%OUTROOT%"
echo [INFO] seg=%SEG%s

:: Process
for /R "%ROOT%" %%F in (*.mp3) do (
  set "IN=%%~fF"

  rem skip alles onder split\
  if /I "!IN:%OUTROOT%\=!" NEQ "!IN!" (
    echo [SKIP]
  ) else (
    set "DIR=%%~dpF"
    set "RELDIR=!DIR:%ROOT%\=!"
    set "OUTDIR=%OUTROOT%\!RELDIR!"
    if not exist "!OUTDIR!" mkdir "!OUTDIR!"

    rem basisnaam: eerste 20 tekens, verder niets
    set "S=%%~nF"
    set "S=!S:~0,20!"
    if "!S!"=="" set "S=seg"

    echo [PROC]
    "%FFMPEG%" -loglevel error -y -i "!IN!" -map 0:a:0 -vn -sn -dn -af "loudnorm=I=-16:TP=-1.5:LRA=11" -ac 1 -ar 44100 -c:a libmp3lame -b:a 128k -write_xing 0 -map_metadata -1 -id3v2_version 0 -write_id3v1 0 -f segment -segment_time %SEG% -reset_timestamps 1 "!OUTDIR!\!S!_%%05d.mp3"
  )
)

echo [DONE]
popd
pause
