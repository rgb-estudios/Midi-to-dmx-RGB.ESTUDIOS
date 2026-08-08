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

echo AEYLA Visual DMX - CLEAN INSTALL
echo.
echo Cierra REAPER, Ableton y cualquier host VST3 antes de continuar.
echo Se eliminara SOLO AEYLA, se respaldara y limpiara SOLO su entrada
echo de cache REAPER, y se instalara el bundle incluido en este paquete.
echo No se eliminan .aeylashow, .RPP ni otros plugins.
echo.

powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "Start-Process -FilePath 'powershell.exe' -Verb RunAs -Wait -ArgumentList @('-NoProfile','-ExecutionPolicy','Bypass','-File','%SCRIPT%','-Action','CleanInstall','-SourceVst3','%SOURCE%','-ResetReaperCache')"
if errorlevel 1 (
  echo ERROR: clean install AEYLA no completado.
  pause
  exit /b 1
)

echo.
echo Clean install AEYLA terminado.
echo Abre REAPER y ejecuta Re-scan si AEYLA no aparece automaticamente.
pause
