# Prueba de host — AEYLA Visual DMX VST3 Alpha

Versión: `0.1.0-alpha.1`

Estado: **Alpha / no apto para show**.

Esta prueba confirma el camino VST3 real antes de integrar Art-Net, USB-DMX, video y la interfaz visual definitiva.

## Qué contiene

- instrumento/generador VST3 silencioso;
- entrada de eventos MIDI;
- salida estéreo siempre en cero;
- parámetros `Output Arm Request`, `Blackout`, `Grand Master`, `MIDI Activity` y `Dropped Events`;
- persistencia segura del estado;
- `Output Arm` nunca se restaura armado;
- sin red, USB, archivos ni video dentro de `process()`.

## Windows

1. Cerrar Ableton Live.
2. Copiar `AeylaVisualDmxAlpha.vst3` a `C:\Program Files\Common Files\VST3\`.
3. Abrir Ableton y habilitar `Use VST3 Plug-In System Folders`.
4. Mantener `Alt` y pulsar `Rescan`.
5. Buscar `RGB Estudios > AEYLA Visual DMX Alpha`.
6. Insertar en una pista MIDI y reproducir notas.

## macOS

1. Cerrar Ableton Live.
2. Copiar el bundle a `~/Library/Audio/Plug-Ins/VST3/`.
3. Habilitar la carpeta VST3 del sistema.
4. Mantener `Option` y pulsar `Rescan`.
5. Buscar `RGB Estudios > AEYLA Visual DMX Alpha`.

El artifact macOS de CI es universal `arm64 + x86_64`, pero todavía no está notarizado con un Developer ID de producción. No desactivar Gatekeeper para esta prueba; registrar el mensaje exacto si macOS lo bloquea.

## Resultado esperado

- Ableton encuentra el plugin;
- se inserta sin crash;
- permanece completamente silencioso;
- las notas MIDI llegan al procesador;
- `Dropped Events` permanece en cero;
- `Blackout` inicia activo;
- guardar/reabrir el Set no restaura `Output Arm`.

## Resultado que no debe esperarse todavía

- salida DMX;
- movimiento de luces;
- editor visual personalizado;
- videos o gradientes;
- escenas o executors completos;
- uso en ensayo o espectáculo.

## Evidencia mínima

Registrar:

- modelo/OS del equipo;
- versión exacta de Ableton;
- ruta de instalación;
- resultado del scan;
- carga en pista MIDI;
- reproducción de notas;
- save/reopen;
- mensaje exacto y captura si falla.
