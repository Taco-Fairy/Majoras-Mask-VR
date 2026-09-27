@echo off
cd /d "%~dp0"
echo This resets VR camera, height and controls. Your game saves are kept.
echo Close the game before continuing.
pause
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0recover-vr.ps1"
pause
