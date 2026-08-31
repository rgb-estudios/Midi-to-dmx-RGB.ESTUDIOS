# RGB Live Control R10.6 — Changelog PRETEST

Estado: **PRETEST_NOT_SHOW_CANDIDATE**

## Afinación R10.6

- Navegación canónica: `TOMA | EN VIVO | MIDI | SISTEMA`.
- RGB Estudios queda como identidad de producto; el nombre visible del show proviene del proyecto abierto.
- Proyectos nuevos usan identidad neutra (`Untitled Show`); AEYLA no se usa como nombre del software.
- Se eliminó copy heredado que anunciaba MTC: la sincronía implementada usa transporte/muestras del DAW y MIDI SHOW.
- EN VIVO mantiene una sola autoridad Art-Net y separa AL AIRE de PREPARADA.
- APAGÓN conserva ARM/carrier y transmite DMX 0 continuo; DESARMAR retira autoridad.
- SISTEMA bloquea selección, edición y aplicación de red mientras exista autoridad física.
- Después de aplicar una nueva red, APAGÓN permanece latched hasta liberación manual del operador.
- MIDI SHOW mantiene sus comandos operativos durante el show, pero congela modo, canal y Learn mientras exista carrier/autoridad.
- Al ARMAR se cancelan Learn MIDI pendientes para evitar remapeos accidentales con la primera nota del show.
- Se corrigió el test de identidad para esperar el nombre neutro del proyecto nuevo.

## Gate

Una build sólo puede llamarse R10.6 PRETEST cuando `core-ci` y `quality-ci` estén verdes. Show Ready requiere además prueba física Windows/REAPER + Avolites/nodo Art-Net y validación posterior macOS/Ableton.
