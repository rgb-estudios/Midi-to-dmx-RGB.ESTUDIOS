# CP-AEYLA-0.3.1 — CI recovery + deterministic UI/runtime cleanup

- Fecha: 2026-08-08
- Repositorio: `rgb-estudios/Midi-to-dmx-RGB.ESTUDIOS`
- PR: #14
- Base inspeccionada: `ebce96470c615cc550b9d4a0482e71e0e4d37c85`
- Commits publicados: `83f4cd23acdc54661e926a52f8b1f43abbb96568`,
  `8c6a916ad9c3eaaac1e1b534a403af8a34089573`
- Estado PCB/hardware: no aplica; Art-Net físico **NOT CONNECTED**

## Cambios

- Semántica de prueba MIDI corregida: Show/Cue tiene precedencia; executors
  36–43 quedan como diagnóstico solo sin Show.
- macOS floor unificado en 11.0 y dependencias universales arm64+x86_64.
- Fase artística derivada de PPQ absoluto del host.
- Eliminadas mutaciones desde `Draw()` y controles ARM/executor duplicados.
- README, contrato de agentes, workflow operativo y matrices sincronizados.

## Validación ejecutada

- `git diff --check`: PASS.
- `aeyla_application_model_tests` compilado manualmente con GCC 13.3: PASS.
- `aeyla_host_song_binding_tests` compilado manualmente con GCC 13.3: PASS.
- GitHub `quality-ci` (ASan/UBSan/TSan): PASS en commit publicado.
- GitHub `core-ci` (Linux/Windows/macOS): PASS en commit publicado.
- GitHub `vst3-and-standalone-proof`: PASS Windows/macOS en `8c6a916`.
- VST3 y AUv2 macOS: build universal arm64+x86_64 PASS; el gate falló por
  orden incorrecto de argumentos de `lipo`, ya corregido para revalidación.
- `core-ci` expuso en macOS un snapshot rasgado del mailbox de transporte;
  barrera seqlock corregida y campaña local optimizada 201 veces PASS.
- REAPER macOS construyó ambos formatos; el mount del DMG se corrigió para
  macOS 15 enviando la aceptación por stdin y guardando transcript.
- Búsqueda de wall-clock artístico en producto: PASS; solo permanece
  `steady_clock` en el scheduler de refresh del worker Art-Net, donde corresponde.

## No validado

- Suite CTest completa local: BLOCKED porque el entorno no incluye CMake; la
  matriz equivalente de `core-ci` sí pasó en Linux/Windows/macOS.
- Validadores VST3/AUv2 macOS: requieren reejecutar el gate corregido.
- Windows standalone crash y REAPER timeout: siguen FAILED hasta diagnóstico.
- El workflow standalone ahora preserva full crash dump, PDB, ejecutable y
  evidencia aunque el proceso falle antes de crear ventana.
- WER del runner no produjo el primer dump; se añadió ProcDump oficial como
  capturador determinista y símbolos PDB para el siguiente intento.
- Dump completo obtenido: execute access violation a `RIP=0` desde la llamada
  indirecta a `glCreateProgram()` en NanoVG después de que `gladLoadGL()` no
  cargara la función. Seguimiento P0: GitHub issue #17.
- Hardware Art-Net/nodo/PAR: NOT CONNECTED.

## Próximo bloque

1. Resolver #17 con preflight/fallback gráfico; nunca continuar NanoVG con GL incompleto.
2. Resolver host smoke REAPER y exigir un archivo de resultado verificable.
3. Sustituir dependencia de `OnIdle` si el editor cerrado detiene runtime.
4. Implementar show-level bindings y authoring transaccional Store Look/Cue.
