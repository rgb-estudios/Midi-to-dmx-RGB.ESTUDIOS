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
| NUEVO | crear proyecto AEYLA nuevo | funcional; confirma descarte si hay cambios sin guardar |
| ABRIR | abrir `.aeylashow` | funcional; diálogo nativo y validación transaccional |
| GUARDAR | guardar proyecto actual | funcional; si no existe ruta deriva a Guardar como |
| GUARDAR COMO | guardar nueva ruta | funcional; fuerza extensión `.aeylashow` |
| ARMAR SALIDA | habilitar salida física | nunca silencioso: arma o muestra causa exacta del bloqueo |
| APAGÓN | alternar apagón | funcional y con prioridad de seguridad |
| TOMA / EDICIÓN | abrir el flujo de captura y edición | pestaña funcional; no comparte controles de red en un panel estrecho |
| RED / SALIDA | abrir routing, IPv4 y telemetría | pestaña funcional; conserva ARMAR y APAGÓN siempre visibles |
| ADAPTADOR RX / TX | seleccionar NIC física | funcional; cambiar TX desarma la salida y nunca elige Wi-Fi silenciosamente |
| IPv4 AEYLA / MÁSCARA | editar la red local de show | campo real; valida IPv4, máscara contigua y dirección de host utilizable |
| APLICAR IP Y PREPARAR ART-NET | configurar la NIC TX | Windows: solicita UAC sólo para el helper, conserva la red existente, valida/rollback y termina desarmado |
| TIMELINE DMX | posicionar el cabezal | clic/arrastre libre; scrub local sólo con salida física desarmada |
| HANDLE IN / OUT | delimitar el rango no destructivo | grip visible y arrastrable; no depende de botones por segundos |
| MARCAR IN / OUT EN CABEZAL | fijar un punto exacto | usa la posición actual del cabezal con precisión de un cuadro DMX |
| TIEMPO IN / OUT | introducir un punto numérico | campo editable `MM:SS.mmm` o segundos; cuantiza al cuadro DMX más cercano |
| AJUSTE ±1f | corregir un punto | mueve un cuadro DMX, aproximadamente 22,7 ms a 44 Hz |
| ZOOM / PAN | navegar una toma extensa | rueda o controles `-/+`; `Shift` + arrastre para paneo horizontal |
| CONSOLIDAR CLIP | materializar el rango ENTRADA/SALIDA | funcional; crea otro `.aeylatake`, conserva el RAW y deja el clip nuevo listo para reproducir |
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

1. NUEVO / ABRIR / GUARDAR / GUARDAR COMO;
2. APAGÓN;
3. RIG 10/14;
4. cada fuente visual;
5. selección de fixtures 1, 7, 8 y 14;
6. cada slider a mínimo/medio/máximo;
7. cada executor por mouse;
8. entrada MIDI física;
9. ARMAR bloqueado con causa visible mientras backend esté desactivado;
10. abrir/cerrar UI repetidamente sin perder interacción.
11. mover libremente el cabezal y marcar IN/OUT desde posiciones no redondas;
12. arrastrar ambos handles, editar ambos timecodes y ajustar ±1 cuadro;
13. usar zoom + paneo y comprobar que los puntos conservan su posición;
14. consolidar y comprobar que el RAW no cambia.
15. cambiar a `RED / SALIDA`, seleccionar Ethernet y comprobar feedback legible;
16. aplicar una IPv4/máscara real, confirmar UAC y verificar que la red previa se conserva;
17. ejecutar `APAGÓN OFF → ARMAR → REPRODUCIR` y observar TX creciente a 44 Hz;
18. minimizar/cerrar la UI durante 5 minutos y verificar continuidad con un receptor externo.

Un screenshot correcto no constituye PASS de interacción. Se requiere click/input real en host.
