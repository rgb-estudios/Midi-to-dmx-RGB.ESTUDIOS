# Handoff para IA paralela y futuros agentes

## Repositorio autorizado

Trabajar exclusivamente en:

```text
https://github.com/rgb-estudios/Midi-to-dmx-RGB.ESTUDIOS
```

No modificar `bees-honey-rgb-tour-manager` ni `rgb-estudios-web` para este proyecto.

## Contexto del producto

AEYLA Visual DMX convierte fuentes visuales de baja resolución —colores, gradientes, animaciones, imágenes y videos cortos— en atributos semánticos de iluminación. El usuario edita sin Ableton mediante una aplicación standalone gratuita. El computador final carga un runtime VST3 en Ableton y activa executors mediante notas MIDI. El runtime no analiza audio.

Salidas planificadas:

- Art-Net, primero unicast y luego broadcast.
- DMX USB Pro y compatibles declarados.
- Open DMX/FTDI como backend separado y con advertencias de timing.

Fixtures iniciales:

- 10 PAR físicos, 14 posiciones lógicas.
- Atributos mínimos: dimmer, shutter, strobe, red, green, blue, white, amber, UV, lime, macro, speed, reset y zoom.
- Hazer/fan y strobes independientes.

## Lectura obligatoria antes de editar

1. `AGENTS.md`
2. `docs/RESUMEN_MAESTRO_ES.md`
3. `docs/ARCHITECTURE.md`
4. `docs/PRODUCT_SPEC.md`
5. `docs/VISUAL_DESIGN_SYSTEM.md`
6. `docs/DEVELOPMENT_STATUS.md`
7. `docs/BACKLOG.md`
8. Especificación del subsistema afectado.

## Invariantes no negociables

- Los looks emiten atributos semánticos; nunca escriben canales DMX absolutos.
- Cambiar modelo, modo, dirección o backend no puede obligar a reprogramar looks, escenas, executors o MIDI.
- Standalone y VST3 usan exactamente el mismo motor y archivo `.aeylashow`.
- Art-Net, USB, decodificación de video y archivos no pueden bloquear el hilo de audio del host.
- Salida inicia desarmada; dimmer, strobe, haze, macro y reset parten seguros.
- El aspecto visual es requisito funcional: canvas dominante, jerarquía clara, feedback inmediato y sin apariencia de panel administrativo genérico.
- No declarar hardware como compatible sin evidencia con modelo, driver, OS y condiciones.

## Estado real al recibir este handoff

Implementado y probado automáticamente:

- Núcleo C++ de atributos semánticos.
- Transformación RGB→RGBWALUV inicial.
- Perfil de fixture y reordenamiento de canales.
- Compilador de universo DMX.
- Rig lógico 10/14.
- Codificador ArtDMX, validado solo con paquetes simulados.
- Prototipo visual HTML/CSS/JS navegable.
- Esquemas JSON iniciales.
- CI CMake en Windows y Linux.

No implementado todavía:

- Envío UDP Art-Net real.
- Runtime de escenas/executors.
- Carga/guardado `.aeylashow`.
- Standalone nativo.
- VST3.
- Decodificación real de imagen/video.
- USB-DMX.
- Pruebas con hardware o Ableton.

## Primera tarea recomendada

Implementar `P5-001` a `P5-005`: backend Art-Net UDP unicast con thread de salida aislado.

Criterios mínimos:

1. API de backend independiente del motor artístico.
2. Selección explícita de IP destino, universo y refresco.
3. Copia thread-safe del último frame DMX.
4. Ninguna llamada de socket desde la ruta de render/host.
5. Secuencia ArtDMX correcta y estable.
6. Shutdown seguro y sin deadlock.
7. Tests unitarios con socket/transport inyectable o fake.
8. Captura de paquete reproducible; hardware sigue marcado `Simulated`, no `Hardware-tested`.
9. Actualizar `CHANGELOG.md`, `DEVELOPMENT_STATUS.md` y documentación afectada.

## Flujo de trabajo esperado

- Crear rama `agent/<scope>`.
- Antes de cambiar código, resumir comprensión y riesgos.
- Hacer cambios pequeños y auditables.
- Ejecutar:

```bash
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
python tools/validate_json.py
```

- Para UI: probar 1366×768 y 1920×1080 y adjuntar capturas.
- Abrir draft PR con alcance, impacto, pruebas y límites honestos.
- No mezclar múltiples epics salvo que exista una dependencia técnica directa.

## Formato obligatorio de cierre de tarea

```text
Status: Specified | Scaffolded | Implemented | Simulated | Hardware-tested | Show-tested
Scope:
Files changed:
Tests run:
Evidence:
Safety impact:
Known limitations:
Recommended next task:
```

## Prohibiciones

- No reemplazar el motor semántico por mapeo de slots directo.
- No vender el prototipo web como standalone terminado.
- No afirmar que un paquete ArtDMX equivale a salida Art-Net real.
- No integrar análisis de audio.
- No añadir una librería universal de fixtures en esta fase.
- No introducir dependencias sin versión, licencia y estrategia de actualización.
- No incluir medios privados en el repositorio público.
