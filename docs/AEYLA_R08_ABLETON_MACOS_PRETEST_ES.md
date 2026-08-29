# AEYLA Visual DMX — R08 macOS / Ableton Live PRETEST

Fecha de definición: 29 de agosto de 2026.

## 1. Objetivo

R08 convierte macOS + Ableton Live en el entorno principal de producción de AEYLA Visual DMX. Windows/REAPER continúa como banco de pruebas y compatibilidad, pero una versión no se promociona a SHOW CANDIDATE si no supera el flujo real en Ableton sobre un Mac.

Entorno objetivo:

- Ableton Live 12.
- macOS 11.7.10 o posterior dentro de la matriz oficialmente soportada por Live 12.
- Mac Apple Silicon como objetivo principal.
- Intel x86_64 conservado en el binario Universal para compatibilidad.
- Ethernet cableado dedicado para Art-Net.
- VST3 como formato principal del Set.
- AUv2 como formato secundario de respaldo/diagnóstico; no mezclar VST3 y AU del mismo plugin dentro del mismo Set.

## 2. Formato recomendado en Ableton

Usar **AEYLA Visual DMX VST3**.

AEYLA está declarado como instrumento MIDI silencioso:

- se inserta en una pista MIDI dedicada;
- recibe las notas MIDI SHOW directamente desde clips MIDI o Arrangement;
- no necesita entrada de audio;
- su salida de audio permanece en silencio;
- el motor DMX usa el reloj de muestras que Ableton entrega al plugin.

AUv2 se compila y valida para macOS, pero no es el formato primario del show. VST3 conserva además la misma identidad de formato cuando el Set se traslada entre macOS y Windows.

## 3. Arquitectura de sincronía

Ruta principal:

`Ableton audio + MIDI SHOW -> mismo reloj de muestras -> AEYLA -> Art-Net`

El evento MIDI SHOW de inicio se coloca exactamente en el punto artístico de comienzo de cada canción. AEYLA compensa el sample offset del bloque de audio y gobierna la toma DMX con el cursor de muestras del host.

La captura física y reproducción DMX siguen trabajando nominalmente a 44 fps, por lo que la resolución física final es aproximadamente 22,7 ms por cuadro DMX. El disparo dentro de Ableton puede ser sample-accurate; no se debe prometer una resolución física menor que un cuadro DMX.

## 4. Timecode hacia Avolites

Ableton Live puede **recibir** MIDI Timecode, pero no genera MTC nativamente como master. Su salida Sync nativa es MIDI Clock.

Por lo tanto AEYLA no debe depender de MTC para sincronizar el DMX con el audio.

Para Avolites se permiten dos rutas paralelas, independientes del reloj interno de AEYLA:

### Opción A — MTC mediante Max for Live

Usar un dispositivo Max for Live dedicado a generar MTC y rutearlo al puerto MIDI que recibe Avolites.

Ventaja: conserva el flujo MTC ya conocido.

Requisito: validar el dispositivo Max for Live, su puerto MIDI y el frame rate antes del show.

### Opción B — LTC en pista de audio dedicada

Usar un archivo/pista LTC generado previamente y una salida física dedicada de la interfaz de audio hacia la entrada LTC correspondiente del sistema de iluminación.

Ventaja: el LTC queda reproducido por el mismo motor de audio que reproduce las canciones.

La elección MTC/LTC para Avolites no cambia el reloj del plugin: AEYLA continúa sincronizado por muestras de Ableton.

## 5. Convención por canción

La convención de una hora de timecode por canción puede conservarse para Avolites:

- canción 01 -> 01:00:00:00
- canción 02 -> 02:00:00:00
- canción 03 -> 03:00:00:00
- etc.

La nota MIDI SHOW que dispara AEYLA debe ubicarse en el mismo inicio artístico que se usa para el timecode de esa canción.

No es necesario que AEYLA decodifique esa hora para mantenerse sincronizado.

## 6. Configuración de Ableton

### Pista AEYLA

1. Crear una pista MIDI dedicada llamada `AEYLA DMX`.
2. Insertar `AEYLA Visual DMX` en formato VST3.
3. Mantener la pista activa durante todo el show.
4. Usar clips/Arrangement MIDI para las notas MIDI SHOW.
5. No insertar una instancia AU adicional de AEYLA en el mismo Set.

### MIDI SHOW R07/R08

Mapa inicial:

- N36: canción anterior.
- N37: siguiente canción.
- N38: PLAY / retrigger.
- N39: pausa / reanudar.
- N40: STOP / RESET.
- N41: PANIC / APAGÓN unidireccional.
- N48–62: canción 01–15.

El canal por defecto es 16 y puede cambiarse desde AEYLA.

PANIC N41 nunca puede armar Art-Net ni quitar APAGÓN.

## 7. Grabación con pre-roll en Ableton

