# Backlog de implementación

Cada tarea debe terminar con pruebas, documentación y estado verificable.

## EPIC 0 — Repositorio y control de proyecto

- **P0-001 — IMPLEMENTED:** repositorio canónico creado en `rgb-estudios/Midi-to-dmx-RGB.ESTUDIOS`.
- **P0-002 — IMPLEMENTED:** foundation publicada en el repositorio canónico; conservar el bundle local como respaldo histórico.
- **P0-003:** activar protección de `main` y CI obligatorio.
- **P0-004:** crear milestones y labels definidos en `GITHUB_SETUP.md`.
- **P0-005 — IMPLEMENTED:** issues iniciales #1–#5 creados para gobernanza, Art-Net, proyecto portable, UI standalone y validación de hardware.

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
- **P2-002:** segmentos nombrados shutter/strobe/macro/reset.
- **P2-003:** coarse/fine 16 bits.
- **P2-004:** curvas e inversión.
- **P2-005:** validación de slots, duplicados, footprint y defaults.
- **P2-006:** protección de reset.
- **P2-007:** reemplazo masivo de perfil.
- **P2-008:** secuencia automática de prueba RGBWALUV/zoom/strobe.

## EPIC 3 — Motor visual

- **P3-001:** frame RGB interno de baja resolución.
- **P3-002:** muestreo bilinear de 14 puntos.
- **P3-003:** rig 10/14 y remuestreo de movimientos.
- **P3-004:** sólido y gradiente.
- **P3-005:** wave, wipe, pulse y chase.
- **P3-006:** noise determinista.
- **P3-007:** mirror/simetría.
- **P3-008:** imagen PNG/JPEG.
- **P3-009:** video H.264/MP4 con Media Foundation.
- **P3-010:** reproducción free, restart-on-note y transport-sync.
- **P3-011:** controles RGB→W/A/L y UV manual.

## EPIC 4 — Runtime de escenas

- **P4-001:** paletas.
- **P4-002:** looks.
- **P4-003:** escenas.
- **P4-004:** executors y mapa MIDI.
- **P4-005:** momentary/toggle/latch/one-shot/loop/flash/replace/release.
- **P4-006:** capas Base/Movement/FX/Safety.
- **P4-007:** HTP/LTP y prioridades.
- **P4-008:** fades de entrada/salida.
- **P4-009:** velocidad MIDI configurable.
- **P4-010:** seek determinista.

## EPIC 5 — Art-Net

- **P5-001:** UDP unicast.
- **P5-002:** broadcast.
- **P5-003:** selector de adaptador.
- **P5-004:** secuencia y refresh estable.
- **P5-005:** output thread.
- **P5-006:** captura Wireshark/golden packet.
- **P5-007:** prueba con nodo real.
- **P5-008:** pérdida/recuperación de red.

## EPIC 6 — USB-DMX

- **P6-001:** abstracción de backend.
- **P6-002:** DMX USB Pro oficial.
- **P6-003:** clones declarados compatibles, uno por uno.
- **P6-004:** device busy/disconnect/reconnect.
- **P6-005:** Open DMX/FTDI.
- **P6-006:** medición de frame timing.
- **P6-007:** matriz de drivers/modelos.

## EPIC 7 — Editor standalone

- **P7-001:** shell visual productiva.
- **P7-002:** canvas y fixture map.
- **P7-003:** source browser.
- **P7-004:** inspector contextual.
- **P7-005:** executor dock.
- **P7-006:** editor de perfiles.
- **P7-007:** patch y salidas.
- **P7-008:** validación y problemas.
- **P7-009:** import/export y versiones.
- **P7-010:** shortcuts y teclado.
- **P7-011:** screenshots 1366×768/1920×1080.

## EPIC 8 — VST3

- **P8-001:** fijar commit de iPlug2.
- **P8-002:** thin adapter sin audio.
- **P8-003:** recepción MIDI.
- **P8-004:** transporte opcional.
- **P8-005:** carga/recarga del proyecto.
- **P8-006:** estado del host.
- **P8-007:** ARM/blackout/status UI.
- **P8-008:** Steinberg validator.
- **P8-009:** scan/save/reopen en Ableton.
- **P8-010:** igualdad byte a byte standalone/VST3.

## EPIC 9 — Seguridad y show

- **P9-001:** startup seguro.
- **P9-002:** stop/close policy.
- **P9-003:** blackout prioritario.
- **P9-004:** haze emergency-off.
- **P9-005:** strobe limit.
- **P9-006:** hot reload atómico.
- **P9-007:** watchdogs y contadores.
- **P9-008:** soak test de 2 horas.
- **P9-009:** prueba bajo sesión Ableton + video.
- **P9-010:** drills de fallo y rollback.
- **P9-011:** ensayo completo.
