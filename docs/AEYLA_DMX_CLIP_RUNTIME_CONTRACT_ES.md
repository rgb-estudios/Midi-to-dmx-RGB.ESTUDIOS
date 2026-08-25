# AEYLA — Contrato de runtime DMX Clip

Estado: **R06 / arquitectura corregida para integración MIDI**  
Ámbito: captura Art-Net, persistencia, edición no destructiva, consolidación, reproducción por eventos MIDI y salida Art-Net de un universo.

## 1. Modelo operativo bloqueado

AEYLA no reemplaza al DAW ni a Avolites.

```text
AVOLITES
  autoría creativa / programación del show
        ↓ Art-Net U1
AEYLA CAPTURE
  muestreo determinista 44 Hz
        ↓
TAKE RAW .aeylatake
  fuente inmutable
        ↓
DMX CLIP EDITOR
  trim / split / offset / hold / blackout / marcadores
        ↓
CONSOLIDAR
  IN/OUT pasan a ser 0 → duración del clip final
        ↓
CLIP DMX CONSOLIDADO
        ↓
DAW
  audio / MIDI / triggers globales / video / otros plugins
        ↓ notas MIDI
AEYLA PLAYER
  selección + PLAY / PAUSE / NEXT / LAUNCH
        ↓
ART-NET TX
  interfaz física elegida
        ↓
NODO U1 → DMX
```

El orden físico de tracks, escenas o clips dentro del DAW **no define la posición artística del DMX**. El DAW es host y router de eventos; AEYLA reproduce el clip DMX seleccionado desde su propio cursor relativo.

## 2. Regla principal de tiempo

La reproducción artística no depende de la posición absoluta de la timeline del DAW y tampoco usa reloj de pared.

Después de consolidar, cada clip posee una timeline relativa propia:

```text
0 muestras ───────────────────────── final del clip
```

Al recibir PLAY/LAUNCH, AEYLA pone el cursor relativo en cero y avanza contando únicamente muestras de audio procesadas mientras el clip está en estado PLAYING.

```text
clipSampleCursor += processedSamples
frame = floor(clipSampleCursor * dmxFps / sampleRate)
```

Un trigger MIDI dentro de un bloque debe respetar su `sampleOffset`: el cursor comienza exactamente en el punto del bloque donde ocurrió el evento, no al inicio arbitrario del bloque.

Consecuencias obligatorias:

- PLAY/LAUNCH inicia el clip consolidado desde 0.
- PAUSE conserva el cursor y mantiene el último frame válido.
- RESUME continúa desde el mismo cursor relativo.
- RETRIGGER/PLAY FROM START vuelve a 0 de forma determinista.
- el final del clip entra en HOLD del último frame salvo una política explícita distinta.
- mover, reordenar o renombrar tracks del DAW no desplaza el DMX.
- hacer seek o loop en la timeline global no mueve el clip AEYLA por sí solo.
- sólo un evento MIDI explícito cambia selección, transporte o posición del clip.
- cerrar la interfaz gráfica no puede detener el runtime.
- render offline inhibe siempre la salida física.

El reloj de pared queda prohibido para calcular la posición artística del DMX.

## 3. Captura

### Entrada

- Art-Net, un universo.
- RX ligado explícitamente a una IPv4 local elegida.
- tasa normalizada: **44 Hz**.
- una fuente Art-Net queda autoritativa durante cada grabación.

### Persistencia de producción

La grabación de producción no puede acumular el Take completo en RAM.

Ruta requerida:

```text
sampler 44 Hz
   ↓
cola SPSC fija 512 KiB
   ↓
thread de disco
   ↓
.aeylatake.tmp
   ↓ STOP
frame count final + checksum + fsync + validación
   ↓
rename atómico
   ↓
.aeylatake
```

- `try_push_frame()` no asigna memoria, no espera y no hace I/O.
- overflow de cola = fallo visible; nunca se omiten frames silenciosamente.
- checkpoints durables periódicos.
- un archivo incompleto nunca reemplaza al último Take válido.

## 4. Reproducción file-backed

Un Take histórico no se mantiene completo en memoria.

- validación de checksum al abrir.
- acceso aleatorio por índice de frame.
- caché fija actual: **128 frames / 64 KiB**.
- carga de clip no requiere mantener el payload completo en RAM.
- I/O de archivo sólo en thread no realtime.

El clip consolidado puede conservar una referencia al RAW + receta de edición o materializar un archivo optimizado de reproducción. La fuente RAW permanece inmutable.

## 5. Selección y transporte por MIDI

La interfaz de control mínima del show es independiente de la timeline del DAW.

Comandos mínimos:

- `SELECT SONG n`: deja una canción/clip preparada.
- `NEXT SONG`: avanza la selección preparada.
- `PREVIOUS SONG`: opcional pero recomendado.
- `PLAY / RETRIGGER`: inicia la selección desde 0.
- `PAUSE`: congela cursor y DMX en el último frame.
- `RESUME`: continúa desde el cursor congelado.
- `STOP / RESET`: recomendado para volver a estado preparado sin depender de un toggle ambiguo.

También se permite un comando atómico `LAUNCH SONG n` que equivale a seleccionar + iniciar desde 0 en el mismo instante MIDI. Este camino es preferible cuando un playback global del show debe disparar luz, video y otros elementos simultáneamente.

Los números de nota no se fijan en el motor; deben ser asignables/MIDI Learn para integrarse con una sesión ya existente.

## 6. Canción activa vs canción preparada

Para evitar cortes accidentales, AEYLA distingue:

- `ACTIVE`: clip que está sonando o en pausa.
- `QUEUED`: canción seleccionada para el próximo PLAY/LAUNCH.

