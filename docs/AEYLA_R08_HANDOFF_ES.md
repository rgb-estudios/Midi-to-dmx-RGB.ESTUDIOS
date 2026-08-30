# AEYLA Visual DMX — R08 HANDOFF / TRASPASO

**Estado contractual:** PRETEST. No es todavía SHOW CANDIDATE.  
**Repositorio:** `rgb-estudios/Midi-to-dmx-RGB.ESTUDIOS`  
**Rama de trabajo:** `aeyla/r08-capture-zero-midi-rec-20260829`  
**PR:** #28 — `R08 capture sync: cero por MIDI + REC N42`  
**Base R08 macOS/Ableton:** `aeyla/r08-macos-ableton-20260829`  
**Regla absoluta:** NO mezclar a `main` sin instrucción explícita del propietario y sin aprobar gates físicos.

---

## 1. Objetivo del producto

AEYLA Visual DMX es un plug-in de show en vivo que permite:

1. recibir Art-Net desde Avolites;
2. grabar DMX multi-canal de un universo a una toma RAW `.aeylatake` a 44 Hz nominales;
3. editar IN/OUT de forma no destructiva;
4. asociar tomas a canciones;
5. reproducirlas desde el reloj de muestras del DAW;
6. transmitir Art-Net de salida manteniendo un contrato fail-closed;
7. controlar el show mediante notas MIDI dedicadas;
8. transportar el mismo show/tomas entre Windows y macOS.

El host final objetivo es **Ableton Live 12 en macOS**. REAPER/Windows sigue siendo entorno de desarrollo, prueba de campo y diagnóstico.

---

## 2. Arquitectura de sincronía

AEYLA **no usa MTC como reloj primario para reproducir DMX**.

La referencia primaria es el reloj de muestras del host:

`DAW audio + MIDI SHOW -> AEYLA sample clock -> scheduler de toma -> Art-Net TX`

Para Avolites, la recomendación operacional sigue siendo una pista LTC/SMPTE dedicada desde Ableton hacia la entrada LTC del T3. El esquema de una hora por canción puede usarse para ordenar el show, pero LTC/MTC no gobierna el scheduler interno de AEYLA.

No prometer sincronía absoluta sub-cuadro: la captura DMX es nominalmente **44 Hz**, equivalente a aproximadamente **22,7 ms por cuadro**.

---

## 3. Mapa MIDI SHOW R08

Mapa operativo por defecto al habilitar MIDI SHOW: canal 16.

| Nota | Acción |
|---|---|
| N36 | Canción anterior / PREPARADA |
| N37 | Canción siguiente / PREPARADA |
| N38 | PLAY / retrigger + marcador explícito de CERO durante captura |
| N39 | PAUSA / REANUDAR |
| N40 | STOP / RESET |
| N41 | PANIC: APAGÓN + DESARME |
| N42 | GRABAR / DETENER captura RAW |
| N48–N62 | Canciones directas 1–15 |

N41 y N42 son reservas fijas. Ninguna nota MIDI puede ARMAR físicamente la salida ni quitar APAGÓN.

---

## 4. Captura RAW y fijación de CERO

### Flujo recomendado sin pre-roll

1. seleccionar canción;
2. seleccionar una vez la biblioteca de tomas;
3. verificar Art-Net RX y fuente válida;
4. DAW detenido;
5. N42 inicia RAW;
6. PLAY en el inicio real;
7. el flanco STOP→PLAY captura atómicamente el cursor del grabador a 44 Hz y fija el auto-IN;
8. N42 detiene/finaliza;
9. AEYLA verifica la toma, conserva RAW y crea el estado editable.

### Flujo con pre-roll

1. N42 inicia RAW;
2. PLAY inicia pre-roll;
3. N38 se coloca exactamente en el inicio artístico;
4. el evento MIDI transporta un snapshot del frame de captura tomado en ingreso MIDI;
5. el primer N38 explícito sustituye/refina el ancla automática;
6. N42 finaliza.

### Si REC comienza con el DAW ya corriendo

No existe flanco STOP→PLAY nuevo. Usar N38 en el inicio artístico.

### Contrato RAW

- RAW nunca se recorta/destruye al fijar CERO;
- IN/OUT son metadata no destructiva;
- consolidar crea una nueva `.aeylatake`;
- volver a RAW debe recuperar la fuente original.

---

## 5. Persistencia de host — estado 1.3

El estado de componente fue promovido a **1.3** para persistir:

