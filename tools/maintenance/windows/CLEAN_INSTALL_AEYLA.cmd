@echo off
setlocal EnableExtensions
cd /d "%~dp0"

set "SCRIPT=%~dp0AEYLA_VST3_MAINTENANCE.ps1"
set "SOURCE=%~dp0VST3\AeylaVisualDmx.vst3"
set "LOG=%~dp0AEYLA_INSTALL_LAST.log"

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

rem Elevate this maintenance launcher itself. REAPER/Ableton remain normal-user.
net session >nul 2>&1
if not "%errorlevel%"=="0" (
  echo AEYLA necesita permiso de administrador SOLO para copiar el VST3 a Program Files.
  echo Se abrira una solicitud UAC. El DAW no se ejecutara como administrador.
  powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "Start-Process -FilePath '%ComSpec%' -Verb RunAs -ArgumentList '/d','/c','""%~f0""'"
  exit /b 0
)

echo ===============================================================
echo  AEYLA SHOW PLAYER - CLEAN INSTALL
echo ===============================================================
echo.
echo Cierra REAPER, Ableton y cualquier otro host VST3.
echo Este proceso elimina SOLO instalaciones anteriores de AEYLA,
echo limpia SOLO su entrada del cache REAPER y copia este VST3.
echo No borra .RPP, .aeylashow, .aeylatake ni otros plugins.
echo.

powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT%" -Action CleanInstall -SourceVst3 "%SOURCE%" -ResetReaperCache
set "RC=%errorlevel%"

echo.
if not "%RC%"=="0" (
  echo ===============================================================
  echo  ERROR: AEYLA NO FUE INSTALADO
  echo ===============================================================
  if exist "%LOG%" (
    echo.
    type "%LOG%"
  ) else (
    echo No se genero log. Conserva una captura de esta ventana.
  )
  echo.
  pause
  exit /b %RC%
)

echo ===============================================================
echo  PASS: AEYLA INSTALADO
echo ===============================================================
echo Destino esperado:
echo C:\Program Files\Common Files\VST3\AeylaVisualDmx.vst3
echo.
echo Ahora abre REAPER. Si no aparece, haz VST Re-scan.
echo.
pause
exit /b 0