`NEXT SONG` y `SELECT SONG` modifican QUEUED sin interrumpir ACTIVE. `PLAY/LAUNCH` convierte QUEUED en ACTIVE y comienza desde 0.

Esto permite preparar la siguiente canción durante la ejecución de la actual sin modificar el DMX que está saliendo.

## 7. Autoridad de salida

La salida Art-Net tiene dos autoridades mutuamente priorizadas:

1. salida semántica/modelo;
2. reproducción de DMX Clip.

Mientras el DMX Clip está armado y posee un estado ACTIVE válido, su autoridad override gana.

Estados mínimos:

- `DISARMED`: sin autoridad física de clip.
- `ARMED / READY`: sistema preparado, canción QUEUED disponible.
- `PLAYING`: cursor relativo avanzando.
- `PAUSED / HOLD`: cursor congelado; mantiene último frame.
- `ENDED / HOLD`: clip terminado; mantiene último frame.
- `OFFLINE`: salida física inhibida.
- `FAULT`: error de archivo, host o storage; fail-closed.

## 8. Host y watchdog

El DAW sigue siendo necesario como host de plugin y fuente de eventos MIDI, pero su posición absoluta no gobierna el clip.

El callback de audio cumple tres funciones:

1. entregar bloques de muestras para avanzar el cursor relativo;
2. entregar eventos MIDI con su `sampleOffset` exacto;
3. demostrar que el host sigue vivo.

El reloj de pared sólo se permite como vigilancia de vida:

- cada callback coherente actualiza heartbeat;
- si el callback desaparece por el umbral de seguridad, AEYLA entra en política fail-closed;
- heartbeat jamás calcula el frame artístico.

Un STOP/SEEK/LOOP de la timeline del DAW no reposiciona AEYLA salvo que la sesión envíe además un comando MIDI definido para hacerlo.

## 9. Editor DMX

El editor debe comportarse como editor de un sample, no como un DAW paralelo.

Primera etapa segura:

- timeline temporal;
- playhead;
- zoom y desplazamiento;
- IN / OUT;
- split;
- crop;
- offset;
- HOLD;
- BLACKOUT;
- marcadores;
- actividad DMX resumida;
- volver al RAW;
- consolidar una nueva versión.

Después de CONSOLIDAR, el punto IN pasa a ser tiempo 0 del clip final. El reproductor de show no necesita conocer la posición que ese fragmento ocupaba originalmente en la grabación RAW.

### Prohibición inicial

No interpolar linealmente los 512 canales para crear fades genéricos.

Canales de strobe, modo, macro o funciones discretas podrían atravesar valores inválidos. Las transiciones continuas sólo podrán habilitarse después de clasificar canales/atributos seguros.

## 10. Integración con la sesión del show

La sesión puede conservar su estructura existente.

Ejemplo conceptual:

```text
PLAY GLOBAL CANCIÓN 06
   ├─ audio/playback existente
   ├─ video / visuales
   ├─ otros triggers
   └─ MIDI → AEYLA: LAUNCH SONG 06
```

AEYLA no exige que `CANCIÓN 06` esté en el sexto track, en una posición fija de Arrangement ni en un orden particular de escenas. La identidad del clip se resuelve por comando/ID MIDI.

Para sesiones donde ya existe un trigger maestro por canción, `LAUNCH SONG n` es el camino recomendado. Para operación manual o ensayo también deben existir `SELECT`, `NEXT`, `PLAY`, `PAUSE/RESUME` y `STOP/RESET`.

## 11. RAM

Objetivos de arquitectura:

- cola de captura: 512 KiB fija;
- caché de lectura: 64 KiB fija;
- históricos: índice/metadata en disco, no vectores de frames;
- crecimiento de RAM por duración de Take: aproximadamente plano;
- cambiar de Song/Take no debe acumular payloads históricos.

La implementación heredada basada en `std::vector<DmxUniverse>` permanece sólo como compatibilidad temporal y debe salir del camino de producto antes de declarar Show Candidate.

## 12. Gates antes de prueba oficial

### P0

1. Captura de producto conectada al writer stream-to-disk.
2. Reemplazar el player sample-locked a timeline absoluta por cursor relativo accionado por MIDI.
3. integrar `sampleOffset` real de los eventos MIDI dentro del bloque de audio.
4. implementar ACTIVE/QUEUED + SELECT/NEXT/PLAY/PAUSE/RESUME/STOP + LAUNCH SONG n.
5. eliminación del cache de múltiples Takes completos por Song.
6. salida Art-Net física repetida con NIC seleccionada, nodo U1 y DMX real.
7. funcionamiento con editor gráfico cerrado.
8. campañas repetidas LAUNCH / PAUSE / RESUME / RETRIGGER / NEXT / save-reopen.
9. pérdida y recuperación de red/nodo sin crash.
10. fallo de almacenamiento visible y fail-closed.

### Resistencia

- sesión completa de 10 canciones;
- campañas repetidas REC / STOP / LOAD / CONSOLIDATE / LAUNCH;
- 50 min captura y playback con RAM plana;
- triggers MIDI simultáneos con otros elementos del show;
- 8 h de soak;
- medir RAM, CPU, threads, handles, errores TX y stale drops.

## 13. Límite de estado actual

Las primitivas de captura streamed y lectura file-backed ya pueden existir y pasar CI antes de estar conectadas a la interfaz principal. El motor sample-locked a posición absoluta del DAW construido en R05 se considera ahora **arquitectura transitoria reemplazada en la rama R06 por cursor relativo**; todavía falta conectar ese motor al flujo VST3 visible y a los eventos MIDI reales del host.

La aceptación final depende de integrar el cursor relativo MIDI-driven en VST3/AUv2, probarlo en REAPER/Ableton con la sesión real, verificar NIC física + nodo + luminarias y completar pruebas prolongadas.
