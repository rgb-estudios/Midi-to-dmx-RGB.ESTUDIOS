# AEYLA Visual DMX — matriz estricta de compatibilidad DAW

## Regla de declaración

AEYLA solo se declara compatible con una combinación concreta de:

- sistema operativo y versión;
- arquitectura de CPU;
- formato de plug-in;
- DAW y versión;
- versión exacta de AEYLA;
- backend gráfico;
- proyecto `.aeylashow` y protocolo de salida probado.

Que un DAW anuncie soporte para VST3 o Audio Units **no demuestra** que una
versión de AEYLA sea compatible. Cada celda debe completar todas las pruebas de
este documento y conservar evidencia reproducible.

No se utilizarán expresiones como «funciona en cualquier DAW» o «sin ningún
problema». El estado permitido es uno de:

- `NO BUILD`: el formato no existe;
- `COMPILED`: el bundle compila para la arquitectura indicada;
- `FORMAT VALIDATED`: pasa Steinberg Validator o `auval`;
- `HOST SCANNED`: el DAW lo reconoce y crea una instancia;
- `HOST FUNCTIONAL`: completa el protocolo funcional;
- `SHOW CANDIDATE`: completa hardware, ensayo, soak y recuperación;
- `REJECTED`: existe un fallo conocido que impide aprobar la celda.

## Matriz objetivo inicial

| Sistema | DAW objetivo | Versión congelada | Formato principal | Arquitectura | Estado actual |
|---|---|---:|---|---|---|
| Windows 11 | Ableton Live | 12.4.3 | VST3 | x86_64 | COMPILED / no host-tested |
| Windows 10 22H2 | Ableton Live | 12.4.3 | VST3 | x86_64 | no target test |
| macOS 15.6+ | Ableton Live | 12.4.3 | VST3 | universal arm64+x86_64 | build pendiente de nuevo gate |
| Windows 11 | REAPER | 7.78 | VST3 | x86_64 | no host-tested |
| Windows 10 22H2 | REAPER | 7.78 | VST3 | x86_64 | no target test |
| macOS 15.6+ | REAPER | 7.78 | VST3 | universal arm64+x86_64 | no host-tested |
| macOS 15.6+ | REAPER | 7.78 | AUv2 | universal arm64+x86_64 | target añadido / no `auval` |
| macOS 15.6+ | Logic Pro | 12.3 | AUv2 (`aumu`) | universal arm64+x86_64 | target añadido / no `auval` |
| Windows 11 | Standalone | 0.3.x | APP | x86_64 | source build / target launch pendiente |
| macOS 15.6+ | Standalone | 0.3.x | APP | universal arm64+x86_64 | build pendiente de nuevo gate |

### Política macOS

- El bundle se compila universal para `arm64` y `x86_64`.
- El deployment target técnico es macOS 11.0.
- El mínimo de certificación para Live 12 es macOS 11.7.10.
- La certificación actual de Logic se realiza en Apple silicon y la versión de
  macOS exigida por la versión congelada de Logic.
- Rosetta no cuenta como evidencia principal de compatibilidad Apple silicon.

### Política Windows

- Solo x86_64 para la primera Show Candidate.
- El VST3 se instala en `C:\Program Files\Common Files\VST3`.
- Windows ARM/Prism, Wine y builds de 32 bits quedan fuera del alcance inicial.

## Formatos que debe producir una misma revisión

| Artefacto | Windows | macOS | Hosts |
|---|---:|---:|---|
| `AeylaVisualDmx.vst3` | requerido | requerido universal | Ableton, REAPER |
| `AeylaVisualDmx.component` AUv2 | no aplica | requerido universal | Logic, REAPER; Ableton secundario |
| Standalone APP | requerido | requerido universal | programación, ensayo y fallback |
| `.aeylashow` | idéntico | idéntico | fuente de verdad compartida |

No habrá motores distintos por formato. VST3, AUv2 y standalone enlazan el mismo
modelo de proyecto, scene engine, compilador semántico, seguridad y salida.

## Gate 1 — validación del formato

### VST3 Windows y macOS

- [ ] Bundle contiene la arquitectura correcta.
- [ ] Steinberg Validator completa toda la suite sin crash, error ni timeout.
- [ ] Bus MIDI/eventos de entrada disponible.
- [ ] Sin entrada de audio.
- [ ] Salida estéreo permanece en silencio.
- [ ] Tamaños de bloque variables y 32/64-bit sample format.
- [ ] Estado versionado válido y estados corruptos rechazados.
- [ ] Editor abre, cierra y redimensiona.
- [ ] Destrucción de instancia no deja workers ni sockets vivos.

### AUv2 macOS

- [ ] `plutil -lint` aprueba el `Info.plist`.
- [ ] Bundle universal contiene `arm64` y `x86_64`.
- [ ] Firma local de CI válida para test; Developer ID para distribución.
- [ ] `auval -v aumu AyVD RGBE` termina con resultado aprobado.
- [ ] El componente aparece como instrumento de software.
- [ ] Render de audio siempre silencioso.
- [ ] Estado y parámetros sobreviven serialize/restore.
- [ ] Vista gráfica abre y cierra sin afectar el runtime.

`auval` no prueba la vista ni el flujo artístico; aprobarlo es obligatorio pero
no suficiente.

## Gate 2 — escaneo e instalación por host

Para cada celda:

1. instalar desde un paquete limpio;
2. borrar caché o forzar reescaneo según el host;
3. confirmar fabricante `RGB Estudios`;
4. confirmar nombre `AEYLA Visual DMX`;
5. crear una instancia en una pista de instrumento/MIDI válida;
6. comprobar que no genera audio;
7. abrir y cerrar la interfaz diez veces;
8. redimensionar y comprobar escala/DPI;
9. cerrar el proyecto y el DAW sin crash;
10. desinstalar y confirmar que no quedan bundles duplicados.

