# RGB Live Control R10.8 — Performance / Stability PRETEST

Estado: `PRETEST_NOT_SHOW_CANDIDATE`

## Objetivo

R10.8 reduce la presión que RGB Live Control ejerce sobre el host durante captura y reproducción DMX. No modifica el contrato de ARM, APAGÓN TOTAL, PANIC, GO A→B, persistencia R10.7 ni el formato artístico del show.

## Cambios de rendimiento

- El callback de audio continúa sin I/O de disco, sin red y sin `mModelMutex`; publica transporte/contadores y avanza el cursor por atomics.
- Durante GRABAR DMX, el escritor mantiene una cola acotada en RAM y el I/O pertenece a un thread dedicado.
- Se elimina el `_commit/fsync` durable periódico de cada segundo durante REC. El frame-count se checkpointa de forma liviana cada 5 s y el sync durable/checksum se reserva para finalizar la toma.
- El runtime general baja de 250 Hz (4 ms) a 125 Hz (8 ms). Sigue muy por encima de la frecuencia DMX y reduce reconciliaciones/locks innecesarios.
- El hot path de REC usa `recorded_frames_fast()` en lugar de construir `ArtNetCaptureStats` completos en cada tick.
- El polling del clip respaldado por archivo baja de 1 ms a 4 ms.
- El supervisor de Take baja de 2 ms a 5 ms. En file-mode la reproducción por reloj de muestras sigue a cargo del file player.

## Invariantes que no pueden cambiar

- ARM mantiene autoridad y carrier Art-Net durante STOP/PAUSA/HOLD, navegación y cambio de ventana.
- APAGÓN TOTAL/PANIC mantiene autoridad y envía DMX 0 continuo.
- DESARMAR sigue siendo la retirada voluntaria de autoridad.
- A AL AIRE → B PREPARADA → GO usa reemplazo atómico sin corte de carrier.
- `live.bin v2` y las 8 memorias renombrables de R10.7 permanecen compatibles.

## Gate físico de rendimiento

Usar la misma sesión, interfaz de audio, sample rate y buffer para comparar R10.7 vs R10.8.

1. Abrir REAPER con RGB Live Control y medir comportamiento con el plugin cargado, sin REC DMX.
2. ARMAR Art-Net y confirmar carrier estable ~44 Hz.
3. Iniciar grabación de audio normal en REAPER y, en paralelo, GRABAR DMX.
4. Mantener 3–5 minutos de captura continua.
5. No deben existir tirones periódicos cercanos a 1 s ni dropouts de audio atribuibles al plugin.
6. El contador de captura debe avanzar; `queue_overflows` debe permanecer 0 y `storage_failed` debe permanecer falso.
7. Detener GRABAR DMX. La finalización debe producir una `.aeylatake` válida y reabrible con checksum correcto.
8. Durante REC, ARM/carrier no debe caer y APAGÓN no debe activarse solo.
9. Repetir A→B GO, HOLD/PAUSA/STOP y APAGÓN para confirmar que la optimización no alteró seguridad.

## Nota sobre STOP

La finalización de una toma sí realiza flush/sync durable y checksum. Es correcto que el trabajo durable exista al cerrar la toma; no debe ejecutarse periódicamente mientras el DAW está grabando. Si una finalización grande todavía produjera un tirón mientras el transporte de audio sigue corriendo, la siguiente corrección será desacoplar también el finalize del gesto de STOP DMX.

## Criterio de aceptación

R10.8 sólo puede avanzar desde PRETEST cuando la prueba física muestre una sesión estable durante captura simultánea de audio + DMX y se conserven todos los contratos de autoridad de R10.7.
