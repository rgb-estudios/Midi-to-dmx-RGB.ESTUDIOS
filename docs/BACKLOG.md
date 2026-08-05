# Backlog de implementación

Cada tarea debe terminar con pruebas, documentación y estado verificable.

## EPIC 0 — Repositorio y control de proyecto

- **P0-001 — IMPLEMENTED:** repositorio canónico creado en `rgb-estudios/Midi-to-dmx-RGB.ESTUDIOS`.
- **P0-002 — IMPLEMENTED:** foundation publicada en el repositorio canónico; conservar el bundle local como respaldo histórico.
- **P0-003:** activar protección de `main` y CI obligatorio.
- **P0-004:** crear milestones y labels definidos en `GITHUB_SETUP.md`.
- **P0-005 — IN PROGRESS:** crear issues iniciales de P0/P1 y mantener el resto del backlog documentado.

## EPIC 1 — Modelo de proyecto

- **P1-001:** implementar lectura/escritura JSON con dependencia seleccionada y licenciada.
- **P1-002:** validación completa contra esquemas.
- **P1-003:** IDs UUID estables.
- **P1-004:** paquete ZIP `.aeylashow`.
- **P1-005:** checksums SHA-256.
- **P1-006:** guardado atómico y backup.
- **P1-007:** migraciones de schema.
- **P1-008:** detección de medios faltantes.

## EPIC 2 — Perfiles semánticos

- **P2-001:** modos continuous/range/constant/trigger/ignore.
- **P2-002:** editor drag-and-drop de slots.
- **P2-003:** segmentos shutter/strobe/macro.
- **P2-004:** reset protegido.
- **P2-005:** coarse/fine 16-bit.
- **P2-006:** validación de footprint, duplicados y canales sin asignar.
- **P2-007:** sustitución masiva de perfiles.
- **P2-008:** import/export `.aeylaprofile`.

## EPIC 3 — Motor visual

- **P3-001:** framebuffer CPU/GPU de baja resolución.
- **P3-002:** muestreo puntual y promedio por radio.
- **P3-003:** solid/gradient/image.
- **P3-004:** wave/pulse/wipe/noise/chase.
- **P3-005:** mirror y simetría.
- **P3-006:** videos H.264 por Media Foundation.
- **P3-007:** free/retrigger/host-position time modes.
- **P3-008:** frame cache y seek.

## EPIC 4 — Runtime de escenas

- **P4-001:** paletas y looks.
- **P4-002:** escenas.
- **P4-003:** tres capas mínimas.
- **P4-004:** HTP/LTP/priority.
- **P4-005:** executor momentary/toggle/latch/one-shot/loop/flash/replace/release.
- **P4-006:** fades.
- **P4-007:** velocidad MIDI opcional.
- **P4-008:** blackout y emergencia.

## EPIC 5 — Art-Net

- **P5-001:** socket UDP y selección de NIC.
- **P5-002:** unicast/broadcast.
- **P5-003:** universo y secuencia.
- **P5-004:** hilo de salida 30–44 Hz.
- **P5-005:** fake backend y captura de paquetes.
- **P5-006:** prueba física con nodo.

## EPIC 6 — USB-DMX

- **P6-001:** interfaz abstracta de backends.
- **P6-002:** DMX USB Pro compatible.
- **P6-003:** enumeración y dispositivo ocupado.
- **P6-004:** Open DMX/FTDI.
- **P6-005:** timing BREAK/MAB.
- **P6-006:** pruebas físicas y matrices de drivers.

## EPIC 7 — Standalone Editor

- **P7-001:** integrar framework iPlug2.
- **P7-002:** shell de ventanas y dock/paneles.
- **P7-003:** source library.
- **P7-004:** canvas y fixture map.
- **P7-005:** inspector semántico.
- **P7-006:** editor de perfiles.
- **P7-007:** executors.
- **P7-008:** monitor de salida.
- **P7-009:** import/export y recent files.
- **P7-010:** accesibilidad, teclado y escalado DPI.

## EPIC 8 — VST3 Runtime

- **P8-001:** target VST3.
- **P8-002:** pista MIDI silenciosa.
- **P8-003:** cola lock-free de eventos.
- **P8-004:** estado y ubicación de show.
- **P8-005:** host playhead opcional.
- **P8-006:** reload seguro.
- **P8-007:** UI mínima.
- **P8-008:** Ableton validation matrix.

## EPIC 9 — Releases

- **P9-001:** instalador Windows.
- **P9-002:** firma de código.
- **P9-003:** CI Windows.
- **P9-004:** artifacts.
- **P9-005:** updater manual y rollback.
- **P9-006:** checklist rehearsal/show.
