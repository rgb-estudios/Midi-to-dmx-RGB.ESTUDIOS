# RGB Live Control — R10.5 PRETEST

Estado: PRETEST / NO SHOW READY / NO MERGE A MAIN.

## 1. Autoridad física

- ARMAR obtiene y mantiene autoridad Art-Net.
- DESARMAR retira autoridad física y deja una ráfaga corta de seguridad.
- APAGÓN TOTAL NO es DESARMAR.
- Con ARM activo, APAGÓN TOTAL mantiene el socket/carrier y transmite DMX 0 de forma continua a la cadencia configurada (~44 Hz).
- APAGÓN TOTAL tiene prioridad absoluta sobre Take, HOLD y memorias EN VIVO.
- Liberar APAGÓN TOTAL revela el estado subyacente sin exigir un segundo ARM.
- Cambio de pestaña, ventana, PREPARADA, PREV/NEXT, PLAY/PAUSA/HOLD/STOP no puede retirar ARM ni activar APAGÓN.
- Backend fault, render offline, runtime fault, shutdown y cambio físico TX conservan fail-closed.

## 2. Navegación

Cabecera canónica única:

TOMA | EN VIVO | MIDI | SISTEMA

La selección de workspace es sólo presentación. Nunca modifica ARM, APAGÓN ni transporte.

ARMAR/DESARMAR y APAGÓN TOTAL permanecen visibles en la misma ubicación en los cuatro workspaces.

## 3. EN VIVO

- PREPARADA y AL AIRE permanecen separados.
- Una canción PREPARADA sólo sustituye AL AIRE mediante GO/PLAY explícito.
- El cambio A -> B debe usar reemplazo atómico sin retirar autoridad ni emitir blackout intermedio.
- Memorias operativas permanecen visibles como botón/fader; Learn, modo y fade aparecen sólo al CONFIGURAR una memoria.

## 4. Gate físico obligatorio

1. ARMAR una vez con DMX no-cero.
2. Activar APAGÓN TOTAL: el receptor debe seguir recibiendo ~44 Hz y todos los slots deben ser 0; ARM debe seguir activo.
3. Liberar APAGÓN: debe reaparecer el estado subyacente sin rearmar.
4. Cambiar TOMA/EN VIVO/MIDI/SISTEMA al menos 10 veces: sin caída de carrier, sin DISARM y sin APAGÓN accidental.
5. A AL AIRE -> B PREPARADA -> GO: B entra sin corte/blackout.
6. DESARMAR: se retira autoridad física.
7. Re-ARM y PANIC/APAGÓN: debe imponerse de inmediato.

CI verde no sustituye esta prueba física.
