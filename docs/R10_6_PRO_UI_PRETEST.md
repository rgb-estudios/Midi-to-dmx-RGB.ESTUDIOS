# RGB Live Control R10.6 — Professional UI PRETEST

Estado: **PRETEST_NOT_SHOW_CANDIDATE**  
Base de seguridad: **R10.5 blackout authority contract**  
Alcance R10.6: interfaz, jerarquía visual e interacción. No redefine la autoridad Art-Net.

## 1. Arquitectura de navegación

La aplicación expone una sola navegación principal:

`TOMA | EN VIVO | MIDI | SISTEMA`

El shell canónico pertenece exclusivamente a `AeylaRuntimeStatusControl`.
`AeylaMainControl` no dibuja ni recibe eventos del header legado.

### Regla de autoridad

Cambiar de workspace es sólo presentación. Nunca puede:

- armar o desarmar Art-Net;
- activar o liberar APAGÓN TOTAL;
- detener el carrier;
- iniciar una toma;
- cambiar el estado AL AIRE.

## 2. Controles globales y autoridad

La cabecera contiene una única instancia de:

- `ARMAR / DESARMAR`;
- `APAGÓN TOTAL`;
- estado Art-Net;
- navegación principal.

No deben existir hit-zones duplicadas por debajo de la cabecera visible.

El botón global ARM representa la autoridad voluntaria visible. Si existe autoridad de TOMA o del modelo heredado, una pulsación de `DESARMAR` debe retirar todas las autoridades voluntarias representadas por ese botón; nunca puede mostrar `DESARMAR` y ejecutar una ruta incompatible de ARM.

APAGÓN/PANIC conserva el contrato R10.5: ARM y carrier permanecen activos y el worker transmite DMX 0 continuo con prioridad absoluta. DESARMAR es la única retirada voluntaria de autoridad.

### Veracidad de estado Art-Net

La UI distingue explícitamente:

- backend/red disponible;
- worker/motor listo;
- autoridad física/carrier activo (`enabled || override_enabled`);
- APAGÓN con carrier activo;
- fail-closed.

`ART-NET · LISTA / SIN CARRIER` nunca debe presentarse como `TX 44 Hz`. Header, footer y SISTEMA deben derivar la autoridad física de la misma fuente del worker.

## 3. TOMA

Prioridad visual:

1. canción seleccionada y estado de toma;
2. timeline/actividad DMX;
3. transporte REC / PLAY / STOP;
4. ENTRADA / SALIDA y herramientas de versión/zoom.

Identidad/selección utiliza violeta/cyan. El rojo queda reservado para REC, APAGÓN, errores y AL AIRE. La selección IN/OUT del timeline usa cyan; la indicación AL AIRE de la setlist usa rojo.

## 4. EN VIVO

La superficie de operación debe privilegiar reconocimiento sobre lectura.

- `AL AIRE`: tarjeta roja de alta prioridad.
- `PREPARADA`: tarjeta cyan diferenciada.
- transporte grande y centrado: PREV, PLAY/GO, HOLD, NEXT.
- PLAY/GO: verde.
- HOLD: ámbar.
- memorias: grilla 2 × 2.

Memorias por defecto:

1. FRONTAL — botón/toggle.
2. HUMO / HAZE — fader.
3. BASE BLANCA — botón/toggle.
4. TEST LUMINARIAS — botón/toggle.

La operación primaria muestra un pad ON/OFF o un fader real. `EDITAR` abre DMX Learn, MIDI Learn, modo y fade sólo para la memoria seleccionada.

Reglas adicionales:

- una memoria activa o en transición no puede entrar a edición hasta volver a OFF / 0%;
- pulsar el pad/fader de una memoria aún sin DMX aprendido abre su modo `EDITAR` y guía al Learn OFF → ON, en vez de ejecutar una acción inválida;
- controles locales antiguos de ARM/PANIC/cierre no forman parte de EN VIVO: la seguridad vive únicamente en el shell global.

## 5. MIDI y persistencia

`PREPARADA` usa cyan y `AL AIRE` usa rojo, igual que EN VIVO.

Un MIDI Learn completado en el runtime thread debe consolidar el estado persistente de memorias y marcar inmediatamente el proyecto como `SIN GUARDAR`. Guardar sigue siendo una acción explícita del usuario; la corrección sólo evita que el footer afirme falsamente `GUARDADO`.

## 6. ARCHIVO

`ARCHIVO` debe funcionar desde los cuatro workspaces, incluido EN VIVO.

Mientras el menú está abierto, el overlay posee toda la superficie de la aplicación:

- NUEVO / ABRIR / GUARDAR / GUARDAR COMO reciben el evento antes que cualquier workspace;
- hacer clic fuera cierra el menú y **consume ese clic**;
- el clic de cierre nunca puede atravesar el menú y disparar una memoria, fader, canción, timeline, transporte o control de SISTEMA.

## 7. Indicadores de operación

REC/PLAY pueden activar el marco periférico pulsante, pero no deben dibujar badges sobre la cabecera ni tapar navegación, Art-Net, ARM o APAGÓN.

Header, footer y SISTEMA deben ser coherentes entre sí:

- carrier activo: estado físico real del worker;
- backend disponible sin autoridad: `LISTA / SIN CARRIER`;
- APAGÓN: rojo y carrier conservado;
- fail-closed: rojo y rearme manual;
- AL AIRE: rojo;
- PREPARADA: cyan;
- reproducción normal: verde;
- HOLD/advertencia: ámbar.

## 8. Gate de interacción R10.6

La build no avanza a prueba física hasta validar:

1. TOMA → EN VIVO → MIDI → SISTEMA ×10 sin cambio de ARM ni carrier.
2. Una sola acción por clic sobre ARM y APAGÓN.
3. Si existe cualquiera de las autoridades voluntarias representadas por el header, `DESARMAR` las retira sin intentar una ruta incompatible de ARM.
4. APAGÓN activo mientras se cambia de workspace: sigue DMX 0 continuo y ARM permanece activo.
5. Header, footer y SISTEMA muestran carrier sólo cuando `enabled || override_enabled` es verdadero.
6. ARCHIVO abre y ejecuta GUARDAR desde EN VIVO.
7. Cerrar ARCHIVO tocando fuera en cualquiera de los cuatro workspaces no dispara ningún control inferior.
8. AL AIRE y PREPARADA permanecen visualmente distinguibles en 1280×800 y en el layout compacto.
9. FRONTAL/BASE/TEST muestran pad ON/OFF grande.
10. HUMO/HAZE muestra fader manipulable y porcentaje legible.
11. EDITAR no cambia nivel DMX por sí mismo y está bloqueado mientras la memoria esté activa/transicionando.
12. Una memoria sin DMX aprendido guía a EDITAR/Learn y no intenta operar salida.
13. Un MIDI Learn exitoso cambia el footer a `SIN GUARDAR` sin auto-guardar.
14. REC/PLAY no ocultan ni interceptan cabecera.
15. APAGÓN/PANIC/DESARMAR conservan los tests de autoridad R10.5.
16. Carga/guardado `.aeylashow` no restaura niveles activos ni ARM.

## 9. Criterio de entrega

CI verde significa candidato **PRETEST**, no Show Ready. La aprobación para show requiere prueba física Windows/REAPER + Avolites/nodo Art-Net y posteriormente validación macOS/Ableton.
