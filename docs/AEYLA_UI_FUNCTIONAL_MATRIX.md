# AEYLA — matriz funcional de UI

Checkpoint: `CP-AEYLA-0.3.2`
Regla: visible = funcional, bloqueado con causa o no presente.

| Superficie | Evento → handler → efecto | Persistencia | Estado actual |
|---|---|---:|---|
| NEW | click → `NewProjectFromUI` → bundle nuevo seguro | sí | **Implemented** en SHA previo; revalidación del producto pendiente |
| OPEN | click → diálogo → carga transaccional | sí | **Implemented** en SHA previo; revalidación pendiente |
| SAVE / SAVE AS | click → ZIP atómico + read-back + `.bak` | sí | **Implemented** en SHA previo; revalidación pendiente |
| BLACKOUT | click → parámetro → modelo → DMX cero | host/show | **Implemented**; cambio de runtime en revalidación |
| ARM | click → gates → arma o explica proyecto/backend/runtime/offline | ARM nunca persiste | **Scaffolded**; backend físico no conectado |
| RIG 10/14 | click → fixtures físicos habilitados | sí | **Implemented** en modelo |
| Source | click → selecciona Look base y carga sus valores propios | sí | **Scaffolded**; producto pendiente de CI |
| Paleta primaria/secundaria | click → color del Look → preview/DMX semántico | sí | **Scaffolded**; 8 colores, sin selector continuo |
| Look Intensity | drag → intensidad propia del Look | sí | **Scaffolded** |
| Speed / W / A / UV | drag → valores propios del Look | sí | **Scaffolded** |
| Grand Master | drag → intensidad global de operador | host state | **Implemented**; separado de Look Intensity |
| Fixtures | click → selección; botón include/exclude → máscara del Look | sí | **Scaffolded**; sin grupos |
| STORE LOOK | click → duplica estado artístico completo con ID/nombre seguro | sí | **Scaffolded**; nombre automático, sin rename/delete |
| Look anterior/siguiente | click → recupera cualquier Look almacenado | sí | **Scaffolded**; máximo acotado a 2048 |
| NEW SONG | click → crea y selecciona Song editable, máximo 15 | sí | **Scaffolded** |
| Song anterior/siguiente | click → selección segura + blackout/disarm | sesión | **Scaffolded**; sin reorder/rename/delete |
| SET SONG START | click → `Song ID → host PPQ` | estado del plugin | **Scaffolded**; no asume PPQ cero |
| STORE CUE @ PLAYHEAD | click → Cue LATCH + placement; extiende Song si hace falta | sí | **Scaffolded**; sin edición/move/delete |
| MIDI mapping | Cue recibe primer note/channel libre, oculto | sí | **Scaffolded**; UI MIDI Learn manual no presente |
| Executors | mouse/MIDI → Cue o diagnóstico sin Show | runtime | **Implemented** en modelo; revalidación de host pendiente |
| Runtime con editor cerrado | worker 4 ms independiente de `OnIdle` | no | **Scaffolded**; falta prueba real en hosts |
| Offline render | hard disarm + blackout cada tick offline | no | **Scaffolded**; falta prueba de host y backend conectado |
| OUTPUT PREFLIGHT | no presente | config | **Specified** |
| SHOW MODE | no presente | UI state | **Specified** |

## Límites visibles de esta entrega

- No hay selector continuo de color, rename/delete/reorder, undo/redo ni grilla
  de timeline.
- La Cue creada desde UI es LATCH; el modelo soporta MOMENTARY pero todavía no
  hay selector visible.
- Art-Net físico sigue sin estar conectado al producto.
- Nada en esta matriz autoriza uso de show sin CI actual, hosts reales,
  hardware, soak y ensayo.
