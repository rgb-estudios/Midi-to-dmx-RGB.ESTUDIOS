# AEYLA — matriz funcional de UI

Checkpoint: `CP-AEYLA-0.3.1`  
Regla: visible = funcional, bloqueado con causa o no presente.

| Superficie | Evento → handler → efecto | Persistencia | Estado |
|---|---|---:|---|
| NEW | click → `NewProjectFromUI` → bundle nuevo seguro | sí | IMPLEMENTED / REVALIDATE |
| OPEN | click → diálogo → carga transaccional | sí | IMPLEMENTED / REVALIDATE |
| SAVE / SAVE AS | click → package atómico + read-back + `.bak` | sí | IMPLEMENTED / REVALIDATE |
| BLACKOUT | click → parámetro → `ApplicationModel` → DMX cero | host/show | IMPLEMENTED / REVALIDATE |
| ARM | click → safety gates → arma o explica bloqueo | nunca ARM | IMPLEMENTED / REVALIDATE |
| RIG 10/14 | click → modelo → fixtures enabled | sí | IMPLEMENTED / REVALIDATE |
| Source | click → modelo semántico → preview/DMX | sí | IMPLEMENTED / REVALIDATE |
| Sliders | click/drag → parámetro → modelo | sí salvo Grand Master host-state | IMPLEMENTED / REVALIDATE |
| Fixtures | click → selección/inspector local | no | BLOCKED como Programmer |
| Executors | mouse/MIDI → HostEvent → Cue o diagnóstico | runtime | IMPLEMENTED / REVALIDATE |
| Song Library | no presente | — | DESIGN LOCK |
| STORE LOOK | no presente | — | BLOCKED: Look completo pendiente |
| STORE CUE @ PLAYHEAD | no presente | — | BLOCKED: authoring transaction pendiente |
| MIDI LEARN | no presente | — | BLOCKED: mapping aún vive en Clip |
| SET START FROM PLAYHEAD | no presente | host state | BLOCKED: show-level bindings pendiente |
| OUTPUT PREFLIGHT | no presente | config | NOT CONNECTED |
| SHOW MODE | no presente | UI state | DESIGN LOCK |

## Reparaciones incluidas

- Eliminado executor visual fantasma/duplicado.
- Eliminado ARM inferior duplicado.
- `Draw()` ya no escribe velocidad ni estado del modelo.
- Preview y DMX comparten fase derivada de PPQ; no usan reloj de pared.

## P0 restante

El runtime consume eventos desde `OnIdle`. Antes de conectar Art-Net hay que
probar explícitamente procesamiento con editor cerrado; si el host suspende
`OnIdle`, se requiere un worker de control independiente de la ventana.
