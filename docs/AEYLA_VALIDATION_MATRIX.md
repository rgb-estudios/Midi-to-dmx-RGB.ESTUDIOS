# AEYLA — matriz de validación

Checkpoint: `CP-AEYLA-0.3.3`
Base del trabajo: PR #14; el SHA del checkpoint se completa al publicar.

| Área | Evidencia actual | Estado |
|---|---|---|
| Core portable previo | `core-ci` Linux/Windows/macOS verde en `4cede207` | **Implemented** en SHA previo |
| Quality previo | ASan/UBSan/TSan verde en `4cede207` | **Implemented** en SHA previo |
| Base CP-0.3.2 | quality/core/product/format CI verde en `703d4a0` | **Implemented** en CI |
| Project schema | v2 completo; migración explícita v1→v2: PASS local | **Scaffolded**; package/CI pendiente |
| show.bin | v1.1 Cue-owned MIDI; migración 1.0 ambigua falla cerrado: PASS local | **Scaffolded**; CI pendiente |
| Host component state | v1.1 con máximo 15 bindings Song→PPQ: PASS local | **Scaffolded**; host save/reopen pendiente |
| Host PPQ | start/before/in/after + authoring más allá del final: PASS local | **Scaffolded**; hosts reales pendientes |
| Runtime sin editor | worker independiente de `OnIdle` implementado en producto | **Scaffolded**; prueba editor cerrado pendiente |
| Offline render | disarm + blackout sostenido y rearmado manual | **Scaffolded**; host/backend pendientes |
| Windows VST3 + Validator previo | build/Validator habían pasado en SHA previo | **Simulated** en CI previo; revalidación pendiente |
| Windows standalone | dump previo: `glCreateProgram()` nulo; issue #17 | **Specified** diagnóstico; P0 abierto |
| REAPER Windows 7.78 | smoke previo sin resultado final verificable | **Specified** gate; pendiente |
| macOS VST3/AUv2 universal | build previo PASS; host smoke no concluyente | **Simulated** build; host pendiente |
| Ableton Windows/macOS | sin prueba real actual | **Specified** |
| Logic Pro AUv2 | sin prueba real actual | **Specified** |
| Art-Net worker | conectado a snapshot seguro del producto; config/model/transporte PASS local | **Scaffolded**; producto CI y nodo pendientes |
| Art-Net preflight | IPv4 unicast numérica, universo, socket y ownership | **Scaffolded**; no prueba recepción de nodo |
| Nodo + PAR físicos | sin evidencia | **Specified** |
| Show 15 Songs / soak | sin evidencia | **Specified** |

## Pruebas locales ejecutadas para CP-AEYLA-0.3.3

- `test_application_model`: PASS.
- `test_artnet_output_worker`: PASS.
- `test_host_song_binding`: PASS.
- `test_plugin_state`: PASS.
- `test_runtime_safety`: PASS.
- `test_project_document`: PASS.
- `test_show_program`: PASS.
- `test_show_program_codec`: PASS.
- standalone diagnostic `--self-test`: PASS.
- `git diff --check`: PASS durante la iteración.

No se ejecutó CTest completo local porque este entorno no contiene CMake ni la
dependencia miniz ya poblada. Package/file-controller y el producto iPlug2
quedan obligatoriamente sujetos a CI.

## Gates antes de Show Candidate

1. CI portable, quality y producto completos verdes en el mismo SHA.
2. Resolver Windows OpenGL #17 y obtener launch/close real.
3. Scan/load/save/reopen y editor-closed runtime en REAPER, Ableton y Logic.
4. Art-Net con nodo real, offline inhibit y watchdog/reachability de nodo.
5. Nodo/PAR reales, disconnect/reconnect, blackout y DMX byte a byte.
6. Soak mínimo 8 horas y tres ensayos completos del show.
