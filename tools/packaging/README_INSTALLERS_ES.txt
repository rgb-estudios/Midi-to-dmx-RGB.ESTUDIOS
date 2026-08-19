AEYLA VISUAL DMX 0.3.3-alpha — INSTALADORES DE PRUEBA
=====================================================

Producto: AEYLA Visual DMX / AEYLA Show Player
Fabricante: RGB Estudios
Plataformas: Windows x64 y macOS 11+ Universal (Apple Silicon + Intel)
Formatos: VST3 para REAPER/Ableton; AUv2 para Logic Pro

MATRIZ DE COMPATIBILIDAD OBJETIVO
---------------------------------

Windows 10/11 x64:
- REAPER: VST3
- Ableton Live: VST3

macOS 11+ Intel x86_64:
- REAPER: VST3 Universal
- Ableton Live: VST3 Universal
- Logic Pro compatible con ese macOS: AUv2 Universal

macOS 11+ Apple Silicon arm64:
- REAPER: VST3 Universal nativo
- Ableton Live: VST3 Universal nativo
- Logic Pro: AUv2 Universal nativo

El bundle macOS debe contener simultaneamente x86_64 + arm64. La ruta principal
en Apple Silicon NO depende de Rosetta. Rosetta puede usarse solo como prueba
adicional de compatibilidad.

IMPORTANTE: el piso macOS 11.0 pertenece al plugin. Cada version de Ableton,
REAPER o Logic mantiene sus propios requisitos de sistema. No se afirma
compatibilidad con una combinacion que el DAW mismo no soporte.

ESTADO REAL
-----------

Este paquete es un Test Candidate ALPHA sin firma comercial ni notarizacion.
El nucleo, los formatos, el empaquetado y los validadores automatizados pasan
en Windows y macOS en el HEAD de desarrollo correspondiente al paquete. Aun
faltan validacion con nodo/PAR fisicos, Ableton/Logic reales, ensayo completo,
reproduccion/captura de show final y soak prolongado. No dependas de esta
version en una funcion.

Una combinacion SO/arquitectura/formato/host solo se considera soportada para
show cuando pasa scan, load, interaccion, save/reopen, transporte, MIDI,
capture/replay DMX, Art-Net fisico, recuperacion y ensayo prolongado.

La salida siempre comienza DISARMED. Output Arm es transitorio y nunca se
restaura automaticamente. Configurar o cambiar Art-Net, abrir otro show,
renderizar offline o detectar un error critico debe producir el estado seguro
definido por el runtime.

INSTALADOR WINDOWS
------------------

1. Cierra REAPER y Ableton.
2. Ejecuta AEYLA-0.3.3-alpha-Windows-x64-Setup-UNSIGNED.exe.
3. Acepta la elevacion de administrador.
4. Abre el DAW y ejecuta un rescan VST3 si AEYLA no aparece.

Destino instalado:
C:\Program Files\Common Files\VST3\AeylaVisualDmx.vst3

El desinstalador solo elimina AEYLA. No borra archivos .aeylashow, sesiones del
DAW, grabaciones/takes del usuario ni otros plugins.

La release publica debera usar Authenticode valido y timestamp. El instalador
UNSIGNED se reserva para QA interna.

INSTALADOR MACOS
----------------

1. Cierra REAPER, Ableton y Logic Pro.
2. Abre AEYLA-0.3.3-alpha-macOS-Universal-UNSIGNED.pkg.
3. En alpha unsigned Gatekeeper puede requerir autorizacion manual.
4. Completa la instalacion con una cuenta administradora.
5. Abre el DAW y fuerza un rescan si corresponde.

Destinos instalados:
/Library/Audio/Plug-Ins/VST3/AeylaVisualDmx.vst3
/Library/Audio/Plug-Ins/Components/AeylaVisualDmx.component

La release publica no usara este flujo unsigned: VST3, AUv2 y package deberan
estar firmados con Developer ID, el package debera notarizarse con el servicio
vigente de Apple y verificarse con Gatekeeper antes de publicacion.

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

1. Inserta UNA sola instancia de AEYLA en una pista AEYLA CONTROL.
2. Abre o crea un show.
3. Organiza Show -> Song -> Take/Snapshot/Cue segun el modelo vigente.
4. Configura RX y TX Art-Net en sus adaptadores de red correspondientes.
5. Mantiene la salida desarmada hasta verificar patch, red y direccion del nodo.
6. Arma solo en una prueba controlada, sin publico ni actuacion en curso.
7. Verifica que el runtime continua con la ventana del plugin cerrada.
8. Verifica que offline render/export del DAW NO transmite Art-Net fisico.

LIMITES CONOCIDOS
-----------------

- El standalone Windows se excluye deliberadamente mientras permanezca el P0
  OpenGL #17. El standalone no debe bloquear una ruta VST3 aislada y sana.
- Un socket Art-Net listo no confirma recepcion del nodo fisico.
- REAPER automatico sigue limitado por el entorno headless de CI.
- Ableton Live y Logic Pro requieren prueba manual en equipos reales.
- La captura/reproduccion DMX del flujo Avolites -> AEYLA debe validarse en el
  plugin real y no solo en herramientas de pretest externas.
- No existe aun firma Authenticode, Developer ID ni notarizacion Apple en alpha.
- "Compila" o "instala" NO equivale a "show ready".

Contrato completo:
docs/AEYLA_DISTRIBUTION_COMPATIBILITY_CONTRACT_ES.md

Soporte y trazabilidad:
https://github.com/rgb-estudios/Midi-to-dmx-RGB.ESTUDIOS/pull/14
