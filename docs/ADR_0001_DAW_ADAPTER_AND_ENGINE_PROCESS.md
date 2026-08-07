# ADR 0001 — Adaptadores DAW y proceso AEYLA Engine

- Estado: **Aceptado para implementación**
- Fecha: 2026-08-07
- Alcance: standalone Windows/macOS, VST3 Windows/macOS y AUv2 macOS

## Contexto

AEYLA debe operar un show real desde Ableton Live, REAPER o Logic Pro. Cada DAW
tiene un ciclo de vida y un modelo de hosting diferente:

- VST3 puede ser suspendido, descargado, ejecutado en proceso aislado o eliminado;
- REAPER permite distintas modalidades de ejecución y firewall del plug-in;
- Logic valida Audio Units y puede alojarlas mediante servicios separados;
- bypass, pista offline, cierre de editor, recuperación del proyecto o crash del
  host no deben dejar Art-Net activo en un estado incierto;
- varias instancias de un plug-in no pueden competir por el mismo universo;
- un Audio Unit no debe ser responsable de resolver archivos, permisos y red de
  forma distinta al VST3.

Mantener un backend DMX completo dentro de cada formato produciría tres riesgos:

1. comportamiento distinto entre hosts;
2. pérdida de salida o estado al suspender/descargar el plug-in;
3. ownership ambiguo del nodo Art-Net con varias instancias.

## Decisión

Se implementará un único proceso local **AEYLA Engine** como propietario de:

- `.aeylashow` y recuperación;
- canciones, escenas, timeline y scene engine;
- compilación semántica y trama DMX;
- Art-Net y futuros backends nombrados;
- Output Arm, blackout, watchdog y estado seguro;
- telemetría, logs y evidencia de ensayo;
- ownership exclusivo del universo físico.

Los formatos de host serán adaptadores delgados:

- **VST3** para Ableton/REAPER en Windows y macOS;
- **AUv2** para Logic/REAPER en macOS;
- standalone como editor, operador y fallback.

El adaptador recibe MIDI/transporte del DAW y lo publica al Engine mediante una
abstracción IPC local. No abre Art-Net, USB-DMX, medios ni archivos desde el
callback. El callback solo deposita eventos compactos en una cola acotada; un
worker no-realtime realiza la comunicación local.

## Propiedades obligatorias

### Una sola fuente de verdad

- El Engine carga y guarda el `.aeylashow`.
- Los adaptadores almacenan UUID/locator y preferencias seguras, no una copia
  divergente del show.
- Standalone, VST3 y AUv2 muestran snapshots del mismo estado autoritativo.

### Ownership de salida

- Solo el Engine puede abrir el socket Art-Net.
- Existe un único lease de output por proyecto/universo.
- Una segunda instancia puede observar/controlar según permisos, pero no toma
  ownership silenciosamente.
- Output Arm requiere gesto explícito en la sesión actual del Engine.

### Heartbeat y fallo seguro

- Adaptador y Engine intercambian heartbeat fuera del callback.
- Pérdida del host propietario, IPC o backend fuerza una política configurada y
  visible; para la primera Show Candidate será blackout + disarm.
- Reiniciar Ableton, REAPER o Logic nunca rearma output.
- Un crash del editor gráfico no destruye el Engine ni deja workers huérfanos.

### Transporte

Cada evento enviado al Engine incluye:

- identidad de instancia y proyecto;
- host y formato;
- estado play/stop/pause;
- posición musical y temporal;
- tempo y compás;
- loop/cycle;
- Note On/Off, canal, velocity y sample/tick offset;
- número de secuencia para detectar pérdidas/reordenamiento.

Seek, loop y reconexión no se resuelven reproduciendo historia antigua: el Engine
reconstruye la escena correcta para la posición autoritativa del host.

### IPC multiplataforma

La API será común y versionada, con transports nativos detrás de una interfaz:

- Windows: named pipe local con ACL de usuario;
- macOS: Unix domain socket o XPC local según firma/sandbox final;
- sin exposición a la red LAN;
- mensajes acotados, versionados y con longitud validada;
- backpressure y contadores de drops;
- autenticación local por sesión/token efímero;
- ninguna espera bloqueante en audio callback.

La elección final del transport macOS se cerrará con pruebas de Logic/AUv2 y
notarización. El protocolo lógico no dependerá del transport.

## Modos de operación

### DAW Mode

DAW → adaptador VST3/AUv2 → AEYLA Engine → Art-Net.

El plug-in puede cerrar su ventana; la instancia sigue enviando transporte/MIDI.
El Engine muestra host, instancia propietaria, latencia y conexión.

### Standalone Mode

Editor/operador → AEYLA Engine → Art-Net.

Puede reproducir el programa interno, recibir MIDI externo o cargar MIDI
exportado desde Ableton/Logic/REAPER.

### Fallback Mode

Un segundo equipo ejecuta standalone con el mismo `.aeylashow`, pero permanece
desarmado. El cambio de ownership requiere una acción manual explícita; no se
implementará failover automático hasta probar split-brain y red física.

## Consecuencias

### Positivas

- mismo comportamiento de output en todos los DAWs;
- Logic no necesita poseer directamente el backend Art-Net;
- mejor recuperación ante crash o descarga del plug-in;
- ownership y multiinstancia verificables;
- standalone se convierte en respaldo real del DAW;
- reduce código específico dentro de VST3/AUv2.

### Costes

- se agrega un protocolo IPC y un proceso instalable;
- instalador/firma deben desplegar Engine + formatos;
- es necesario medir latencia y jitter extremo a extremo;
- el DAW y Engine deben resolver reconexión y versiones incompatibles;
- el show no puede declararse listo hasta probar el conjunto completo.

## Gates de aceptación

- [ ] Engine inicia desarmado y en blackout.
- [ ] Adaptador conecta/desconecta sin bloquear callback.
- [ ] 100000 eventos secuenciales sin pérdida en carga nominal.
- [ ] Drops deliberados se detectan y fuerzan resync.
- [ ] Stop/seek/loop reconstruyen escena correctamente.
- [ ] Cierre de ventana no cambia output.
- [ ] Eliminación/bypass/offline de instancia libera lease y asegura salida.
- [ ] Crash del DAW detectado por heartbeat.
- [ ] Varias instancias no obtienen ownership simultáneo.
- [ ] VST3 y AUv2 producen snapshots/DMX equivalentes para el mismo fixture.
- [ ] Ocho horas de DAW + Engine + Art-Net sin fuga, deadlock ni drop.

## Rechazado

- Backend Art-Net independiente dentro de cada plug-in.
- Usar el audio callback para IPC, archivos, red o compilación DMX.
- Compartir output mediante “última instancia gana”.
- Restaurar Output Arm desde el proyecto del DAW.
- Depender de que la ventana del plug-in permanezca abierta.
