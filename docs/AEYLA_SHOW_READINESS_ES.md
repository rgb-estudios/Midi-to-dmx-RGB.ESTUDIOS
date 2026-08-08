# AEYLA Visual DMX — objetivo operativo y criterios de show

## Propósito vinculante

AEYLA Visual DMX no es un experimento genérico de MIDI ni una interfaz de
previsualización. Su primera entrega debe operar el show real de **Aeyla**, de
hasta 45 minutos y un máximo de **15 canciones**, coordinando iluminación,
escenas visuales y automatización MIDI-DMX con comportamiento determinista,
recuperación segura y documentación suficiente para ensayo, montaje y función.

La misma lógica de proyecto y reproducción debe funcionar en:

1. standalone Windows;
2. standalone macOS;
3. VST3 dentro de REAPER en Windows;
4. VST3 dentro de REAPER en macOS;
5. VST3 dentro de Ableton Live en Windows;
6. VST3 dentro de Ableton Live en macOS Apple Silicon;
7. Audio Unit dentro de Logic Pro en macOS Apple Silicon.

El archivo `.aeylashow` es la fuente de verdad portable. APP, VST3 y AU no
pueden mantener versiones distintas del show.

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
- Organizar hasta 15 canciones en orden de show.
- Dibujar Cue placements en una grilla musical con compás, tempo, PPQ, zoom,
  snapping y duración. Nota/canal son metadatos secundarios de MIDI Learn.
- Visualizar claramente qué Look produce cada Cue.
- Permitir editar, duplicar, mover, redimensionar y borrar bloques.
- Importar y exportar archivos MIDI estándar para trabajar con REAPER,
  Ableton y Logic.
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

La versión 0.3 usa un máximo de **15 canciones** y una **pista única de escenas**
por canción:

- una escena activa a la vez;
- los bloques pueden tocarse, pero no solaparse;
- Note Off se procesa antes que Note On cuando comparten tick;
- una Cue LATCH permanece hasta la siguiente Cue; la duración es semántica para
  MOMENTARY y metadato/editor para LATCH;
- las transiciones pertenecen a la escena y no a un slot DMX;
- cada escena referencia un look existente o es un blackout explícito.

Las capas simultáneas solo se incorporarán con prioridades y reglas de mezcla
explícitas. No se permitirá que la superposición accidental decida el show.

## Gates obligatorios antes del primer show

### G0 — Integridad de build y hosts

- [ ] APP Windows compila, abre y cierra limpiamente en el Lenovo objetivo.
- [ ] APP macOS universal compila, abre y cierra en Intel y Apple Silicon.
- [ ] VST3 Windows y macOS pasan Steinberg Validator sin crash ni fallos.
- [ ] AU macOS pasa `auval` y validación de Logic Plug-in Manager.
- [ ] REAPER Windows escanea, instancia, guarda y reabre el VST3.
- [ ] REAPER macOS escanea, instancia, guarda y reabre el VST3.
- [ ] Ableton Windows escanea, instancia, guarda y reabre el VST3.
- [ ] Ableton macOS Apple Silicon escanea, instancia, guarda y reabre el VST3 nativo.
- [ ] Logic Pro Apple Silicon escanea, instancia, guarda y reabre el AU nativo.
- [ ] Instaladores reproducibles y checksums publicados.

### G1 — Proyecto y edición

- [x] Documento versionado, parser acotado y guardado atómico.
- [x] `.aeylashow` ZIP mínimo con `project.json` y backup.
- [x] New/Open/Save/Save As en la arquitectura del producto.
- [x] Modelo determinista de canciones, escenas y bloques MIDI.
- [x] Límite de 15 canciones aplicado en el modelo y cubierto por regresión.
- [x] Persistencia del programa de show dentro de `.aeylashow` (`show.bin`).
- [ ] Editor completo de canciones y Cues; el slice Store/Navigate está **Scaffolded**.
- [ ] Piano roll/grilla MIDI con creación y edición de bloques.
- [ ] Importación/exportación MIDI tipo 0/1 con validación.
- [ ] Undo/redo y autosave de recuperación.

