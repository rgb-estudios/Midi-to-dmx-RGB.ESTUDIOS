# ADR-004 — AEYLA es la consola del show, no el reproductor de audio

Estado: **LOCKED / vinculante**
Fecha: 2026-08-07

## Decisión

AEYLA Visual DMX es la **consola de programación y ejecución del show de iluminación/automatización**. No es, ni debe convertirse en, el reproductor de audio de las canciones en vivo.

El DAW anfitrión (Ableton Live, Logic Pro o REAPER) mantiene la responsabilidad exclusiva sobre playback de audio, stems, click, instrumentos, edición de audio y transporte musical.

AEYLA recibe del host únicamente la información necesaria para sincronizar y controlar el show: transporte, posición temporal, tempo/PPQ cuando esté disponible, MIDI y estados de host relevantes.

## Modelo de instancia

Un show de hasta 15 canciones utiliza **una sola instancia lógica de AEYLA** y un solo propietario de salida DMX/Art-Net.

Las 15 canciones son entidades internas del mismo `.aeylashow`; no corresponden a 15 instancias del plugin.

La instancia contiene:

- rig y patch;
- perfiles de luminaria;
- looks;
- cue stacks/scenes;
- hasta 15 canciones;
- timelines de cues;
- bindings MIDI;
- bindings de sincronía con el DAW;
- configuración de output;
- seguridad de Arm / Blackout / Panic;
- datos necesarios para exportar/importar el show.

## Audio

AEYLA **no debe cargar el audio para reproducirlo en vivo**.

No se requiere que `.aeylashow` incluya WAV, MP3, stems ni masters de canciones para poder ejecutar el show.

Si en el futuro se añade una waveform o referencia de audio para ayudar a programar visualmente, será estrictamente una ayuda de edición opcional y nunca una dependencia de ejecución ni una segunda cadena de playback.

## Sincronía

El show debe poder ejecutarse de dos formas compatibles:

1. **Host-timeline driven**: el DAW aporta posición/transport y AEYLA reconstruye determinísticamente la Cue correcta.
2. **Cue/MIDI driven**: MIDI dispara o captura Cues y AEYLA mantiene su propio estado de consola.

En ambos casos el audio sigue siendo responsabilidad del DAW.

AEYLA nunca debe requerir que el audio pase a través de su motor para poder sincronizar las luces.

## Flujo correcto

```text
AUDIO / STEMS / CLICK
        ↓
       DAW
        │
        ├── reproduce audio
        ├── transport / PPQ / posición
        └── MIDI
                 ↓
        AEYLA Visual DMX
                 ↓
        Show / Song / CueRuntime
                 ↓
              Looks
                 ↓
       Semantic lighting engine
                 ↓
              DMX 512
                 ↓
             Art-Net
                 ↓
           nodo / PAR LED
```

## Programación

El usuario debe poder:

1. abrir un `.aeylashow`;
2. elegir una de hasta 15 canciones internas;
3. programar Looks y Cues;
4. capturar/disparar Cues por MIDI;
5. sincronizar la canción seleccionada con el transporte del DAW;
6. ensayar Play / Stop / Seek / Loop;
7. cambiar de canción sin crear otra instancia;
8. ejecutar el show desde Show Mode;
9. guardar/exportar la programación completa independientemente del audio.

## Consecuencias de seguridad

- sólo una instancia/output owner puede transmitir Art-Net para el show;
- una segunda instancia no puede adquirir el mismo destino/universo;
- cambiar de canción no crea ni destruye el backend de red;
- Stop / Seek / Loop no dependen de que el audio sea procesado por AEYLA;
- offline render/bounce del DAW inhibe salida de red;
- restore/reopen siempre vuelve `DISARMED`;
- audio ausente no invalida un `.aeylashow` porque audio no forma parte de la ejecución de luces.

## No objetivos

Fuera de alcance del producto:

- reproducir canciones o stems;
- reemplazar Ableton/Logic/REAPER como playback engine;
- mezclar audio;
- procesar audio para FOH;
- exigir 15 instancias para 15 canciones;
- usar una instancia Art-Net por canción.

Cualquier implementación futura que contradiga estas reglas debe tratarse como regresión arquitectónica.
