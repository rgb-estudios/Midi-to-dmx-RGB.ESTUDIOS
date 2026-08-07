# AEYLA Visual DMX — Contrato de flujo de trabajo

Estado: vinculante para el desarrollo posterior a `0.3.0-alpha.1`.

## 1. Principio central

La unidad creativa de AEYLA Visual DMX es una **Cue / Scene**, no una nota MIDI y no un canal DMX.

El usuario programa estados visuales con nombres, transiciones y comportamiento. MIDI es una capa de control y captura que debe permanecer mayormente invisible durante el trabajo creativo.

No se considera aceptable un flujo principal que obligue al usuario a recordar números de nota, dibujar manualmente cada evento en un piano-roll o razonar en canales DMX para construir una canción.

## 2. Responsabilidades

### El DAW

Ableton Live, Logic Pro o REAPER debe aportar:

- audio/playback;
- transporte y posición temporal;
- entrada MIDI del controlador físico;
- opcionalmente una pista MIDI de control portable como fallback/intercambio.

El DAW **no** es el editor de luminarias.

### AEYLA Visual DMX

El plugin debe aportar:

- Rig 10 / Rig 14;
- perfiles y patch DMX;
- configuración de salida;
- Looks;
- Scenes / Cues;
- transiciones;
- timeline de la canción;
- MIDI Learn / captura;
- preview;
- compilación determinista de un universo DMX;
- salida Art-Net;
- seguridad de Arm / Blackout / Panic.

### `.aeylashow`

El archivo de show debe ser la fuente portable de:

- rig y patch;
- perfiles;
- looks;
- escenas;
- canciones;
- timeline de cues;
- transiciones;
- mappings MIDI;
- configuración de output no peligrosa;
- identidad y versión del proyecto.

Output Arm nunca debe restaurarse automáticamente.

## 3. Flujo de usuario objetivo

### A. SETUP

1. Crear/abrir show.
2. Elegir Rig 10 o Rig 14.
3. Elegir perfil de PAR y patch.
4. Configurar Art-Net: interfaz de red, IP del nodo, universo y FPS.
5. Ejecutar `TEST OUTPUT` con el show desarmado para verificar conectividad de forma controlada.

### B. PROGRAM

1. Crear una canción.
2. Reproducir el audio desde el DAW.
3. Diseñar Looks/Scenes desde la interfaz gráfica.
4. Guardar cada estado como Cue con nombre.
5. Definir transición de entrada/salida y comportamiento.
6. Ubicar/capturar las Cues sobre una timeline visual de canción.

La timeline debe mostrar bloques con nombres como `INTRO RED`, `COLD WAVE`, `WHITE HIT`, no notas MIDI sin contexto.

### C. CAPTURE / MIDI LEARN

El controlador MIDI es una superficie de disparo y grabación rápida.

- El usuario selecciona una Cue y pulsa `MIDI LEARN`.
- Toca una tecla/pad.
- AEYLA recuerda la nota/canal internamente.
- Durante reproducción, el usuario puede disparar Cues en tiempo real para capturar su posición contra el transporte del DAW.
- El número MIDI puede mostrarse como información técnica secundaria, nunca como nombre principal de la Cue.

### D. REHEARSE

1. Reproducir canción desde distintas posiciones.
2. Hacer Stop / Play / Seek / Loop.
3. AEYLA reconstruye el estado correcto desde la timeline, sin depender del historial previo de Note On.
4. Comparar preview y salida DMX.
5. Corregir cues/transiciones en Program Mode.

### E. SHOW

Show Mode bloquea edición peligrosa y muestra sólo:

- canción actual;
- cue actual / siguiente;
- estado MIDI;
- estado Art-Net;
- nodo/universo;
- Arm;
- Blackout;
- Panic;
- warnings críticos.

## 4. Tipos de evento

### LATCH SCENE

`Note On` selecciona una escena persistente.

`Note Off` no apaga la escena.

La escena permanece hasta una nueva Cue o Blackout.

Uso: cambios de look, color, ambiente, sección musical.

### MOMENTARY / BUMP

`Note On` aplica un override mientras la nota/pad está presionado.

`Note Off` libera el override y vuelve al estado persistente anterior.

Uso: strobe, white hit, UV hit, acento, flash.

### CONTINUOUS

MIDI CC o automatización puede controlar valores continuos cuando corresponda.

Uso potencial: Grand Master, speed, amount.

Los controles de seguridad críticos no deben depender de una nota MIDI ordinaria.

## 5. Pipeline de datos

```text
MIDI keyboard / DAW MIDI item
        ↓
DAW MIDI input + transport
        ↓
AEYLA VST3/AU wrapper
        ↓
HostEvent ingress
        ↓
Cue / Scene resolver
        ↓
Semantic lighting engine
        ↓
Fixture profiles + patch
        ↓
512-byte DMX desired frame
        ↓
Latest-frame output worker @ fixed FPS
        ↓
Art-Net ArtDMX (prefer unicast to configured node)
        ↓
Ethernet NIC
        ↓
Art-Net node
        ↓
physical DMX output
        ↓
PAR fixtures
```

No se realizan llamadas de red desde el callback de audio.

El worker de salida debe transmitir el **último frame deseado**, no una cola atrasada de estados MIDI intermedios.

## 6. Portabilidad entre DAWs

La programación visual vive en `.aeylashow`, no en una implementación específica de REAPER/Ableton/Logic.

El timeline interno sincronizado al host es la representación principal.

Una pista MIDI `AEYLA CONTROL` puede existir como:

- modo de captura;
- fallback;
- export/import portable;
- evidencia de timing.

Pero el producto no debe depender de que el usuario dibuje manualmente el show completo en el piano-roll.

## 7. Contrato de seguridad de output

- Arranque: `DISARMED + BLACKOUT`.
- Proyecto inválido: no output real.
- Backend no saludable: no Arm.
- Restore/reopen: vuelve `DISARMED`.
- Offline render/bounce: output de red inhibido.
- Stop/seek/reload: estado reconstruido determinísticamente.
- Blackout/Panic tienen precedencia sobre cues y MIDI.

## 8. Estado transitorio de Alpha 0.3

La implementación actual **no representa todavía este flujo final**.

Actualmente:

- sólo las notas MIDI 36–43 disparan 8 ejecutores;
- los 8 ejecutores son momentáneos;
- la duración de ciertos clips está modelada como longitud de nota;
- la UI de canción/cue/timeline todavía no constituye el flujo principal;
- el wrapper recibe Note On/Off, pero el bridge completo de transporte del DAW sigue pendiente;
- el modelo genera un frame DMX de 512 bytes, pero el backend Art-Net del producto integrado todavía no está conectado;
- por tanto `BACKEND OFF / SIMULATED / NO DMX` es el estado correcto de Alpha 0.3.

Estos puntos son deuda de implementación y no deben reinterpretarse como decisiones de UX definitivas.

## 9. Próximas prioridades derivadas de este contrato

1. Reparar interacción completa de UI en host.
2. Verificar MIDI físico → REAPER → plugin con indicador visible.
3. Separar escenas persistentes de ejecutores momentáneos.
4. Crear editor de Cue/Scene con `STORE CUE` y `MIDI LEARN`.
5. Conectar transporte/posición del host.
6. Crear timeline de cues por canción con captura contra transporte.
7. Implementar worker Art-Net latest-frame-only y configuración de red.
8. Añadir output test/preflight y seguridad de backend.
9. Probar nodo físico + PAR real.
10. Validar save/reopen/seek/loop y portabilidad entre hosts.
