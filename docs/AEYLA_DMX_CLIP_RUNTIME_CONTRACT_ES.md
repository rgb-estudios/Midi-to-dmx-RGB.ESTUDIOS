# AEYLA — Contrato de runtime DMX Clip

Estado: **R05 / preintegración de producto**  
Ámbito: captura Art-Net, persistencia, edición no destructiva, reproducción DAW y salida Art-Net de un universo.

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
CLIP CONSOLIDADO
        ↓
DAW
  timeline global / audio / MIDI / triggers / video / otros plugins
        ↓
AEYLA PLAYER
  posición absoluta de muestras del host
        ↓
ART-NET TX
  interfaz física elegida
        ↓
NODO U1 → DMX
```

## 2. Regla principal de tiempo

La reproducción artística **no avanza desde reloj de pared**.

Para cada posición válida del host:

```text
elapsedSamples = hostSample - clipStartSample
frame = floor(elapsedSamples * dmxFps / sampleRate)
```

Consecuencias obligatorias:

- PLAY reconstruye desde la posición absoluta del DAW.
- SEEK hacia adelante o atrás reconstruye el frame correspondiente.
- LOOP no acumula error.
- STOP conserva el último frame válido (HOLD).
- cambios de tempo no alteran la duración del clip DMX ya capturado.
- cerrar la interfaz gráfica no puede detener el runtime.
- render offline inhibe siempre la salida física.

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
- seek/loop no requiere cargar el archivo completo.
- I/O de archivo sólo en thread no realtime.

## 5. Autoridad de salida

La salida Art-Net tiene dos autoridades mutuamente priorizadas:

1. salida semántica/modelo;
2. reproducción de DMX Clip.

Mientras el DMX Clip está armado, su autoridad override gana.

Estados mínimos:

- `DISARMED`: sin autoridad física de clip.
- `ARMED / WAITING`: preparado, aún sin trigger válido.
- `PLAYING`: host corriendo dentro del clip.
- `HOLD`: host detenido o clip terminado; conserva último frame válido.
- `OFFLINE`: salida física inhibida.
- `FAULT`: error de archivo, host o storage; fail-closed.

## 6. Heartbeat del host

El reloj de pared sólo se permite como vigilancia de vida.

- cada publicación coherente del host actualiza el heartbeat;
- si el callback deja de publicar por el umbral de seguridad, AEYLA deshabilita autoridad física;
- el heartbeat jamás calcula qué frame artístico corresponde.

## 7. Editor DMX

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

El RAW permanece inmutable.

### Prohibición inicial

No interpolar linealmente los 512 canales para crear fades genéricos.

Canales de strobe, modo, macro o funciones discretas podrían atravesar valores inválidos. Las transiciones continuas sólo podrán habilitarse después de clasificar canales/atributos seguros.

## 8. Triggers del DAW

El DAW conserva autoridad global del show.

Un evento MIDI recibido dentro de un bloque debe convertirse a ancla absoluta:

```text
clipStartSample = projectBlockStartSample + event.sampleOffset
```

AEYLA puede disparar/seleccionar clips DMX, pero no debe intentar reemplazar la timeline global de Ableton/Reaper/Logic.

Video, audio, otros plugins y automatizaciones paralelas siguen siendo responsabilidad del DAW.

## 9. RAM

Objetivos de arquitectura:

- cola de captura: 512 KiB fija;
- caché de lectura: 64 KiB fija;
- históricos: índice/metadata en disco, no vectores de frames;
- crecimiento de RAM por duración de Take: aproximadamente plano;
- cambiar de Song/Take no debe acumular payloads históricos.

La implementación heredada basada en `std::vector<DmxUniverse>` permanece sólo como compatibilidad temporal y debe salir del camino de producto antes de declarar Show Candidate.

## 10. Gates antes de prueba oficial

### P0

1. Captura de producto conectada al writer stream-to-disk.
2. Player de producto conectado al runtime sample-locked/file-backed.
3. eliminación del cache de múltiples Takes completos por Song.
4. salida Art-Net física repetida con NIC seleccionada, nodo U1 y DMX real.
5. funcionamiento con editor gráfico cerrado.
6. STOP / SEEK / LOOP / backward seek / save-reopen.
7. pérdida y recuperación de red/nodo sin crash.
8. fallo de almacenamiento visible y fail-closed.

### Resistencia

- sesión completa de 10 canciones;
- campañas repetidas REC / STOP / LOAD / PLAY;
- 50 min captura y playback con RAM plana;
- 8 h de soak;
- medir RAM, CPU, threads, handles, errores TX y stale drops.

## 11. Límite de estado actual

Las primitivas de captura streamed, lectura file-backed y proyección sample-locked pueden existir y pasar CI antes de estar conectadas a la interfaz principal. **Eso no convierte por sí solo el plugin en Show Ready.**

La aceptación final depende de integración real VST3/AUv2 + hosts + NIC física + nodo + luminarias y pruebas prolongadas.
