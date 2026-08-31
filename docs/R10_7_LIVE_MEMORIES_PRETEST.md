# RGB Live Control R10.7 — EN VIVO / memorias renombrables PRETEST

Estado: PRETEST_NOT_SHOW_CANDIDATE. No mergear a main sin aprobación explícita.

## Alcance R10.7

R10.7 no modifica scheduler, carrier, APAGÓN, ARM, Take playback ni política de red. Hereda íntegramente los contratos de seguridad R10.5/R10.6.

Añade exclusivamente a EN VIVO:
- nombres de memoria editables y persistentes;
- capacidad de 1..8 memorias, con 4 iniciales por compatibilidad;
- páginas 1–4 y 5–8, manteniendo cuatro executors grandes visibles;
- botón + MEMORIA hasta un máximo de 8;
- `live.bin` v2 con nombre y cantidad activa;
- lectura backward-compatible de `live.bin` v1 de cuatro memorias;
- refinamiento visual de cards/fader sin alterar composición DMX.

## Compatibilidad

Un show R10.6 con `live.bin` v1 debe abrir con exactamente cuatro memorias y conservar los defaults históricos: FRONTAL, HUMO / HAZE, BASE BLANCA y TEST LUMINARIAS. El archivo no se reescribe a v2 hasta un Guardar explícito.

Un show guardado en R10.7 puede contener hasta ocho memorias y nombres personalizados de hasta 48 bytes UTF-8.

## Gates funcionales

1. Abrir un show R10.6: deben aparecer las cuatro memorias históricas y sus definiciones/MIDI.
2. Renombrar FRONTAL a `CONTRA VIOLINES`, guardar, cerrar REAPER y reabrir: el nombre debe persistir.
3. Pulsar + MEMORIA: crea MEMORIA 5, cambia a página 5–8 y abre su edición.
4. Renombrar MEMORIA 5 a `STROBE`, hacer Learn DMX y opcional MIDI Learn; guardar/reabrir: count, nombre y definición deben persistir.
5. Añadir hasta 8: + MEMORIA deja de añadir y muestra 8/8.
6. Cambiar 1–4 / 5–8 nunca toca ARM, APAGÓN, transporte ni nivel de memorias.
7. Una memoria activa/fundiendo sigue bloqueada para edición igual que R10.6.
8. APAGÓN TOTAL continúa imponiendo DMX 0 continuo con carrier/ARM conservados y memorias runtime OFF.
9. A AL AIRE → B PREPARADA → GO conserva reemplazo atómico sin blackout intermedio.
10. DESARMAR sigue siendo la única retirada voluntaria de autoridad.

## Gate visual

- Sólo cuatro memorias grandes visibles por página; nunca ocho tarjetas comprimidas.
- El nombre es la jerarquía principal de la tarjeta.
- El color se reserva para estado/selección, no para decoración.
- Fader HAZE/otros faders debe ser grueso, legible y con relleno violeta proporcional.
- EDITAR expone NOMBRE / DMX / MIDI / MODO / FADE; operación oculta esos controles.
