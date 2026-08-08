# CP-AEYLA-0.3.1 — CI recovery + deterministic UI/runtime cleanup

- Fecha: 2026-08-08
- Repositorio: `rgb-estudios/Midi-to-dmx-RGB.ESTUDIOS`
- PR: #14
- Base inspeccionada: `ebce96470c615cc550b9d4a0482e71e0e4d37c85`
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
- Búsqueda de wall-clock artístico en producto: PASS; solo permanece
  `steady_clock` en el scheduler de refresh del worker Art-Net, donde corresponde.

## No validado

- Suite CTest completa local: BLOCKED porque el entorno no incluye CMake.
- APP/VST3/AUv2 nativos: requieren CI Windows/macOS del nuevo SHA.
- Windows standalone crash y REAPER timeout: siguen FAILED hasta diagnóstico.
- Hardware Art-Net/nodo/PAR: NOT CONNECTED.

## Próximo bloque

1. Publicar un commit coherente y observar CI.
2. Resolver Windows standalone y host smoke sin suavizar gates.
3. Sustituir dependencia de `OnIdle` si el editor cerrado detiene runtime.
4. Implementar show-level bindings y authoring transaccional Store Look/Cue.
