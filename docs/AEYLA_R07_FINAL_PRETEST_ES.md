# AEYLA Visual DMX — R07 FINAL PRETEST

Fecha de congelación de procedimiento: 29 de agosto de 2026.

## 1. Estado de esta entrega

R07 FINAL PRETEST es la última candidata previa a declarar **SHOW CANDIDATE**. La compilación, tests de núcleo, validación de formatos, empaquetado e integridad se automatizan en CI. La promoción a SHOW CANDIDATE requiere además una prueba física en un Windows interactivo con REAPER y un receptor Art-Net externo.

No se debe interpretar PRETEST como autorización automática para usar una compilación nueva por primera vez en escenario.

## 2. Arquitectura de sincronía congelada

La ruta de show es:

`DAW (REAPER/Ableton) -> audio + MIDI SHOW sample-accurate -> AEYLA`

En paralelo, el mismo DAW puede enviar:

`DAW -> MTC -> Avolites`

AEYLA no recircula ni decodifica MTC para gobernar la toma DMX. La nota MIDI SHOW y el audio comparten el reloj de muestras del host, evitando introducir cuantización quarter-frame y otro punto de fallo de ruteo.

La convención de una hora MTC por canción puede mantenerse para Avolites y para organizar la sesión.

## 3. Resolución física real

La captura/reproducción DMX de R07 trabaja nominalmente a 44 cuadros por segundo. Un cuadro DMX representa aproximadamente 22,7 ms. El disparo MIDI se sella con el `sampleOffset` del bloque del DAW, pero la observación física de la toma sigue limitada por la cadencia DMX.

No se debe prometer precisión física inferior a un cuadro DMX.

## 4. Mapa MIDI SHOW R07

MIDI SHOW está desactivado por defecto y usa canal 16 por defecto. El canal puede cambiarse desde la interfaz.

Mapa inicial:

- Nota 36: canción anterior.
- Nota 37: siguiente canción.
- Nota 38: PLAY / reiniciar desde cero.
- Nota 39: pausa / reanudar.
- Nota 40: STOP / RESET a cero.
- **Nota 41: PANIC / APAGÓN — reservada, unidireccional y no aprendible en R07.**
- Notas 48–62: lanzamiento directo de canciones 01–15.

Los comandos configurables admiten APRENDER MIDI. La nota 41 se mantiene fija en R07 para no modificar el formato persistente del estado VST3 durante el cierre de entrega. Nuevas asignaciones no pueden usar N41 ni crear un banco de canciones que la atraviese.

Compatibilidad: un estado PRETEST 1.2 antiguo que ya hubiese usado N41 para otro comando sigue siendo legible. No se rechaza la sesión completa; en ejecución, PANIC tiene prioridad segura sobre esa asignación heredada. Se recomienda reaprender luego el comando antiguo en otra nota.

### Contrato de PANIC

PANIC sólo puede:

1. volver el transporte de la toma a un estado seguro;
2. desarmar la autoridad física de la toma;
3. liberar transitorios;
4. desarmar el modelo de salida;
5. activar APAGÓN.

PANIC **nunca** arma Art-Net y **nunca** desactiva APAGÓN. Repetir la nota 41 deja el sistema apagado y desarmado. El rearme posterior siempre es manual.

## 5. Grabación sincronizada

### Flujo recomendado

1. Seleccionar la canción correcta en AEYLA.
2. Confirmar entrada Art-Net estable desde Avolites.
3. Pulsar `GRABAR NUEVA TOMA` antes del inicio real de la canción.
4. El DAW puede estar detenido o puede estar corriendo en pre-roll.
5. Colocar la nota MIDI SHOW de PLAY exactamente en el inicio artístico de la canción.
6. Al pasar esa nota, AEYLA fija el marcador de sincronía de captura.
7. Detener y guardar la toma al final.
8. AEYLA conserva la toma RAW completa y aplica el marcador sólo como ENTRADA no destructiva.

### Fallback

Si la grabación comenzó con el DAW detenido, el primer `stopped -> running` puede fijar un anclaje de respaldo. Si luego aparece el marcador MIDI SHOW explícito, ese primer marcador puede refinar el anclaje. Retriggers posteriores no vuelven a moverlo.

## 6. Preview manual versus reproducción de show

El transporte manual de la pestaña TOMA sirve para revisar/editar una toma. Puede utilizar un reloj operativo monotónico independiente para seguir funcionando durante pruebas aun cuando el host suspenda audio.