- locator de biblioteca de tomas;
- basename seguro de la `.aeylatake` por canción;
- `start_frame` / `end_frame_exclusive`;
- asociación no destructiva con la canción.

### Reapertura en el mismo equipo

Si la carpeta guardada existe exactamente, AEYLA puede rehidratar biblioteca + toma + trims al reabrir el Set/proyecto.

### Windows -> macOS o macOS -> Windows

AEYLA **no traduce ni inventa rutas** (`C:\...` no se convierte a `/Users/...`). Si la ruta original no existe:

- mantiene bindings y trims pendientes;
- muestra que la biblioteca guardada no está disponible;
- el operador selecciona la biblioteca correcta una vez;
- AEYLA resuelve por nombre de archivo seguro y valida geometría/frame-count antes de rehidratar.

La restauración de host **nunca arma Art-Net ni inicia PLAY automáticamente**.

La conversión UTF-8 de paths usa `std::u8string` + `std::filesystem::path`; se eliminó el uso de `std::filesystem::u8path()` deprecado que fallaba bajo GCC 13 con warnings-as-errors.

---

## 6. Reset de motor / sample-rate

`OnReset()` no hace I/O, locking pesado ni carga de archivo en el callback. Marca un reset pendiente y el runtime no-realtime:

- invalida una toma precargada cuya conversión dependía del sample-rate anterior;
- desarma/fail-closes cuando corresponde;
- obliga a que el siguiente PLAY/preflight recargue la toma con el sample-rate actual.

No continuar reproduciendo una toma calibrada al sample-rate anterior.

---

## 7. Carrier, reproducción y autoridad Art-Net

Con una toma cargada y salida manualmente ARMADA:

- el carrier puede permanecer activo con el DAW detenido;
- STOP/RESET vuelve al inicio sin necesariamente retirar carrier;
- PLAY avanza por muestras procesadas;
- una pérdida real del pulso del host durante PLAY desarma fail-closed;
- cambio de canción preparada no debe crear un hueco de autoridad de un ciclo.

La autoridad física sigue siendo explícita: **APAGÓN OFF manual + ARM manual**.

---

## 8. PANIC N41

N41 es una transición de seguridad de una sola dirección:

- APAGÓN ON;
- DESARME;
- limpieza de transitorios;
- repetir N41 nunca relanza salida;
- quitar APAGÓN no rearma;
- ARM debe ser explícito/manual.

---

## 9. Compatibilidad y formatos

### Windows

Objetivo: Windows x64 + VST3 + REAPER.

Paquete CI esperado:

`AEYLA-0.3-REAPER-Windows-PRETEST`

Debe incluir VST3, helper de red, scripts de auditoría/clean/install/clean-install, `BUILD_ID.txt` y documentación de prueba.

### macOS

Objetivo: Ableton Live 12, macOS, plugin Universal.

Paquete CI esperado:

`AEYLA-R08-ABLETON-macOS-Universal-PRETEST`

Debe incluir:

- VST3 Universal `arm64+x86_64`;
- AUv2 Universal `arm64+x86_64`;
- scripts install/uninstall/audit;
- network preflight;
- `BUILD_ID.txt`;
- `SHA256SUMS.txt`;
- manual Ableton/macOS.

El CI Mac debe validar `lipo`, `codesign`, Steinberg Validator y `auval`.

### Archivos de proyecto

`.aeylashow` y `.aeylatake` están diseñados para ser portables Windows/macOS. No copiar paths absolutos como mecanismo de portabilidad; copiar archivos y re-seleccionar biblioteca cuando cambie la raíz del sistema.

---

## 10. Incidencias técnicas ya reparadas en R08

- offset de CERO causado por leer `recorded_frames` demasiado tarde en el runtime;
- N38 ahora porta snapshot de frame tomado en ingreso MIDI;
- N42 REC ejecuta disco/red sólo fuera del hilo de audio;
- flanco PLAY puede fijar automáticamente el CERO durante RAW;
- stale transport markers no pueden heredarse por una captura posterior;
- carrera `READY -> PLAYING` durante reemplazo de canción armada que podía retirar autoridad por un ciclo;
- diagnóstico de pérdida de heartbeat alineado con el comportamiento fail-closed;
- crash standalone Windows `0xC0000005` asociado al modo iPlug2 `--no-io` y selección MIDI sin backend;
- binding RX macOS apto para Art-Net broadcast mediante interfaz adecuada;
- carrier silencioso de Ableton no depende de que la UI permanezca abierta;
- sample-rate reset invalida take precargada;
- persistencia 1.3 de biblioteca/toma/trims;
- portabilidad UTF-8 de locator sin `u8path()` deprecado.