1. Seleccionar la canción en AEYLA.
2. Confirmar Art-Net RX desde Avolites.
3. Comenzar `GRABAR NUEVA TOMA` antes del inicio real.
4. Ableton puede estar ya corriendo en pre-roll.
5. Colocar/disparar N38 exactamente en el inicio artístico.
6. Durante grabación, esa nota actúa como marcador explícito de sincronía y no intenta reproducir una toma antigua.
7. Detener y guardar.
8. AEYLA conserva el RAW completo y usa el marcador como ENTRADA no destructiva.

## 8. Red en macOS

AEYLA puede enumerar interfaces IPv4 y R08 distingue interfaces Wi-Fi de interfaces Ethernet en macOS. La salida de show debe utilizar Ethernet cableado.

R08 no modifica de forma privilegiada la configuración IPv4 del sistema desde dentro de Ableton. Antes de abrir el show:

1. Conectar el adaptador Ethernet que se usará para Art-Net.
2. Configurar su IPv4/submáscara en Ajustes del Sistema -> Red.
3. Usar una red compatible con Avolites/nodo/Capture.
4. Abrir Ableton y AEYLA después de configurar la NIC.
5. En AEYLA pulsar reescaneo y seleccionar esa interfaz para RX/TX.
6. Confirmar que la IPv4 mostrada por AEYLA coincide con la configurada en macOS.

Esto evita pedir privilegios de administrador desde un plugin cargado dentro de Ableton y reduce un punto de fallo durante show.

## 9. Instalación macOS PRETEST

El paquete R08 contiene:

- `VST3/AeylaVisualDmx.vst3` Universal arm64+x86_64.
- `AUv2/AeylaVisualDmx.component` Universal arm64+x86_64.
- `INSTALL_AEYLA_ABLETON.command` para instalar únicamente VST3, opción recomendada.
- `INSTALL_AEYLA.command` para instalar VST3 + AUv2 si se requiere diagnóstico en ambos formatos.
- scripts de auditoría/desinstalación limitados a AEYLA.
- `BUILD_ID.txt` y hashes SHA-256.

La compilación PRETEST usa firma ad-hoc. Una distribución pública definitiva requiere Developer ID y notarización de Apple; no se debe presentar una build ad-hoc como release pública firmada/notarizada.

## 10. Gate automático macOS

El workflow `ableton-macos-pretest` debe aprobar:

1. compilación VST3 Universal;
2. compilación AUv2 Universal;
3. presencia de slices arm64 + x86_64;
4. verificación codesign ad-hoc de ambos bundles;
5. Steinberg VST3 Validator;
6. Apple `auval` para AUv2;
7. generación de artefacto PRETEST con BUILD_ID y hashes.

Estas pruebas no sustituyen Ableton real, porque Ableton Live no está disponible en GitHub Actions.

## 11. Gate físico obligatorio — Ableton / Mac

### A — descubrimiento del plugin

- instalación limpia VST3;
- Live 12 encuentra `AEYLA Visual DMX` bajo VST3;
- insertar en pista MIDI;
- abrir/cerrar UI repetidamente sin crash;
- redimensionar ventana y cambiar pestañas.

### B — MIDI y transporte

- ejecutar N36/N37/N38/N39/N40 desde clips MIDI;
- confirmar selección PREPARADA/ACTIVA correcta;
- hacer cinco lanzamientos desde el mismo punto de Arrangement;
- no debe aparecer deriva acumulativa respecto del audio.

### C — captura

- Avolites -> Art-Net -> Ethernet Mac -> AEYLA;
- grabar con pre-roll;
- N38 en inicio real debe fijar ENTRADA de captura;
- RAW debe permanecer intacto.

### D — carrier y reproducción física

- quitar APAGÓN manualmente;
- ARMAR manualmente;
- sin PLAY, Capture/nodo debe detectar el universo inmediatamente;
- observar al menos 30 s con Ableton detenido: carrier estable;
- PLAY por MIDI SHOW debe iniciar la toma sincronizada;
- STOP/RESET conserva carrier y vuelve a cero;
- DISARM elimina autoridad.

### E — seguridad

- con salida armada, enviar N41;
- resultado: APAGÓN activo + autoridad desarmada;
- repetir N41 varias veces: jamás vuelve la luz;
- quitar APAGÓN manualmente no rearma la salida;
- ARM posterior siempre es manual.

### F — sesión completa

- preparar al menos tres canciones consecutivas;
- cambiar PREPARADA mientras otra canción está activa;
- lanzar cada canción desde su clip/marker real;
- validar que el cambio entre tomas no introduce blackout no solicitado;
- validar la ruta paralela de timecode seleccionada para Avolites.

## 12. Promoción

R08 sólo puede denominarse `ABLETON/MAC SHOW CANDIDATE` cuando los gates automáticos y A–F hayan pasado sobre el Mac de show o sobre un Mac equivalente con la misma arquitectura, versión de Live y adaptador de red.