## Gate 3 — comportamiento MIDI y transporte

Cada host debe completar:

- Note On y Note Off de las notas asignadas.
- Velocity mínima, media y máxima.
- Notas fuera del mapa no alteran la escena.
- Stop publica `all notes off` y el estado seguro definido.
- Pause/reanudar no duplica Note On.
- Seek hacia delante y atrás reconstruye la escena correcta.
- Inicio desde mitad de canción produce el estado correcto sin depender del
  historial anterior.
- Loop en límites de escena no deja notas colgadas.
- Cambio de tempo y compás conserva posicionamiento en ticks.
- Cierre de la ventana del plug-in no detiene runtime ni eventos.
- Bypass, desactivación de pista y eliminación de instancia desarman salida.
- Varias instancias no pueden competir silenciosamente por el mismo backend.
- Cero eventos descartados durante una pasada completa del set.

## Gate 4 — proyecto y estado del host

- Crear/abrir/guardar `.aeylashow` desde el plug-in.
- Guardar el proyecto del DAW y cerrarlo.
- Reabrir y resolver el mismo `.aeylashow` por UUID/locator.
- Proyecto faltante o UUID distinto → `PROJECT INVALID`, blackout y disarmed.
- Output Arm nunca se restaura.
- Ejecutores momentáneos nunca se restauran.
- Cambios no guardados se advierten antes de descartar.
- Duplicar pista o instancia no crea dos propietarios de Art-Net.
- Mover el `.aeylashow` produce un diagnóstico claro y recuperable.
- Estado antiguo compatible migra; major no compatible se rechaza.

## Gate 5 — flujo específico por DAW

### Ableton Live

- VST3 x64 en Windows y universal nativo en macOS.
- Prueba Arrangement y Session View.
- Clips MIDI con Note On/Off y velocity.
- Stop All Clips, relanzamiento, loop y cambio de locator.
- Freeze/Flatten y export no deben activar salida inesperadamente.
- Guardar, Collect All and Save y reapertura.
- Crash recovery del Set no rearma output.
- Editor cerrado durante reproducción completa.

### REAPER

- VST3 Windows/macOS y AUv2 macOS.
- Pista con MIDI item y monitorización.
- Seek/scrub, loop selection y cambio de play rate.
- FX online/offline, bypass, anticipative FX y auto-bypass.
- Guardar/reabrir `.RPP` y plantilla de pista.
- Ejecutar en proceso, proceso dedicado y firewall de plug-in cuando aplique.
- Comparar VST3 y AUv2 en macOS: mismo proyecto y misma trama DMX.

### Logic Pro

- AUv2 universal, instrumento `aumu`.
- Plug-in Manager muestra validación compatible.
- Pista de Software Instrument con regiones MIDI.
- Cycle, play from selection, stop, seek y cambio de tempo.
- Cerrar ventana sin detener runtime.
- Guardar/reabrir proyecto y alternativas de proyecto.
- Probar hosting in-process y Audio Units hosting service cuando Logic lo permita.
- Desactivar/cargar el channel strip sin restaurar Output Arm.

## Gate 6 — hardware y show

Una celda `HOST FUNCTIONAL` todavía no es `SHOW CANDIDATE`. Para el show de Aeyla:

- [ ] Art-Net unicast real con nodo nombrado.
- [ ] Captura ArtDMX comparada contra frame esperado.
- [ ] Blackout y disarm físicos.
- [ ] Pérdida/recuperación de red y nodo.
- [ ] Tres ensayos completos sin intervención.
- [ ] Soak de ocho horas.
- [ ] Prueba de backup standalone en otro equipo.
- [ ] Checklist de montaje y fallback ejecutado por un segundo operador.

## Evidencia requerida

Cada ejecución debe registrar:

- fecha UTC;
- versión del DAW;
- versión de OS y arquitectura;
- SHA del commit y checksum del bundle;
- formato;
- resultado de Validator/`auval`;
- proyecto de prueba utilizado;
- duración de la prueba;
- drops MIDI, errores de estado, frames DMX y fallos de backend;
- capturas/logs;
- resultado PASS/FAIL por caso.

Sin esta evidencia, la celda permanece como `COMPILED` o `HOST SCANNED`, nunca
como compatible ni show-ready.

## Fuentes oficiales de requisitos

- Ableton, VST en Windows: https://help.ableton.com/hc/en-us/articles/209071729-Using-VST-plug-ins-on-Windows
- Ableton, AU/VST en macOS: https://help.ableton.com/hc/en-us/articles/209068929-Using-AU-and-VST-plug-ins-on-macOS
- Ableton, requisitos de Live: https://help.ableton.com/hc/en-us/articles/115001663530-Live-Minimum-System-Requirements
- REAPER, formatos soportados: https://www.reaper.fm/about.php
- REAPER, descarga/versiones: https://www.reaper.fm/download.php
- Apple, Audio Units en Logic: https://support.apple.com/guide/logicpro/lgcp22a0dab0/mac
- Apple, Plug-in Manager: https://support.apple.com/guide/logicpro/lgcp9e26ef17/mac
- Apple, release notes de Logic: https://support.apple.com/109503
- Apple, `auval`: https://developer.apple.com/library/archive/documentation/MusicAudio/Conceptual/AudioUnitProgrammingGuide/AudioUnitDevelopmentFundamentals/AudioUnitDevelopmentFundamentals.html
