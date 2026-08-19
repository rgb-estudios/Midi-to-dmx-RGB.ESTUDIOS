# AEYLA — flujo operativo disponible en CP-AEYLA-0.3.3

Estado: **Scaffolded** para programación controlada; no es Show Candidate.
Alcance: una instancia, hasta 15 Songs, Rig 10/14, preview visual/DMX interno.

## Idea simple

El DAW reproduce el audio. AEYLA programa luces como
`Look → Cue → Song → Show`. Las notas MIDI son un detalle interno de transporte
y control: no se necesitan para crear un Cue desde la UI.

## Crear una programación mínima

1. Inserta una sola instancia en una pista `AEYLA CONTROL`.
2. Pulsa `NEW` o abre un `.aeylashow`.
3. Elige `RIG 10` o `RIG 14` y una fuente visual.
4. Edita color primario/secundario, Look Intensity, Speed y extracción W/A/UV.
5. Selecciona cada fixture requerido y usa `FIXTURE INCLUDED/EXCLUDED` para la
   máscara del Look.
6. Pulsa `STORE LOOK`. En este checkpoint el nombre se asigna automáticamente.
7. Usa los `<` y `>` del selector LOOK para recuperar Looks guardados.
8. Pulsa `NEW SONG`. Usa los `<` y `>` del selector SONG para moverte entre
   Songs existentes.
9. Coloca el playhead del DAW exactamente en el comienzo musical de esa Song y
   pulsa `SET SONG START`.
10. Mueve el playhead al momento deseado y pulsa `STORE CUE @ PLAYHEAD`.
   El Song se extiende automáticamente si el playhead está más allá de su final
   inicial.
11. Repite los cambios de Look y `STORE CUE` necesarios. Guarda con `SAVE AS`.
12. Para una prueba de red controlada, pulsa `OUTPUT SETUP` e introduce la IP
    numérica del nodo y el universo como `IPv4@universo`, por ejemplo
    `2.0.0.20@0`. Escribe `OFF` para deshabilitar el backend.
13. Confirma `BACKEND READY`, quita Blackout y pulsa `ARM OUTPUT`. La
    configuración nunca rearma automáticamente la salida.

Cada cambio artístico/estructural o selección de Look/Song fuerza
`DISARMED + BLACKOUT`. El
operador debe salir de Blackout y solicitar ARM otra vez de forma explícita.

## Playback y reconstrucción

- Play/Seek/Loop se proyectan desde PPQ absoluto del host al tick relativo del
  Song seleccionado.
- Un Song sin `SET SONG START` no supone PPQ cero: resuelve a estado seguro.
- Stop libera overrides transitorios.
- El runtime musical continúa en un worker propio aunque el editor no reciba
  `OnIdle`; esta propiedad todavía requiere evidencia de host real.
- Un bounce/render offline fuerza disarm y blackout y nunca se rearma solo.

## Lo que aún no debe intentarse como show

- Art-Net está conectado en software con preflight de configuración/socket y
  fail-closed ante error de envío, pero no hay watchdog de recepción del nodo
  ni hardware validado.
- No hay Show Mode, edición de Cue, MOMENTARY visible, rename/delete/reorder,
  undo/redo ni timeline gráfica.
- Windows standalone mantiene el P0 de OpenGL nulo; REAPER/Ableton/Logic y el
  cierre de ventana deben revalidarse en máquinas/hosts reales.

Por lo tanto este flujo sirve para programación y prueba Art-Net controlada en
laboratorio, no para operar una función.
