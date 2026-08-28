# AEYLA — matriz de validación

Fecha: 28 agosto 2026

Base auditada: `efa11df` (contenido equivalente al HEAD local previo
`f9228e4`). El `BUILD_ID` del paquete publicado es la autoridad para
identificar el checkpoint exacto de esta auditoría.

Los estados permitidos son: **Specified**, **Scaffolded**, **Implemented**,
**Simulated**, **Host-tested**, **Hardware-tested** y **Show-tested**.

| Área | Evidencia vigente | Estado exacto |
|---|---|---|
| Core portable | `core-ci` verde en Linux, Windows y macOS para `efa11df` | **Simulated** |
| Quality | quality/sanitizers verdes para `efa11df` | **Simulated** |
| Suite base | 27 targets CTest en la base publicada | **Simulated** |
| Auditoría portable actual | 28/28 ejecutables estrictos locales PASS, incluidos sesión, archivo exacto, recuperación de cierre y loopback | **Simulated** localmente en Linux |
| Proyecto | `project.json` 2.0; carga transaccional y guardado atómico | **Simulated** |
| Show binario | `show.bin` 1.1 | **Simulated** |
| Captura Art-Net | bind RX, muestreo 44 Hz, escritura directa a disco | **Simulated**; usuario informó captura real correcta |
| Referencia REPRODUCIR/MTC | flanco de transporte → ENTRADA no destructiva; pruebas de estado | **Simulated** |
| Editor | actividad, cabezal, ENTRADA/SALIDA, versiones y consolidación | **Simulated**; interacción formal pendiente |
| Geometría UI auditada | 960×620, 1024×694/700/710, 1280×800, 1440×900, 1920×1200 y 2560×1600 sin cruces calculados | **Simulated** localmente; render host pendiente |
| TX Art-Net | worker 44 Hz, autoridad separada, ráfaga blackout, loopback | **Simulated**; usuario informó TX real correcto |
| Cierre durante captura | `stop/unload` finaliza una toma streamed recuperable con cuadro final validado | **Simulated** localmente en Linux |
| Fail-closed | 3 errores, desarme, apagón y rearme únicamente explícito | **Simulated** localmente; inyección/host pendiente |
| Red Windows | helper UAC, alias secundario, validación y rollback compilados | **Simulated** |
| VST3 Windows | build, PE, formatos, validadores e instaladores PASS en CI | **Simulated** |
| Standalone Windows | ejecutable existe pero cae `0xC0000005` en NanoVG/GL2 | **Scaffolded**; P0 abierto |
| UI macOS | aplicación gráfica compila en runner; el workflow no la abre ni captura interacción | **Simulated** |
| REAPER Windows/macOS automático | harness existe pero termina por timeout sin evidencia terminal | **Scaffolded** |
| REAPER manual | reporte del usuario: graba y transmite | evidencia útil, no **Host-tested** formal |
| Ableton Windows/macOS | sin ejecución documentada | **Specified** |
| Logic Pro AUv2 | sin ejecución documentada | **Specified** |
| Nodo/PAR físico | nodo/modelo, bytes DMX y logs no documentados | **Specified** |
| Soak / ensayo | sin 8 horas ni tres ensayos | **Specified** |

## Diferencias de plataforma y formato

- Windows aplica IPv4 mediante un helper UAC separado; macOS no posee aún esa
  mutación privilegiada desde AEYLA.
- APP Windows conserva el crash gráfico conocido. El VST3 usa el mismo motor,
  pero su build/validator no demuestra interacción dentro del host.
- VST3 y AUv2 comparten motor de proyecto/captura/reproducción; cada formato
  requiere evidencia de host propia.

## Riesgos P0/P1 abiertos

1. Windows standalone/NanoVG `0xC0000005`.
2. REAPER/Ableton/Logic reales, reapertura, minimización y cierre de ventana.
3. UAC, preservación y rollback de red en Windows físico.
4. Nodo real, desconexión/reconexión y confirmación de recepción; `sendto()` no
   prueba que el cable o nodo estén presentes.
5. Primera construcción del mapa de actividad de una toma larga todavía lee el
   archivo fuera del hilo de audio, pero puede ocupar temporalmente el hilo UI.
6. MIDI Learn/transporte de show aún no integrado.
7. Soak de 8 horas y Show-tested.

## Dependencias y migración

- iPlug2: `584df5a3306f3a9a62b5ebb803d3fb58134abdcf`.
- VST3 SDK: `9fad9770f2ae8542ab1a548a68c1ad1ac690abe0`.
- miniz: `77d0dce8627735138c51770d1799a1ef48f2117d`.
- Esta auditoría no agrega dependencias ni modifica licencias.
- No cambia `.aeylashow`, `project.json`, `show.bin` ni `.aeylatake`.
- El estado de sesión/UI no se persiste como autoridad física.

## Gates antes de Show Candidate

1. Toda la CI verde en el mismo SHA y paquete Windows ligado a ese SHA.
2. Pruebas manuales repetibles en REAPER, Ableton y Logic.
3. Hardware real con blackout, pérdida/reconexión y DMX comprobado.
4. UI completa a tamaño mínimo y nominal en Windows/macOS.
5. Soak de 8 horas y tres ensayos del show.
