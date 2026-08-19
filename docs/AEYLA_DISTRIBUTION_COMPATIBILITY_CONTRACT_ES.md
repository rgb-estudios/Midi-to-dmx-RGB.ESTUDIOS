# AEYLA — Contrato de distribución y compatibilidad

Estado: DESIGN LOCK / QA CONTRACT
Producto: AEYLA Visual DMX / AEYLA Show Player
Plataformas: Windows + macOS
Objetivo: una única base de producto con comportamiento funcional equivalente en hosts soportados.

## 1. Regla principal

No se acepta la frase "compatible" porque un bundle compile, un instalador termine o el plugin aparezca en un scanner.

Cada combinación SO / arquitectura / formato / DAW debe mantenerse en uno de estos estados:

- NOT TESTED
- BUILD PASS
- FORMAT VALIDATED
- INSTALL PASS
- HOST LOAD PASS
- FUNCTIONAL PASS
- SHOW QA PASS
- BLOCKED / FAILED

Sólo `SHOW QA PASS` autoriza uso de esa combinación en performance.

## 2. Formatos congelados

### Windows

- Formato de distribución principal: VST3 64-bit.
- Arquitectura objetivo V1: x86_64.
- Destino del instalador: `C:\Program Files\Common Files\VST3\AeylaVisualDmx.vst3`.
- Hosts objetivo: Ableton Live y REAPER.
- No AU en Windows.
- Standalone no es requisito para el show y no debe bloquear el VST3 cuando su fallo sea aislado del plugin.

### macOS

Se distribuyen ambos formatos desde la misma base funcional:

- VST3 Universal `arm64 + x86_64` para Ableton Live y REAPER.
- AUv2 Universal `arm64 + x86_64` para Logic Pro y como ruta alternativa en hosts compatibles.

Destinos:

- `/Library/Audio/Plug-Ins/VST3/AeylaVisualDmx.vst3`
- `/Library/Audio/Plug-Ins/Components/AeylaVisualDmx.component`

Logic Pro se valida mediante AUv2; no se declara soporte Logic mediante VST3.

## 3. Compatibilidad Mac antiguo / nuevo

El binario macOS debe ser Universal 2 y contener obligatoriamente:

- `x86_64` para Mac Intel.
- `arm64` para Apple Silicon.

Deployment target del producto V1: **macOS 11.0 Big Sur**.

Esto define el piso del plugin, no garantiza que cualquier versión histórica o futura de un DAW funcione en macOS 11. Cada host mantiene sus propios requisitos de SO.

No subir el deployment target por comodidad de CI o tooling sin una decisión de producto documentada.

No bajar de macOS 11.0 sin reauditar dependencias, `std::filesystem`, iPlug2, SDK, host matrix y pruebas reales.

## 4. Matriz de hosts objetivo

### Windows x64

#### REAPER

Formato: VST3.

Gate mínimo:

- scan;
- instantiate;
- editor open/close/reopen;
- MIDI input;
- DAW transport;
- play/pause/stop;
- seek;
- loop;
- save project;
- close/reopen;
- state restore;
- capture/playback DMX;
- Art-Net output;
- editor cerrado durante reproducción;
- offline render inhibit;
- clean shutdown.

#### Ableton Live

Formato: VST3.

Mismos gates que REAPER, más:

- carga en una pista `AEYLA CONTROL`;
- recepción de triggers MIDI desde clips;
- transport/sample-position binding;
- cambio y repetición de Songs;
- session reopen;
- background plug-in scan/reload.

### macOS Intel

#### Ableton Live

Formato principal: VST3 x86_64 dentro del bundle Universal.

#### REAPER

Formato principal de QA: VST3 x86_64 dentro del bundle Universal.

#### Logic Pro

Formato: AUv2 x86_64 dentro del component Universal.

### macOS Apple Silicon

#### Ableton Live

Formato principal: VST3 arm64 nativo dentro del bundle Universal.

No depender de Rosetta para la ruta soportada.

#### REAPER

Formato principal de QA: VST3 arm64 nativo.

#### Logic Pro

Formato: AUv2 arm64 nativo.

Debe pasar `auval` y el scanner real de Logic.

Rosetta puede usarse sólo como prueba de compatibilidad adicional, nunca como requisito de la ruta principal Apple Silicon.

## 5. Instaladores

### Windows

Formato: instalador `.exe` generado con Inno Setup.

Debe:

- instalar únicamente el bundle VST3 de AEYLA y archivos auxiliares propios;
- usar el directorio VST3 estándar del sistema;
- incluir versión y SHA de origen;
- permitir instalación silenciosa para QA;
- incluir desinstalador;
- no borrar `.aeylashow`, sesiones del DAW ni contenido del usuario al desinstalar;
- comprobar por CI que el hash del binario instalado coincide con el staged build.

Release pública:

