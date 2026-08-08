AEYLA VISUAL DMX 0.3.3-alpha — INSTALADORES DE PRUEBA
=====================================================

Producto: AEYLA Visual DMX
Fabricante: RGB Estudios
Plataformas: Windows x64 y macOS 11+ universal (Apple Silicon + Intel)
Formatos: VST3 para REAPER/Ableton; AUv2 para Logic Pro

ESTADO REAL
-----------

Este paquete es un Test Candidate ALPHA sin firma comercial ni notarizacion.
El nucleo, los formatos y los validadores automatizados pasan en Windows y
macOS. Aun faltan validacion con nodo/PAR fisicos, Ableton/Logic reales, ensayo
completo y soak de ocho horas. No dependas de esta version en una funcion.

La salida siempre comienza DISARMED + BLACKOUT. Configurar o cambiar Art-Net,
abrir otro show, renderizar offline o detectar un error fuerza desarme seguro.

INSTALADOR WINDOWS
------------------

1. Cierra REAPER y Ableton.
2. Ejecuta AEYLA-0.3.3-alpha-Windows-x64-Setup-UNSIGNED.exe.
3. Acepta la elevacion de administrador.
4. Abre el DAW y ejecuta un rescan VST3 si AEYLA no aparece.

Destino instalado:
C:\Program Files\Common Files\VST3\AeylaVisualDmx.vst3

El desinstalador solo elimina AEYLA. No borra archivos .aeylashow, sesiones del
DAW ni otros plugins.

INSTALADOR MACOS
----------------

1. Cierra REAPER, Ableton y Logic Pro.
2. Abre AEYLA-0.3.3-alpha-macOS-Universal-UNSIGNED.pkg.
3. Si Gatekeeper lo bloquea por no estar firmado/notarizado, usa clic derecho
   > Abrir o autoriza una vez en Privacidad y seguridad.
4. Completa la instalacion con una cuenta administradora.
5. Abre el DAW y fuerza un rescan si corresponde.

Destinos instalados:
/Library/Audio/Plug-Ins/VST3/AeylaVisualDmx.vst3
/Library/Audio/Plug-Ins/Components/AeylaVisualDmx.component

PACK MANUAL VST3 / AUV2
-----------------------

Windows:
- Abre la carpeta Windows.
- Ejecuta CLEAN_INSTALL_AEYLA.cmd como administrador.

macOS:
- Abre la carpeta macOS.
- Ejecuta INSTALL_AEYLA.command.
- AUDIT_AEYLA.command informa que copias existen.
- UNINSTALL_AEYLA.command elimina exclusivamente AEYLA.

FLUJO DE PRUEBA MINIMO
----------------------

1. Inserta una sola instancia de AEYLA en una pista MIDI.
2. Abre o crea un .aeylashow.
3. Programa mediante Look -> Cue -> Song -> Show.
4. Configura OUTPUT SETUP con IPv4@universo, por ejemplo 2.0.0.20@0.
5. Mantiene BLACKOUT activo hasta verificar patch, red y direccion del nodo.
6. Arma solo en una prueba controlada, sin publico ni actuacion en curso.

LIMITES CONOCIDOS
-----------------

- El standalone Windows se excluye deliberadamente por el P0 OpenGL #17.
- Un socket Art-Net listo no confirma recepcion del nodo fisico.
- REAPER automatico sigue limitado por el entorno headless de CI.
- Ableton Live y Logic Pro requieren prueba manual en equipos reales.
- No existe firma Authenticode, Developer ID ni notarizacion Apple en alpha.

Soporte y trazabilidad:
https://github.com/rgb-estudios/Midi-to-dmx-RGB.ESTUDIOS/pull/14