**No es la ruta de sincronización del show.**

En producción, la toma debe lanzarse mediante MIDI SHOW desde el DAW para compartir el reloj de muestras con el audio.

## 7. Contrato de ARM / carrier Art-Net

Después de cargar una toma válida y armar la salida:

- AEYLA publica inmediatamente el cuadro actual/inicial;
- el receptor externo debe detectar Art-Net **antes de pulsar PLAY**;
- el carrier debe permanecer continuo mientras ARM siga vigente y el heartbeat del host sea seguro;
- PAUSA mantiene el cuadro;
- STOP / RESET de operador o MIDI vuelve al cuadro cero y conserva ARM + carrier;
- DISARM o APAGÓN retiran autoridad física;
- pérdida de heartbeat, render offline o fallo enclavado continúan siendo límites fail-closed.

El reset técnico interno conserva por defecto su semántica destructiva. Sólo el reset artístico de operador/MIDI solicita explícitamente conservar autoridad.

## 8. Gate físico obligatorio en REAPER / Windows

Usar un PC Windows interactivo, Ethernet cableado y un receptor externo como Capture o nodo real.

### Gate A — host/UI

- Instalar limpio el VST3 del BUILD_ID candidato.
- REAPER debe escanear y abrir AEYLA.
- La ventana debe aceptar mouse, pestañas y edición sin congelarse.
- Cerrar/abrir el FX no debe producir crash.

### Gate B — recepción y grabación

- Avolites -> Art-Net -> NIC RX -> AEYLA.
- Confirmar universo correcto y señal presente.
- Grabar una toma con pre-roll.
- Disparar la nota MIDI SHOW de inicio en el comienzo de la canción.
- Guardar y comprobar que la ENTRADA automática coincide visualmente con ese comienzo dentro de la resolución de un cuadro DMX.

### Gate C — carrier armado

- Configurar TX y destino Art-Net.
- Desactivar APAGÓN manualmente.
- Cargar una toma y pulsar ARMAR.
- **Sin pulsar PLAY**, Capture/nodo debe mostrar el universo emitido.
- Observar el universo al menos 30 s: no debe desaparecer ni depender de mover el playhead.

### Gate D — reproducción sincronizada

- Poner una nota MIDI SHOW de PLAY en el mismo punto de inicio del audio.
- Lanzar la canción al menos cinco veces desde cero.
- Verificar que audio y DMX repiten la misma relación temporal sin deriva acumulativa perceptible.
- Probar PAUSA / REANUDAR.
- Probar STOP / RESET: debe volver a cero sin desaparecer el carrier.

### Gate E — seguridad

- Con salida armada, enviar PANIC nota 41 en el canal MIDI SHOW.
- Resultado obligatorio: APAGÓN ACTIVO + salida desarmada.
- Enviar PANIC varias veces: nunca debe volver la luz ni rearmar la salida.
- Quitar APAGÓN manualmente: la salida debe seguir desarmada hasta un ARM manual.
- Probar DISARM manual: el receptor debe dejar de ver autoridad de la toma.

## 9. Criterio de promoción

R07 puede denominarse SHOW CANDIDATE sólo si:

- core-ci está verde en Windows, macOS y Linux;
- quality-ci está verde;
- formatos/validator están verdes;
- instaladores Windows/macOS se construyen y superan sus smokes;
- el smoke gráfico Windows confirma arranque sin crash;
- la ventana real fue validada manualmente en REAPER/Windows;
- Gate B, C, D y E anteriores pasan con hardware/red reales;
- no aparece ningún crash, pérdida persistente de carrier, rearme inesperado o desincronización acumulativa.

Si falla cualquier punto físico, conservar evidencia (video, BUILD_ID, IPs, universo y paso exacto) y mantener la compilación como PRETEST.

## 10. Regla de operación para show

Antes de show:

1. abrir sesión y AEYLA;
2. validar red RX/TX;
3. validar biblioteca/tomas y precarga MIDI;
4. confirmar APAGÓN activo durante preparación;
5. quitar APAGÓN manualmente;
6. ARMAR manualmente;
7. comprobar carrier en receptor externo;
8. ejecutar canciones mediante MIDI SHOW desde el DAW;
9. mantener PANIC nota 41 disponible como acción de salida a estado seguro.

No usar una compilación cuyo BUILD_ID no haya sido registrada en la prueba física correspondiente.
