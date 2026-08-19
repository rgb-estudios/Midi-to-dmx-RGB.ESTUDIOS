# AEYLA Visual DMX — Contrato de interacción UI

Estado: vinculante para Alpha 0.3 y posteriores.

## Regla central

Todo elemento que visualmente parezca interactivo debe estar exactamente en uno de estos estados:

1. **FUNCIONAL** — ejecuta una acción real y el resultado es observable;
2. **BLOQUEADO** — no puede ejecutarse por una condición real y muestra la causa de forma visible/accionable;
3. **NO PRESENTE** — una función aún no implementada no se dibuja como si ya existiera.

No se aceptan botones, menús, sliders, pads o selectores "muertos" que parezcan disponibles pero no tengan efecto real.

## Interacciones Alpha 0.3

| Superficie | Acción | Contrato |
|---|---|---|
| NEW | crear proyecto AEYLA nuevo | funcional; confirma descarte si hay cambios sin guardar |
| OPEN | abrir `.aeylashow` | funcional; diálogo nativo y validación transaccional |
| SAVE | guardar proyecto actual | funcional; si no existe ruta deriva a Save As |
| SAVE AS | guardar nueva ruta | funcional; fuerza extensión `.aeylashow` |
| ARM OUTPUT | habilitar salida física | nunca silencioso: arma o muestra causa exacta del bloqueo |
| BLACKOUT | alternar blackout | funcional y con prioridad de seguridad |
| RIG 10/14 | cambiar fixtures habilitados | funcional y persistente |
| SOLID / GRADIENT / WAVE / NOISE / CHASE | seleccionar fuente de preview/look | funcional |
| Fixture 1–14 | selección visual actual | funcional como selección/inspección; no debe presentarse como Programmer por-fixture hasta estar conectado |
| Grand Master | control global | funcional |
| Animation Speed | velocidad de preview/efecto | funcional; debe migrar a tiempo derivado del host para show determinista |
| White / Amber / UV | parámetros de preview/look | funcional |
| Executor 1–8 | diagnóstico MIDI/runtime Alpha | funcional momentáneo; no es el lenguaje creativo final |

## Hit testing / overlays

- `AeylaRuntimeStatusControl` puede dibujar sobre toda la ventana, pero sólo puede poseer mouse en el footer y el botón ARM.
- El resto del editor debe atravesar hacia `AeylaMainControl`.
- `AeylaExecutorRuntimeControl` posee el área de ejecutores y es la única verdad visible/operativa de esos pads.
- Ningún overlay full-window puede volver a usar el hit test por defecto.

## Estados bloqueados

ARM debe bloquearse con feedback explícito cuando corresponda:

- proyecto inválido;
- backend físico no listo;
- show/cue program no performance-ready;
- cualquier gate de seguridad posterior.

Un backend deliberadamente desconectado no se representa como fallo de click: se representa como `ARM LOCKED · BACKEND`.

## Frontera del Programmer

La selección actual de fixtures sirve para inspección y preparación del flujo. Hasta que `Programmer` esté conectado a `ApplicationModel`, los controles globales no deben insinuar que editan únicamente el fixture seleccionado.

Cuando el Programmer se integre, el contrato será:

`fixture/group selection -> semantic attributes -> programmer overlay -> STORE LOOK -> STORE CUE @ PLAYHEAD`.

## Validación mínima en host real

Cada PRETEST que cambie UI debe comprobar manualmente en REAPER/Windows:

1. NEW / OPEN / SAVE / SAVE AS;
2. BLACKOUT;
3. RIG 10/14;
4. cada fuente visual;
5. selección de fixtures 1, 7, 8 y 14;
6. cada slider a mínimo/medio/máximo;
7. cada executor por mouse;
8. entrada MIDI física;
9. ARM bloqueado con causa visible mientras backend esté OFF;
10. abrir/cerrar UI repetidamente sin perder interacción.

Un screenshot correcto no constituye PASS de interacción. Se requiere click/input real en host.
