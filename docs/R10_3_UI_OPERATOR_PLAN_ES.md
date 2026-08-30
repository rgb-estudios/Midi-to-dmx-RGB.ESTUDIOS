# R10.3 — reparación UI / operación EN VIVO

Estado: PRETEST / no Show Ready.

## Evidencia de campo

Revisión basada en el video real del plugin en REAPER del 30-08-2026.

Problemas observados:

1. EN VIVO se percibe como una pantalla paralela/modal y no como parte natural de una sola aplicación.
2. El área normal sigue acumulando demasiados estados, botones de archivo y navegación en paralelo.
3. Las memorias EN VIVO exponen simultáneamente DMX, modo, fade y MIDI; durante operación esto agrega ruido.
4. `DMX` no comunica el flujo real de dos snapshots Avolites.
5. MIDI Learn queda bloqueado hasta aprender DMX, aunque el mapeo MIDI es configuración independiente.
6. Los mensajes de estado quedan demasiado lejos del control que originó la acción.

## Contrato R10.3

### Navegación

Presentación única y consistente:

`TOMA | EN VIVO | MIDI | SISTEMA`

No duplicar EN VIVO como acción administrativa junto a Guardar.

### EN VIVO — modo operación

Cada memoria muestra sólo:

- nombre;
- estado DMX (`SIN DMX` o `N CH`);
- binding MIDI (`SIN MIDI`, `NOTE`, `CC`);
- control principal real: pad o fader;
- botón discreto `CONFIGURAR`.

Los controles de autoría no ocupan espacio durante el show.

### EN VIVO — configurar una memoria

Al entrar a CONFIGURAR se sustituye temporalmente el control principal por:

- DMX Learn guiado;
- MIDI Learn;
- BOTÓN/FADER;
- FADE;
- VOLVER A OPERACIÓN.

Nunca se despliegan los cuatro paneles de configuración al mismo tiempo.

### DMX Learn

Texto inequívoco:

- antes: `1/2 CAPTURAR OFF`;
- tras baseline: `2/2 CAPTURAR ON`;
- completado: `DMX OK · N CH`;
- error sin diferencias: conservar paso 1 y explicar que debe encenderse sólo la memoria objetivo en Avolites.

### MIDI Learn

Debe poder aprenderse antes o después de DMX.

- BOTÓN → Note On;
- FADER → CC continuo;
- primer gesto sólo asigna, no ejecuta;
- siguiente gesto opera;
- SHOW/PANIC conserva prioridad;
- sin DMX configurado, un control MIDI aprendido se consume pero no genera salida física.

### Archivo

El footer normal debe priorizar estado y no una fila permanente de cinco botones. La dirección es una acción `ARCHIVO` que agrupe Nuevo/Abrir/Guardar/Guardar como.

### Estados globales

- REC: marco rojo pulsante, prioridad máxima;
- PLAY: marco verde pulsante sólo cuando no hay REC;
- violeta claro: selección, configuración y Learn;
- PANIC/APAGÓN: rojo exclusivo de seguridad;
- ARM: estado explícito separado de PLAY.

## Gates

- MIDI Learn sin DMX configurado;
- MIDI Learn no destructivo;
- DMX Learn 1/2 y 2/2;
- memoria sin DMX no puede emitir aunque tenga MIDI;
- 960x620 y 1280x800 sin clipping;
- Windows graphical startup;
- REAPER host smoke;
- carrier y Take gates sin regresión.
