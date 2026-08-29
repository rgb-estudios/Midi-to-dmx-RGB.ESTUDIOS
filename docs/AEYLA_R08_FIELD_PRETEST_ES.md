# AEYLA Visual DMX — R08 FIELD PRETEST

**Estado:** PRETEST / NO SHOW CANDIDATE  
**Rama:** `aeyla/r08-capture-zero-midi-rec-20260829`  
**Regla:** no promover ni mezclar a `main` hasta aprobar CI y pruebas físicas A–E.

## 1. Arquitectura de sincronía

AEYLA no usa MTC como reloj primario de reproducción DMX. El DAW es la referencia común:

- audio de la canción;
- MIDI SHOW hacia AEYLA;
- MTC/SMPTE hacia Avolites en paralelo.

La reproducción de una toma en show usa el reloj de muestras del host. El reproductor manual de toma es sólo PREVIA/EDICIÓN y usa un reloj independiente.

La captura y reproducción DMX trabajan nominalmente a **44 Hz**, por lo que la resolución física es aproximadamente **22,7 ms por cuadro**. No se promete precisión DMX sub-cuadro.

## 2. Mapa MIDI SHOW R08

Por defecto el mapa está desactivado y usa canal 16 al habilitarse.

| Nota | Acción |
|---|---|
| N36 | Canción anterior / PREPARADA |
| N37 | Canción siguiente / PREPARADA |
| N38 | PLAY / retrigger / marcador explícito de sincronía durante REC |
| N39 | PAUSA / REANUDAR |
| N40 | STOP / RESET |
| N41 | PANIC: APAGÓN + DESARME |
| N42 | GRABAR / DETENER captura RAW |
| N48–N62 | Lanzamiento directo canciones 1–15 |

N41 y N42 son reservas fijas. No pueden aprenderse sobre otros comandos. Los mapas antiguos que ya colisionaban con esas notas siguen pudiendo cargarse, pero N41/N42 tienen prioridad en ejecución.

Ninguna nota MIDI puede ARMAR la salida física ni quitar APAGÓN.

## 3. Flujo recomendado para grabar una canción

### Sin pre-roll artístico

1. Seleccionar canción.
2. Seleccionar una vez la biblioteca de tomas.
3. Confirmar RX Art-Net activo y señal válida desde Avolites.
4. DAW detenido y cursor exactamente en el inicio real de la canción.
5. Pulsar **N42**: AEYLA inicia captura RAW.
6. Pulsar **PLAY** en el DAW.
7. El primer inicio de transporte fija automáticamente el CERO/IN de la captura.
8. Al terminar, pulsar **N42** nuevamente.
9. AEYLA cierra y verifica el archivo, conserva RAW y carga el rango editable.

El marcador automático por PLAY se resuelve dentro de la granularidad física DMX. N38 puede usarse como refinamiento explícito.

### Con pre-roll o cuando PLAY ocurre antes del inicio artístico

1. Pulsar N42 antes de comenzar el pre-roll.
2. Iniciar PLAY.
3. Colocar **N38 exactamente en el punto donde comienza realmente la canción/audio**.
4. N38 reemplaza una sola vez el ancla automática de transporte por el cuadro de captura tomado al ingreso MIDI.
5. Retriggers posteriores no pueden mover ese CERO.
6. N42 al final cierra la toma.

Este es el flujo preferido cuando el PLAY del DAW no coincide con el inicio musical.

### Si el DAW ya estaba corriendo al iniciar N42

No existe un nuevo flanco STOP→PLAY que pueda servir de referencia. Usar **N38** en el inicio artístico.

## 4. Contrato de RAW

- El archivo RAW no se recorta ni se destruye al fijar CERO/IN.
- El auto-IN sólo modifica el rango editable asociado a la toma.
- Si no existe marcador válido, la toma se conserva completa y el IN se ajusta manualmente.
- Al detener N42, el archivo debe aparecer verificablemente en el índice de biblioteca antes de considerarse guardado.

## 5. Contrato de salida Art-Net

Una toma cargada y ARMADA debe mantener autoridad Art-Net aun con el DAW detenido:

1. APAGÓN OFF manual.
2. Cargar toma.
3. ARMAR manualmente.
4. Sin pulsar PLAY, un receptor externo debe ver el universo y el carrier aproximado de 44 Hz.
5. STOP/RESET vuelve al cuadro inicial y conserva el carrier si continúa armado.
6. DISARM elimina la autoridad de la toma.

Durante PLAY con reloj por muestras, la pérdida del pulso del host es fail-closed: AEYLA desarma la salida y expone diagnóstico al operador.

## 6. PANIC N41

N41 es una acción de seguridad de una sola dirección:

- activa APAGÓN;
- desarma el scheduler de toma;
- libera transitorios;
- no puede volver a encender salida;
- repetir N41 no rearma nada;
- quitar APAGÓN manualmente no es suficiente: se requiere ARM explícito.

## 7. Windows / macOS

Los archivos `.aeylashow` y `.aeylatake` son portables entre Windows y macOS. Al migrar de sistema puede ser necesario seleccionar nuevamente la carpeta de biblioteca porque la ruta del sistema de archivos cambia.

El mismo código de captura, carrier, MIDI SHOW y seguridad debe compilar para:

- Windows x64 VST3 / REAPER;
- macOS Universal arm64 + x86_64 VST3;
- macOS Universal AUv2;
- Ableton Live en macOS mediante formato compatible.

## 8. Pruebas físicas obligatorias

### A — Host/UI

- instalación limpia;
- REAPER encuentra VST3;
- UI nativa abre y responde;
- cerrar/reabrir FX sin crash.

### B — RX / captura

- Art-Net desde Avolites estable;
- N42 inicia RAW;
- PLAY desde detenido fija CERO automático;
- repetir con pre-roll y N38 en inicio artístico;
- N42 detiene;
- RAW preservado;
- IN resultante dentro de un cuadro DMX de la referencia esperada.

### C — Carrier

- salida configurada;
- APAGÓN OFF;
- toma cargada + ARM;
- receptor ve Art-Net antes de PLAY;
- carrier estable al menos 30 s.

### D — Reproducción show

- N38 junto al inicio de audio;
- cinco lanzamientos desde cero;
- sin deriva acumulativa perceptible;
- pausa/reanudar;
- N40 vuelve a cero y conserva carrier armado;
- PREV/NEXT sólo cambia PREPARADA y no corta la canción al aire.

### E — Seguridad

- estando armado, N41;
- APAGÓN activo + salida desarmada;
- repetir N41 no relanza salida;
- quitar APAGÓN no rearma;
- DISARM elimina autoridad de toma;
- pérdida de reloj durante PLAY desarma fail-closed.

## 9. Criterio de promoción

R08 sólo puede pasar de PRETEST a SHOW CANDIDATE cuando:

1. todos los gates automatizados relevantes estén verdes en **el mismo SHA**;
2. las pruebas físicas A–E pasen con evidencia;
3. Windows y macOS se empaqueten desde ese mismo SHA;
4. hashes y BUILD_ID/SOURCE_COMMIT queden registrados;
5. no exista ningún P0/P1 de salida, captura, sincronía o seguridad abierto.
