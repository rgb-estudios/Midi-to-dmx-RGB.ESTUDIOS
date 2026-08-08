# CP-AEYLA-0.3.2 — authoring slice + editor-independent runtime

- Fecha: 2026-08-08
- Repositorio: `rgb-estudios/Midi-to-dmx-RGB.ESTUDIOS`
- PR: #14
- Base inspeccionada: `4cede207b46143e6dd5140dd1da89450798f6aa0`
- Estado físico: Art-Net **NOT CONNECTED**; hardware **NOT TESTED**

## Resultado funcional

- Runtime musical/safety en worker de 4 ms independiente de `OnIdle`.
- Play/Seek/Loop reconstruidos desde PPQ absoluto y binding explícito por Song.
- Offline render fuerza disarm + blackout sostenido y nunca rearma solo.
- Project schema 2.0 con Look artístico completo y migración v1.
- `show.bin` 1.1 con MIDI propiedad de Cue y migración legacy fail-closed.
- UI mínima utilizable para paleta primaria/secundaria, Look Intensity,
  fixture mask, Store/prev/next Look, New/prev/next Song, Set Song Start y
  Store Cue.
- Store Cue puede extender el Song más allá de su longitud inicial.

## Validación local

Compilado con GCC 13.3, C++20, `-Wall -Wextra -Wpedantic -Werror`:

- application model: PASS;
- host-song binding: PASS;
- plugin state: PASS;
- runtime safety: PASS;
- project document/migration: PASS;
- show program: PASS;
- show codec/migration: PASS;
- standalone diagnostic `--self-test`: PASS;
- `git diff --check`: PASS durante la iteración.

CTest completo y el producto iPlug2 no pudieron compilarse localmente porque el
entorno no incluye CMake ni miniz poblado. Deben validarse en CI en el SHA
publicado.

## Auditoría y deuda abierta

### P0

1. Windows standalone: `glCreateProgram()` nulo / issue #17.
2. REAPER Windows/macOS: falta resultado verificable de scan/load/save/reopen.
3. Runtime con editor cerrado y offline render: fuente corregida, evidencia de
   host real pendiente.

### P1

1. Art-Net todavía no está conectado al producto ni a preflight/ownership.
2. No hay Show Mode, rename/delete/reorder, timeline, Cue editing, MOMENTARY
   visible, MIDI Learn manual, undo/redo ni autosave recovery.
3. Paleta limitada a ocho colores; no hay selector continuo.
4. No hay hardware, soak de 8 horas ni ensayo completo.

## Estado honesto

La entrega queda **Scaffolded** como flujo de autoría coherente y auditable.
No es Host-tested, Hardware-tested ni Show-tested y no debe distribuirse como
build de función.
