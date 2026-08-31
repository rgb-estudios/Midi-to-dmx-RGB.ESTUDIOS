# RGB Live Control R10.6 — Professional UI PRETEST

Estado: **PRETEST_NOT_SHOW_CANDIDATE**  
Base de seguridad: **R10.5 blackout authority contract**  
Alcance R10.6: interfaz, jerarquía visual e interacción. No redefine la autoridad Art-Net.

## 1. Arquitectura de navegación

La aplicación expone una sola navegación principal:

`TOMA | EN VIVO | MIDI | SISTEMA`

El shell canónico pertenece exclusivamente a `AeylaRuntimeStatusControl`.
`AeylaMainControl` no dibuja ni recibe eventos de un header legado.

### Regla de autoridad

Cambiar de workspace es sólo presentación. Nunca puede:

- armar o desarmar Art-Net;
- activar o liberar APAGÓN TOTAL;
- detener el carrier;
- iniciar una toma;
- cambiar el estado AL AIRE.

## 2. Controles globales

La cabecera contiene una única instancia de:

- `ARMAR / DESARMAR`;
- `APAGÓN TOTAL`;
- estado Art-Net;
- navegación principal.

No deben existir hit-zones duplicadas por debajo de la cabecera visible.

APAGÓN/PANIC conserva el contrato R10.5: ARM y carrier permanecen activos y el worker transmite DMX 0 continuo con prioridad absoluta. DESARMAR es la única retirada voluntaria de autoridad.

## 3. TOMA

Prioridad visual:

1. canción seleccionada y estado de toma;
2. timeline/actividad DMX;
3. transporte REC / PLAY / STOP;
4. ENTRADA / SALIDA y herramientas de versión/zoom.

Identidad/selección utiliza violeta/cyan. El rojo queda reservado para REC, APAGÓN, errores y AL AIRE.

## 4. EN VIVO

La superficie de operación debe privilegiar reconocimiento sobre lectura.

- `AL AIRE`: tarjeta roja de alta prioridad.
- `PREPARADA`: tarjeta cyan diferenciada.
- transporte: PREV, PLAY/GO, HOLD, NEXT.
- PLAY/GO: verde.
- HOLD: ámbar.
- memorias: grilla 2 × 2.

Memorias por defecto:

1. FRONTAL — botón/toggle.
2. HUMO / HAZE — fader.
3. BASE BLANCA — botón/toggle.
4. TEST LUMINARIAS — botón/toggle.

La operación primaria muestra un pad ON/OFF o un fader real. `EDITAR` abre DMX Learn, MIDI Learn, modo y fade sólo para la memoria seleccionada.

## 5. ARCHIVO

`ARCHIVO` debe funcionar desde los cuatro workspaces, incluido EN VIVO.

Cuando el menú está abierto:

- NUEVO / ABRIR / GUARDAR / GUARDAR COMO reciben el evento antes que la superficie EN VIVO;
- hacer clic fuera cierra el menú y **consume ese clic**;
- el clic de cierre nunca puede atravesar el menú y disparar una memoria, fader, canción o transporte.

## 6. Indicadores de operación

REC/PLAY pueden activar el marco periférico pulsante, pero no deben dibujar badges sobre la cabecera ni tapar navegación, Art-Net, ARM o APAGÓN.

El footer muestra estado del proyecto y estado operativo sin duplicar controles globales.

## 7. Gate de interacción R10.6

La build no avanza a prueba física hasta validar:

1. TOMA → EN VIVO → MIDI → SISTEMA ×10 sin cambio de ARM ni carrier.
2. Una sola acción por clic sobre ARM y APAGÓN.
3. APAGÓN activo mientras se cambia de workspace: sigue DMX 0 continuo y ARM permanece activo.
4. ARCHIVO abre y ejecuta GUARDAR desde EN VIVO.
5. Cerrar ARCHIVO tocando fuera no dispara ningún control de EN VIVO.
6. AL AIRE y PREPARADA permanecen visualmente distinguibles en 1280×800 y en el layout compacto.
7. FRONTAL/BASE/TEST muestran pad ON/OFF grande.
8. HUMO/HAZE muestra fader manipulable y porcentaje legible.
9. EDITAR no cambia nivel DMX por sí mismo.
10. REC/PLAY no ocultan ni interceptan cabecera.
11. APAGÓN/PANIC/DESARMAR conservan los tests de autoridad R10.5.
12. Carga/guardado `.aeylashow` no restaura niveles activos ni ARM.

## 8. Criterio de entrega

CI verde significa candidato **PRETEST**, no Show Ready. La aprobación para show requiere prueba física Windows/REAPER + Avolites/nodo Art-Net y posteriormente validación macOS/Ableton.
