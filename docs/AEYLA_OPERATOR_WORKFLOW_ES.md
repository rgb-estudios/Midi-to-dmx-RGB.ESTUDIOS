# AEYLA — flujo operativo R07 PRETEST

Estado actual: **Simulated** con entradas/paquetes generados por software. No es
**Host-tested**, **Hardware-tested** ni Show Candidate.

Alcance congelado: una instancia, un universo Art-Net, hasta 15 canciones,
captura directa a disco a 44 Hz, referencia automática de inicio desde el
transporte que genera MTC, edición no destructiva y reproducción Art-Net.

## 1. Preparación

1. Cierra REAPER antes de instalar otra compilación de AEYLA.
2. Ejecuta `CLEAN_INSTALL_AEYLA.cmd` y vuelve a abrir el DAW.
3. Inserta una sola instancia de AEYLA en una pista de control.
4. Crea un proyecto con `NUEVO` o abre un `.aeylashow`.
5. Crea/selecciona una canción. El doble clic permite renombrarla.

`NUEVO` y `ABRIR` quedan bloqueados mientras GRABAR está activo. GUARDAR sigue
disponible. El armado de salida nunca se restaura desde el archivo del host.

## 2. Red de captura

1. Abre `RED / SALIDA`.
2. Selecciona el adaptador Ethernet correcto en `ENTRADA / ADAPTADOR RX`.
3. Confirma `RX ACTIVO` o `RX LISTO · ESPERANDO ART-NET`.
4. En Avolites habilita Art-Net continuo para el mismo universo.
5. No inicies GRABAR hasta que AEYLA muestre `SEÑAL PRESENTE`.

AEYLA nunca captura y transmite simultáneamente. Si una salida está armada,
GRABAR muestra el motivo del bloqueo y no modifica la toma existente.

## 3. Grabación alineada con MTC

Este es el orden requerido cuando el mismo DAW envía MTC a Avolites:

1. Detén el transporte del DAW.
2. Pulsa `GRABAR NUEVA TOMA` en AEYLA.
3. Comprueba `ESPERANDO REPRODUCIR / MTC`.
4. Inicia el transporte del DAW. El DAW envía MTC a Avolites y, en paralelo,
   entrega a AEYLA el flanco de transporte y su conteo de muestras.
5. AEYLA conserva el pre-roll en la TOMA BRUTA y fija ese cuadro como
   `ENTRADA AUTO`; no modifica el archivo original.
6. Al terminar la canción, detén primero el transporte y luego pulsa
   `DETENER + GUARDAR TOMA`.

AEYLA usa el transporte directo del host, no necesita decodificar los mensajes
MTC Quarter Frame `0xF1` que el propio DAW está enviando a la consola. Si GRABAR
comienza con el DAW ya reproduciendo, la toma se guarda, pero la ENTRADA debe
ajustarse manualmente.

## 4. Edición y consolidación

1. En `TOMA / EDICIÓN`, revisa el cabezal y la actividad DMX.
2. Arrastra `ENTRADA` y `SALIDA`, escribe `MM:SS.mmm` o ajusta `±1f`.
3. Usa rueda para ampliar y `Mayús + arrastre` para desplazarte.
4. Comprueba la duración efectiva.
5. Pulsa `CONSOLIDAR MUESTRA DMX` si deseas materializar el rango.

La consolidación crea otro `.aeylatake` cuyo `00:00` corresponde a ENTRADA.
La TOMA BRUTA permanece byte por byte intacta. Las flechas cambian de versión y
`VOLVER A TOMA BRUTA` recupera la fuente original.

No se permiten cambios de rango, versión o canción mientras GRABAR,
REPRODUCIR o la salida física estén activos.

## 5. Preparar la salida

1. Abre `RED / SALIDA`.
2. Selecciona el adaptador Ethernet correcto en `SALIDA / ADAPTADOR TX`.
3. Introduce `IPv4 AEYLA / MÁSCARA`, por ejemplo
   `2.0.0.20 / 255.0.0.0`.
4. Pulsa `APLICAR IP Y PREPARAR ART-NET`.
5. En Windows confirma UAC. El helper agrega una IPv4 secundaria, conserva la
   red previa, verifica el bind y hace rollback si la validación falla.
6. Confirma `RED LISTA`, motor a `44 Hz`, `DESARMADA` y cero errores nuevos.

Cambiar el adaptador o la red fuerza `APAGÓN + DESARMADO`.

## 6. Transmitir una toma

1. Comprueba que GRABAR está detenido.
2. Desactiva `APAGÓN`.
3. Pulsa `ARMAR SALIDA DE TOMA`.
4. Pulsa `REPRODUCIR TOMA ACTIVA`.
5. Verifica `TOMA AL AIRE`, contador TX creciente y recepción en el nodo.
6. Minimiza/cierra solamente la ventana del plugin y comprueba durante al
   menos cinco minutos que el receptor externo continúa cerca de 44 Hz.

`DETENER / MANTENER` conserva el último cuadro. `APAGÓN` tiene prioridad,
desarma toda autoridad y solicita una ráfaga de tres cuadros DMX en cero.

## 7. Recuperación de fallos

Tres errores UDP consecutivos enclavan el fail-closed una sola vez:

1. AEYLA desarma toma y modelo.
2. Activa APAGÓN y muestra `FALLO ENCLAVADO · REARME MANUAL`.
3. Corrige cable, NIC o configuración.
4. Desactiva APAGÓN.
5. Pulsa ARMAR explícitamente.

AEYLA no se rearma sola. Si se cambia de red, se desactiva el FX, se descarga
el plugin, el host entra en render sin conexión o se pierde su vida operativa,
la salida vuelve al estado seguro.

Si el host descarga el plugin durante GRABAR, AEYLA intenta finalizar en disco
una toma recuperable en vez de borrar el temporal. Esa recuperación no sustituye
`DETENER + GUARDAR`: al reabrir, vuelve a seleccionar la misma biblioteca y
verifica la toma antes de editarla o transmitirla.

## 8. Gates aún abiertos

- REAPER, Ableton Live y Logic con una matriz formal y repetible.
- UAC/rollback en un Windows físico.
- NIC → nodo → DMX/luminaria, pérdida/reconexión y unicast/broadcast dirigido.
- interacción visual completa a 960×620 y 1280×800 en ambos sistemas.
- soak de 8 horas y tres ensayos completos.
- MIDI Learn y comandos de operación de show.

Hasta cerrar esos gates, todo paquete debe llamarse **PRETEST**.