- Authenticode válido;
- timestamp;
- firma verificable antes de publicación.

Alpha interna puede permanecer unsigned, pero debe estar rotulada explícitamente `UNSIGNED / TEST ONLY`.

### macOS

Formato: `.pkg` Universal con VST3 + AUv2.

Debe:

- instalar ambos bundles en las rutas estándar `/Library/Audio/Plug-Ins/...`;
- verificar que VST3 y AUv2 contienen `arm64 + x86_64`;
- ejecutar Steinberg Validator para VST3;
- ejecutar Apple `auval` para AUv2;
- instalar/desinstalar en smoke CI sin residuos del bundle;
- conservar `.aeylashow` y datos del usuario al desinstalar.

Release pública:

- firma Developer ID de todos los ejecutables/bundles;
- Hardened Runtime cuando corresponda;
- firma Developer ID Installer del package;
- notarización Apple mediante `notarytool`/servicio vigente;
- stapling y verificación Gatekeeper antes de publicar.

Ad-hoc signing sólo sirve para CI interna y nunca equivale a release firmada/notarizada.

## 6. Regla de fuente única

VST3 Windows, VST3 macOS y AUv2 macOS deben utilizar el mismo:

- ApplicationModel;
- show/setlist/song/take model;
- recorder/playback engine;
- host transport abstraction;
- MIDI mapping;
- Art-Net output worker;
- persistence/state format;
- safety state;
- UI product model.

No crear forks funcionales específicos por host salvo adapters estrictamente necesarios.

## 7. Runtime y thread safety

Nunca ejecutar desde audio callback:

- socket/network send/receive;
- file I/O;
- allocation no acotada;
- locks bloqueantes;
- video decode;
- dialogs;
- installer/maintenance operations.

El callback del host sólo publica estado temporal/eventos acotados hacia estructuras realtime-safe.

Recorder, playback scheduler, archivos y Art-Net viven fuera del audio thread.

La reproducción del show debe continuar correctamente con la ventana/editor del plugin cerrada.

Offline render del DAW debe inhibir cualquier salida Art-Net física.

## 8. Estado de salida

Una sola instancia del producto debe poseer la salida Art-Net del show.

Al crear/reabrir una instancia:

- OUTPUT = DISARMED;
- BLACKOUT/safe state según contrato vigente;
- nunca restaurar ARM automáticamente desde estado persistido.

Cambios de show, endpoint crítico o modo de render deben forzar una transición segura definida.

## 9. QA mínimo por release candidate

Para cada plataforma soportada:

1. build clean;
2. formato validado;
3. installer clean install;
4. scan host;
5. instantiate;
6. editor interactions;
7. save/reopen;
8. play/pause/stop/seek/loop;
9. MIDI trigger;
10. Song switching;
11. capture DMX desde Avolites;
12. replay de Take;
13. Art-Net TX físico;
14. pérdida/reconexión de NIC/node;
15. editor cerrado;
16. offline render inhibit;
17. clean shutdown;
18. full-show repeat;
19. soak prolongado;
20. MASTER + BACKUP + STABLE verificados.

## 10. Compatibilidad no prometida

No se promete "compatibilidad absoluta" con:

- todas las versiones históricas de macOS;
- todos los DAWs existentes;
- versiones de host fuera de la matriz validada;
- Windows 32-bit;
- plugins/hosts no probados;
- sistemas donde el DAW mismo no soporte el SO/arquitectura.

La promesa correcta del producto es:

> AEYLA mantiene una matriz explícita de plataformas y hosts soportados, con builds universales donde corresponde y evidencia reproducible por combinación. Una combinación sólo se declara soportada después de pasar sus gates reales.

## 11. Matriz objetivo V1

| Plataforma | Arquitectura | Formato | Host | Objetivo |
|---|---|---|---|---|
| Windows 10/11 | x86_64 | VST3 | REAPER | REQUIRED |
| Windows 10/11 | x86_64 | VST3 | Ableton Live | REQUIRED |
| macOS 11+ | Intel x86_64 | VST3 | REAPER | REQUIRED |
| macOS 11+ | Intel x86_64 | VST3 | Ableton Live | REQUIRED |
| macOS 11+ | Intel x86_64 | AUv2 | Logic Pro compatible con ese SO | REQUIRED |
| macOS 11+ | Apple Silicon arm64 | VST3 | REAPER | REQUIRED |
| macOS 11+ | Apple Silicon arm64 | VST3 | Ableton Live | REQUIRED |
| macOS 11+ | Apple Silicon arm64 | AUv2 | Logic Pro | REQUIRED |

## 12. Gate de release

No publicar como `SHOW READY` mientras alguna combinación REQUIRED del entorno real de AEYLA permanezca en NOT TESTED / FAILED / BLOCKED.

La firma, el instalador y la estética del paquete no sustituyen validación funcional en host ni prueba física Art-Net/DMX.
