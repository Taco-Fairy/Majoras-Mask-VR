@echo off
setlocal
cd /d "%~dp0"
if not exist "cache\tmp" mkdir "cache\tmp"
set "TEMP=%~dp0cache\tmp"
set "TMP=%TEMP%"
set "MMVR_ENABLE=1"
set "MMVR_CREATE_COMPLETE_SLOT3=0"
set "MMVR_SMOKE_FRAMES="
set "MMVR_NATIVE_TEST="
set "MMVR_DEBUG_TEST="
set "MMVR_LIFECYCLE_TEST="
set "MMVR_TOWN_TEST="
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "launch-mmvr.ps1" %*
endlocal
