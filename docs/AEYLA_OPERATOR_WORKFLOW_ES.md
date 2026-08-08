# AEYLA — flujo operativo objetivo

Estado: **DESIGN LOCK / implementación parcial**  
Versión objetivo: `0.3.x`  
Alcance v1: 1 instancia, hasta 15 Songs, Rig 10/14, 1 universo Art-Net.

## La idea simple

El DAW reproduce el audio. AEYLA agrega la secuencia de luces sobre la misma
línea de tiempo. El operador piensa en `Look → Cue → Song → Show`; las notas
MIDI quedan ocultas salvo diagnóstico o MIDI Learn.

## Preparación de la sesión

1. Crear una pista única llamada `AEYLA CONTROL`.
2. Insertar una única instancia VST3/AUv2.
3. Crear o abrir el `.aeylashow` del show completo.
4. Crear entre 1 y 15 Songs dentro de esa instancia.
5. Para cada Song, ubicar el playhead del DAW en su inicio y ejecutar
   `SET START FROM PLAYHEAD`.
6. La referencia de audio/waveform es opcional y solo ayuda a programar; el
   playback real nunca sale desde AEYLA.

## Programación de una canción

1. Seleccionar la Song.
2. Seleccionar fixtures o grupo.
3. Diseñar el estado visual en Programmer: intensidad, color, fuente/efecto,
   velocidad y extracción RGBW correspondiente.
4. Ejecutar `STORE LOOK` y asignar un nombre musical, por ejemplo `INTRO RED`.
5. Ejecutar `STORE CUE @ PLAYHEAD` para ubicar la Cue en la timeline interna.
6. Elegir comportamiento:
   - `LATCH`: permanece hasta otra Cue.
   - `MOMENTARY`: override mientras dura el golpe/bump.
7. Opcionalmente seleccionar la Cue, activar `MIDI LEARN` y tocar un pad/tecla.
8. Repetir Play, Stop, Seek y Loop desde distintos puntos. La misma posición
   debe reconstruir siempre el mismo Look/DMX.

## Ensayo y show

1. Validar todas las Songs y bindings.
2. Configurar adaptador, IP del nodo, universo y FPS Art-Net.
3. Ejecutar preflight de red y prueba controlada de output.
4. Confirmar arranque `DISARMED + BLACKOUT`.
5. Entrar a Show Mode, donde la edición peligrosa queda bloqueada.
6. ARM solo queda disponible si proyecto, Show, backend, red y condiciones de
   seguridad pasan.

## Estado del build actual

El HEAD `ebce964` anterior a `CP-AEYLA-0.3.1` todavía no ofrece este flujo
completo en la UI. Guardar/abrir y controles de preview existen; Song Library,
Store Look, Store Cue, MIDI Learn, bindings de todas las Songs y Art-Net físico
siguen bloqueados. No usar el PRETEST actual como sistema de show.
