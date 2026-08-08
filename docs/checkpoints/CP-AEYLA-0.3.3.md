# CP-AEYLA-0.3.3 — Art-Net integrado + entrega REAPER verificable

- Fecha: 2026-08-08
- Repositorio: `rgb-estudios/Midi-to-dmx-RGB.ESTUDIOS`
- PR: #14
- Base CI verificada: `703d4a0568a0696eb1af736133a3592112b94e12`
- Estado físico: Art-Net **SOFTWARE CONNECTED**; hardware **NOT TESTED**

## Resultado funcional

- `OUTPUT SETUP` acepta `IPv4@universo` o `OFF` desde la UI.
- El proyecto persiste backend, destino y universo, y actualiza el patch completo
  de un universo como una sola transición segura.
- El worker Art-Net publica el último frame DMX a 40 FPS fuera del callback de
  audio; nunca acumula una cola de frames antiguos.
- La salida comienza desarmada. Configurar, abrir, deshabilitar, recargar,
  renderizar offline o detectar un error de envío fuerza un límite seguro.
- Dos instancias no pueden reclamar silenciosamente el mismo
  destino/puerto/universo dentro del mismo proceso.
- La UI muestra estado de backend y contadores de paquetes/errores.
- El smoke de REAPER se instala como `Scripts/__startup.lua`, ruta soportada
  por el host, para probar scan, instantiate, editor, save/reopen y editor
  restaurado.

## Validación local

Compilado con C++20, `-Wall -Wextra -Wpedantic -Werror`:

- `test_artnet_output_worker`: PASS;
- `test_application_model`: PASS;
- `git diff --check`: PASS durante la iteración.

El producto iPlug2, Validator, formatos y REAPER deben aprobar CI en el SHA
publicado. Este entorno local no dispone de CMake.

## Límites abiertos

1. El socket UDP listo no confirma que el nodo haya recibido ArtDMX; falta
   prueba con nodo/PAR nombrados y comparación DMX byte a byte.
2. No existe aún ArtPoll/heartbeat ni watchdog positivo de recepción del nodo.
3. Windows standalone mantiene el P0 OpenGL `glCreateProgram()`/issue #17.
4. Ableton Windows/macOS y Logic AUv2 requieren evidencia manual real.
5. Faltan soak de 8 horas y tres ensayos completos.

## Estado honesto

Este checkpoint busca ser un **REAPER Test Candidate** para programación y
prueba de red controlada. No es Hardware-tested, Show-tested ni Show Candidate.
