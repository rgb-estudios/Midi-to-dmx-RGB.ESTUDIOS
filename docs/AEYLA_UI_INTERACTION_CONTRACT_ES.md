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
| GRABAR NUEVA TOMA | iniciar/finalizar captura | exige RX con señal, salida desarmada y canción; escribe directamente a disco |
| REPRODUCIR / DETENER | operar la toma activa | cursor relativo; detener conserva el último cuadro válido |
| HANDLE ENTRADA / SALIDA | delimitar el rango no destructivo | grip visible y arrastrable; no depende de botones por segundos |
| MARCAR ENTRADA / SALIDA EN CABEZAL | fijar un punto exacto | usa la posición actual del cabezal con precisión de un cuadro DMX |
| TIEMPO ENTRADA / SALIDA | introducir un punto numérico | campo editable `MM:SS.mmm` o segundos; cuantiza al cuadro DMX más cercano |
| AJUSTE ±1f | corregir un punto | mueve un cuadro DMX, aproximadamente 22,7 ms a 44 Hz |
| ZOOM / PAN | navegar una toma extensa | rueda o controles `-/+`; `Shift` + arrastre para paneo horizontal |
| CONSOLIDAR MUESTRA DMX | materializar el rango ENTRADA/SALIDA | funcional; crea otro `.aeylatake`, conserva la TOMA BRUTA y deja la nueva versión preparada |
| Versiones / TOMA BRUTA | cambiar fuente de edición | funcional; nunca altera ni reemplaza la grabación original |
| Telemetría RX/TX/AUTORIDAD | inspeccionar salud | tres estados separados; texto y color, nunca sólo color |

## Hit testing / overlays

- `AeylaRuntimeStatusControl` puede dibujar sobre toda la ventana, pero sólo puede poseer mouse en el footer.
- El resto del editor debe atravesar hacia `AeylaMainControl`.
- No se adjuntan overlays invisibles ni controles heredados con rectángulo cero.
- Ningún overlay full-window puede volver a usar el hit test por defecto.

## Estados bloqueados

ARM debe bloquearse con feedback explícito cuando corresponda:

- proyecto inválido;
- backend físico no listo;
- APAGÓN activo;
- captura en curso;
- ausencia de toma activa;
- runtime no saludable o renderizado sin conexión;
- cualquier gate de seguridad posterior.

Un motor deliberadamente desconectado se representa antes del clic como
`BLOQUEADA · RED`. El clic conserva feedback detallado y accionable.

Mientras GRABAR o su escritor de disco sigan activos, NUEVO, ABRIR, crear,
renombrar o cambiar canción y cualquier actualización RX/TX deben quedar
bloqueados con causa. Una transacción IPv4/UAC bloquea también las flechas y
la actualización de adaptadores hasta que el runtime confirme su estado final.

## Validación mínima en host real

Cada PRETEST que cambie UI debe comprobar manualmente en REAPER/Windows:

1. NUEVO / ABRIR / GUARDAR / GUARDAR COMO;
2. APAGÓN;
3. crear/seleccionar/renombrar canción y respetar el límite de 15;
4. seleccionar RX/TX sin perder legibilidad a 960×620;
5. comprobar bloqueo previo de ARMAR por red, apagón, grabación y sin toma;
6. grabar antes de iniciar transporte y observar ENTRADA AUTO;
7. abrir/cerrar UI repetidamente sin perder interacción;
8. mover libremente el cabezal y marcar ENTRADA/SALIDA desde posiciones no redondas;
9. arrastrar ambos handles, editar timecodes y ajustar ±1 cuadro;
10. usar ampliación + desplazamiento y comprobar que los puntos conservan su posición;
11. consolidar y comprobar que la TOMA BRUTA no cambia;
12. aplicar una IPv4/máscara real, confirmar UAC y verificar que la red previa se conserva;
13. ejecutar `APAGÓN OFF → ARMAR → REPRODUCIR` y observar TX creciente a 44 Hz;
14. minimizar/cerrar la UI durante 5 minutos y verificar continuidad con un receptor externo;
15. simular fail-closed, confirmar que no hay rearme automático y recuperar con APAGÓN OFF → ARMAR.

Un screenshot correcto no constituye PASS de interacción. Se requiere click/input real en host.
