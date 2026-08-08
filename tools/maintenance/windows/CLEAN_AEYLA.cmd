@echo off
setlocal
set "SCRIPT=%~dp0AEYLA_VST3_MAINTENANCE.ps1"

if not exist "%SCRIPT%" (
  echo ERROR: no se encontro AEYLA_VST3_MAINTENANCE.ps1
  pause
  exit /b 1
)

echo AEYLA Visual DMX - CLEAN
echo.
echo Cierra REAPER, Ableton y cualquier host VST3 antes de continuar.
echo Este proceso elimina SOLO AeylaVisualDmx.vst3 de rutas conocidas
echo y retira SOLO la entrada AEYLA del cache VST de REAPER, con backup.
echo No elimina .aeylashow, .RPP ni otros plugins.
echo.

powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "Start-Process -FilePath 'powershell.exe' -Verb RunAs -Wait -ArgumentList @('-NoProfile','-ExecutionPolicy','Bypass','-File','%SCRIPT%','-Action','Clean','-ResetReaperCache')"
if errorlevel 1 (
  echo ERROR: mantenimiento AEYLA no completado.
  pause
  exit /b 1
)

echo.
echo Limpieza AEYLA terminada.
pause