### G2 — Reproducción y transporte DAW

- [ ] Transporte standalone estable: play, pause, stop, seek y loop de ensayo.
- [ ] Seguimiento de transporte implementado en fuente; falta Host-tested en
      REAPER, Ableton y Logic con reposicionamiento seguro.
- [ ] Scene engine con transiciones deterministas.
- [ ] Sin notas colgadas después de stop, seek, loop o cierre de editor.
- [ ] Reapertura del proyecto DAW recupera el `.aeylashow` correcto en blackout/disarmed.
- [x] La fase/timeline artística en fuente deriva de PPQ, no de reloj de pared;
      falta revalidación de host en el SHA actual.
- [ ] Prueba completa del set de Aeyla con audios y clips definitivos.

### G3 — Salida física

- [ ] Backend Art-Net real conectado al motor compartido.
- [ ] Configuración de interfaz/IP/universo y detección de errores.
- [ ] Worker de salida con frecuencia estable y contadores de fallo.
- [ ] Ninguna llamada de red o espera bloqueante ocurre dentro del callback del host.
- [ ] Render/bounce offline no puede emitir una secuencia acelerada a Art-Net.
- [ ] Una sola instancia posee la salida física de cada universo; duplicados no compiten.
- [ ] Prueba con nodo real y fixtures del proyecto.
- [ ] Blackout, disarm, pérdida de backend y recuperación validados físicamente.
- [ ] El DMX observado coincide byte a byte con el preview esperado.

### G4 — Estabilidad de función

- [ ] Cero defectos P0/P1 conocidos.
- [ ] Sanitizers y tests de plataforma verdes.
- [ ] Soak test mínimo de 8 horas sin crash, crecimiento de memoria ni drops.
- [ ] 20 ciclos de abrir/guardar/cerrar/reabrir proyecto sin corrupción.
- [ ] 20 ciclos de scan/load/save/reopen por cada host soportado sin estado incorrecto.
- [ ] Saltos y seeks entre las 15 canciones reconstruyen exactamente el mismo estado DMX.
- [ ] Ensayo completo del show al menos tres veces sin intervención técnica.
- [ ] Prueba de desconexión de red, cierre de ventana, seek, stop y reinicio.
- [ ] Procedimiento de backup y fallback ejecutado por otra persona del equipo.

## Definición honesta de estabilidad

No se declarará “100% estable” por intuición ni por una sola apertura exitosa.
La entrega se considerará **show-grade** cuando cumpla todos los gates anteriores,
no existan fallos críticos conocidos y las pruebas físicas/reales reproduzcan el
flujo de Aeyla completo. Hasta entonces, cada build debe identificarse como
Development, Rehearsal Candidate o Show Candidate.

Un binario puede declararse **REAPER Test Candidate** antes de Show Candidate
solamente cuando compile de forma reproducible, pase el core, REAPER lo escanee
e instancie, sobreviva save/reopen y mantenga la seguridad `DISARMED + BLACKOUT`
al restaurar. Esa etiqueta autoriza programación/ensayo controlado, no operación
de luminarias en un show.

## Orden de implementación

1. corregir build/Validator y obtener binarios reproducibles;
2. cerrar REAPER Windows como primer host de ensayo y generar Test Candidate;
3. persistir canciones, escenas y clips MIDI en `.aeylashow`;
4. integrar scene engine y transporte determinista de host;
5. construir editor de escenas y piano roll;
6. importar/exportar MIDI y cerrar REAPER/Ableton/Logic;
7. integrar Art-Net y validar hardware;
8. ejecutar pruebas de ensayo, soak, recuperación y fallback;
9. congelar Show Candidate específico para Aeyla.