---

## 11. Riesgo conocido abierto

### P2 — saturación extrema de cola MIDI coincidente con N42 STOP

Si una saturación excepcional coincide exactamente con N42 STOP:

- la salida entra en APAGÓN/DESARME correctamente;
- la captura RAW puede requerir un segundo N42 para cerrar;
- no existe una ruta que rearme o relance luces por este caso.

No elevar a SHOW CANDIDATE sin revisar este punto o demostrar que queda fuera de las condiciones operativas reales.

---

## 12. Gates automáticos requeridos sobre el MISMO SHA

Antes de entregar como Show Candidate revisar, como mínimo:

- `core-ci` Windows/macOS/Linux;
- sanitizers relevantes;
- VST3 validator Windows;
- VST3 validator macOS;
- AUv2 `auval`;
- macOS Universal slices;
- Ableton macOS PRETEST package;
- REAPER Windows PRETEST package;
- REAPER host smoke;
- Windows startup smoke / diagnóstico;
- instalador/install-uninstall smokes.

Un build que compila pero no comparte `SOURCE_COMMIT` con los otros artefactos NO constituye una entrega coherente.

---

## 13. Gates físicos obligatorios A–E

### A — Host/UI

Instalación limpia, descubrimiento de plugin, abrir/cerrar/resize repetido sin crash.

### B — MIDI/transport

N36, N37, N38, N39, N40, N41, N42 y canciones directas. Repetir lanzamientos desde cero y revisar que no exista deriva acumulativa.

### C — Captura real

Avolites -> Art-Net -> Ethernet -> AEYLA. N42, PLAY, N38 con pre-roll, N42 STOP. Verificar RAW y auto-IN dentro de la resolución física de 44 Hz.

### D — Carrier/replay

APAGÓN OFF manual, ARM manual, carrier detenido estable 30 s, PLAY por sample-clock, STOP/reset, DISARM.

### E — Safety

ARM -> N41 = blackout + disarm. Repetir N41 no rearma. Quitar blackout no rearma. Pérdida del heartbeat durante PLAY debe fail-close.

---

## 14. Entrega / congelamiento

Para traspasar el trabajo:

1. usar esta rama y PR como fuente de verdad;
2. no reconstruir requisitos desde conversaciones antiguas;
3. revisar primero este documento y `docs/AEYLA_R08_FIELD_PRETEST_ES.md`;
4. registrar el HEAD exacto mediante `git rev-parse HEAD`;
5. descargar los dos artefactos PRETEST generados desde ese mismo HEAD;
6. verificar `BUILD_ID.txt` / `SOURCE_COMMIT` / hashes;
7. ejecutar las pruebas físicas A–E;
8. sólo después evaluar promoción a SHOW CANDIDATE;
9. `main` permanece sin cambios hasta autorización explícita.

---

## 15. Prompt de continuidad para otro ChatGPT Work / ingeniero

> Continúa AEYLA Visual DMX desde `rgb-estudios/Midi-to-dmx-RGB.ESTUDIOS`, PR #28, rama `aeyla/r08-capture-zero-midi-rec-20260829`. Lee primero `docs/AEYLA_R08_HANDOFF_ES.md` y `docs/AEYLA_R08_FIELD_PRETEST_ES.md`. No mezcles a `main`. Mantén estado PRETEST hasta que CI Windows/macOS y gates físicos A–E pasen sobre el mismo SHA. Prioridad: CORREGIR -> SIMPLIFICAR -> ESTABILIZAR -> VERIFICAR. No sustituir el sample-clock del DAW por MTC. Preservar RAW, PANIC fail-closed, ARM manual y portabilidad Windows/macOS. Toda nueva afirmación de estabilidad debe tener evidencia reproducible.

---

## 16. Criterio de cierre de R08

R08 puede considerarse técnicamente traspasado cuando existen simultáneamente:

- código limpio en la rama;
- sin workflows one-shot temporales;
- handoff canónico presente;
- paquetes Windows y macOS del mismo SHA;
- BUILD_ID/hashes verificables;
- CI automatizado relevante documentado;
- lista explícita de gates físicos pendientes;
- ningún merge a `main` realizado accidentalmente.
