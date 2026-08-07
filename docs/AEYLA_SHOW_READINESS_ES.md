# AEYLA Visual DMX — objetivo operativo y criterios de show

## Propósito vinculante

AEYLA Visual DMX no es un experimento genérico de MIDI ni una interfaz de
previsualización. Su primera entrega debe operar el show real de **Aeyla**, de
hasta 45 minutos, coordinando iluminación, escenas visuales y automatización
MIDI-DMX con comportamiento determinista, recuperación segura y documentación
suficiente para ensayo, montaje y función.

La misma lógica de proyecto y reproducción debe funcionar en:

1. standalone Windows;
2. standalone macOS;
3. VST3 dentro de Ableton Live en Windows;
4. VST3 dentro de Ableton Live en macOS Apple Silicon.

El archivo `.aeylashow` es la fuente de verdad portable. APP y VST3 no pueden
mantener versiones distintas del show.

## Flujo de trabajo que debe resolver

### 1. Preparación del rig

- Crear o importar perfiles de fixtures por atributos semánticos.
- Definir orden físico de canales por modo de personalidad.
- Patch de universo y dirección con detección de colisiones.
- Posicionar los 14 puntos lógicos y seleccionar Rig 10 o Rig 14.
- Configurar Art-Net y verificar nodo, universo, red y frecuencia de salida.
- Mantener Output Arm apagado hasta completar validaciones.

### 2. Creación artística

- Crear looks reutilizables sin escribir slots DMX directamente.
- Crear escenas con nombre, look, blackout y tiempos de entrada/salida.
- Organizar canciones en orden de show.
- Dibujar bloques MIDI en una grilla musical con compás, tempo, PPQ, zoom,
  snapping, duración, nota, canal y velocity.
- Visualizar claramente qué escena produce cada nota.
- Permitir editar, duplicar, mover, redimensionar y borrar bloques.
- Importar y exportar archivos MIDI estándar para trabajar con Ableton.
- Previsualizar el resultado sin habilitar salida física.

### 3. Ensayo

- Reproducir una canción desde cualquier posición.
- Saltar a una escena sin dejar notas o ejecutores colgados.
- Detener transporte y publicar un estado seguro definido.
- Mostrar playhead, escena activa, siguiente escena y tiempo restante.
- Ejecutar escenas manualmente para correcciones y contingencia.
- Registrar drops MIDI, fallos de red, frames DMX y restauraciones rechazadas.
- Guardar automáticamente una recuperación sin reemplazar el proyecto válido.

### 4. Operación de show

- Modo Show bloqueado contra ediciones accidentales.
- Blackout siempre visible y accionable.
- Armado deliberado mediante gesto explícito posterior a cada carga/reinicio.
- Salida Art-Net en worker separado del callback y del render.
- Watchdog de backend: pérdida de red/nodo fuerza disarm y estado seguro.
- Controles manuales de escena siguiente/anterior, go, stop y blackout.
- La ventana puede cerrarse sin detener MIDI, runtime ni salida.
- Restauración de proyecto/Set nunca reactiva Output Arm.
- Backup operativo documentado y comprobado antes del show.

## Modelo de show inicial

La versión 0.3 usa una **pista única de escenas** por canción:

- una escena activa a la vez;
- los bloques pueden tocarse, pero no solaparse;
- Note Off se procesa antes que Note On cuando comparten tick;
- la duración del bloque MIDI define la duración artística de la escena;
- las transiciones pertenecen a la escena y no a un slot DMX;
- cada escena referencia un look existente o es un blackout explícito.

Las capas simultáneas solo se incorporarán con prioridades y reglas de mezcla
explícitas. No se permitirá que la superposición accidental decida el show.

## Gates obligatorios antes del primer show

### G0 — Integridad de build

- [ ] APP Windows compila, abre y cierra limpiamente en el Lenovo objetivo.
- [ ] APP macOS universal compila, abre y cierra en Intel y Apple Silicon.
- [ ] VST3 Windows y macOS pasan Steinberg Validator sin crash ni fallos.
- [ ] Instaladores reproducibles y checksums publicados.

### G1 — Proyecto y edición

- [x] Documento versionado, parser acotado y guardado atómico.
- [x] `.aeylashow` ZIP mínimo con `project.json` y backup.
- [x] New/Open/Save/Save As en la arquitectura del producto.
- [x] Modelo determinista de canciones, escenas y bloques MIDI.
- [ ] Persistencia del programa de show dentro de `.aeylashow`.
- [ ] Editor de canciones y escenas.
- [ ] Piano roll/grilla MIDI con creación y edición de bloques.
- [ ] Importación/exportación MIDI tipo 0/1 con validación.
- [ ] Undo/redo y autosave de recuperación.

### G2 — Reproducción

- [ ] Transporte standalone estable: play, pause, stop, seek y loop de ensayo.
- [ ] Seguimiento del transporte de Ableton y reposicionamiento seguro.
- [ ] Scene engine con transiciones deterministas.
- [ ] Sin notas colgadas después de stop, seek, loop o cierre de editor.
- [ ] Reapertura de Set recupera el `.aeylashow` correcto en blackout/disarmed.
- [ ] Prueba completa del set de Aeyla con audios y clips definitivos.

### G3 — Salida física

- [ ] Backend Art-Net real conectado al motor compartido.
- [ ] Configuración de interfaz/IP/universo y detección de errores.
- [ ] Worker de salida con frecuencia estable y contadores de fallo.
- [ ] Prueba con nodo real y fixtures del proyecto.
- [ ] Blackout, disarm, pérdida de backend y recuperación validados físicamente.
- [ ] El DMX observado coincide byte a byte con el preview esperado.

### G4 — Estabilidad de función

- [ ] Cero defectos P0/P1 conocidos.
- [ ] Sanitizers y tests de plataforma verdes.
- [ ] Soak test mínimo de 8 horas sin crash, crecimiento de memoria ni drops.
- [ ] 20 ciclos de abrir/guardar/cerrar/reabrir proyecto sin corrupción.
- [ ] 20 ciclos de Ableton scan/load/save/reopen sin estado incorrecto.
- [ ] Ensayo completo del show al menos tres veces sin intervención técnica.
- [ ] Prueba de desconexión de red, cierre de ventana, seek, stop y reinicio.
- [ ] Procedimiento de backup y fallback ejecutado por otra persona del equipo.

## Definición honesta de estabilidad

No se declarará “100% estable” por intuición ni por una sola apertura exitosa.
La entrega se considerará **show-grade** cuando cumpla todos los gates anteriores,
no existan fallos críticos conocidos y las pruebas físicas/reales reproduzcan el
flujo de Aeyla completo. Hasta entonces, cada build debe identificarse como
Development, Rehearsal Candidate o Show Candidate.

## Orden de implementación

1. corregir build/Validator y obtener binarios reproducibles;
2. persistir canciones, escenas y clips MIDI en `.aeylashow`;
3. integrar scene engine y transporte;
4. construir editor de escenas y piano roll;
5. importar/exportar MIDI y conectar Ableton;
6. integrar Art-Net y validar hardware;
7. ejecutar pruebas de ensayo, soak, recuperación y fallback;
8. congelar Show Candidate específico para Aeyla.
