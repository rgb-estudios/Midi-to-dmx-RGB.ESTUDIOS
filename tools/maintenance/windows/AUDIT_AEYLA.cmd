@echo off
setlocal
set "SCRIPT=%~dp0AEYLA_VST3_MAINTENANCE.ps1"
if not exist "%SCRIPT%" (
  echo ERROR: no se encontro AEYLA_VST3_MAINTENANCE.ps1
  pause
  exit /b 1
)

powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT%" -Action Audit
pause
