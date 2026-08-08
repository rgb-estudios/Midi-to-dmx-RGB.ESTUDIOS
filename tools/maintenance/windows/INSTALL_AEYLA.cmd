@echo off
setlocal
set "SCRIPT=%~dp0AEYLA_VST3_MAINTENANCE.ps1"
set "SOURCE=%~dp0VST3\AeylaVisualDmx.vst3"

if not exist "%SCRIPT%" (
  echo ERROR: no se encontro AEYLA_VST3_MAINTENANCE.ps1
  pause
  exit /b 1
)
if not exist "%SOURCE%" (
  echo ERROR: no se encontro VST3\AeylaVisualDmx.vst3 junto al instalador.
  pause
  exit /b 1
)

echo AEYLA Visual DMX - INSTALL
echo Cierra REAPER, Ableton y cualquier host VST3 antes de continuar.
echo.

powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "Start-Process -FilePath 'powershell.exe' -Verb RunAs -Wait -ArgumentList @('-NoProfile','-ExecutionPolicy','Bypass','-File','%SCRIPT%','-Action','Install','-SourceVst3','%SOURCE%')"
if errorlevel 1 (
  echo ERROR: instalacion AEYLA no completada.
  pause
  exit /b 1
)

echo.
echo AEYLA instalado.
pause
