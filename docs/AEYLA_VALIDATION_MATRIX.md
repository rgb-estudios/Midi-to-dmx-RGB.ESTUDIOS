# AEYLA — matriz de validación

Checkpoint: `CP-AEYLA-0.3.1`  
Fuente: PR #14, HEAD previo `ebce96470c615cc550b9d4a0482e71e0e4d37c85`.

| Área | Evidencia actual | Estado |
|---|---|---|
| Core Linux/Windows/macOS | suite PASS en `83f4cd2`; macOS detectó luego tear de transporte, corregido y en revalidación | IMPLEMENTED / REVALIDATE |
| ASan/UBSan/TSan | suite completa PASS en `83f4cd2` | IMPLEMENTED / CI |
| Windows VST3 build | build PASS | CI-BUILT |
| Steinberg Validator Windows | PASS en HEAD previo | SIMULATED / CI |
| REAPER Windows 7.78 | proceso quedó abierto sin evidencia final | FAILED |
| Windows standalone | acceso inválido `0xC0000005` antes de ventana | FAILED |
| macOS VST3/AUv2 universal | build arm64+x86_64 PASS; corrección del gate `lipo` pendiente de reejecución | CI-BUILT / REVALIDATE |
| VST3/standalone proof Windows+macOS | PASS en `8c6a916` | IMPLEMENTED / CI |
| REAPER macOS | host smoke del nuevo SHA en ejecución | REVALIDATE |
| Ableton Windows/macOS | sin prueba real actual | NOT STARTED |
| Logic Pro AUv2 | sin prueba real actual | NOT STARTED |
| Host PPQ determinista | binding PASS; mailbox seqlock corregido, 201 campañas locales optimizadas PASS | UNIT-TESTED / REVALIDATE |
| UI con ventana cerrada | falta demostrar que runtime no depende de `OnIdle` del editor | BLOCKED P0 |
| Art-Net worker | unit/loopback software; no integrado al producto físico | SIMULATED / NOT CONNECTED |
| Nodo + PAR físicos | sin evidencia | NOT STARTED |
| Show 15 Songs / soak | sin evidencia | NOT STARTED |

## Gates para Show Candidate

1. CI portable completo verde en el mismo SHA.
2. Host smoke real en REAPER, Ableton y Logic según plataforma.
3. Runtime independiente de ventana y offline-render inhibit demostrado.
4. Flujo Song/Look/Cue/Binding funcional y persistente.
5. Art-Net preflight, ARM, Blackout, Disarm y shutdown validados.
6. Nodo y PAR reales con disconnect/reconnect y soak mínimo de 8 horas.
