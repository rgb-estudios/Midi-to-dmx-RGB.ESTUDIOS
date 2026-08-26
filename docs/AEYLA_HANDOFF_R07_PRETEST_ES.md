# AEYLA · HANDOFF R07 PRETEST · CONTINUIDAD ENTRE CHATS

Fecha: 25/26 agosto 2026
Rama activa: `agent/aeyla-r05-dmx-clip-runtime`
PR de trabajo: #21 (borrador / no show-ready)

Este documento existe para retomar el trabajo desde otro chat sin reconstruir decisiones previas.

## 1. Objetivo inmediato

Llegar a una prueba oficial corta y de alto valor con un sistema estable para:

- capturar Art-Net de Avolites;
- grabar DMX de un universo sin crecimiento lineal de RAM;
- editar la toma como una muestra DMX;
- consolidar un clip limpio con ENTRADA/SALIDA propios;
- disparar canciones/acciones por MIDI desde Ableton Live o REAPER;
- reproducir el clip con cursor relativo propio, independiente de la posición absoluta del Arrangement;
- emitir Art-Net por una interfaz física seleccionada;
- llegar a nodo U1 y DMX físico sin cierres, pérdidas o comportamiento ambiguo.

La oportunidad de prueba de campo será limitada: aproximadamente 2 horas efectivas, posiblemente interrumpidas. No se debe consumir ese tiempo resolviendo instalación, permisos básicos, selección de NIC o errores evitables de configuración.

## 2. Arquitectura congelada

Avolites → Art-Net U1 → AEYLA CAPTURA → TOMA ORIGINAL `.aeylatake` → EDITOR DMX → CONSOLIDAR CLIP → comandos MIDI del DAW → reproductor relativo AEYLA → Art-Net 44 Hz → NIC física → nodo → DMX.

Reglas:

- Avolites crea la iluminación.
- AEYLA captura, conserva, edita y reproduce DMX.
- Ableton Live/REAPER organiza el show y entrega triggers MIDI.
- La posición absoluta del Arrangement NO gobierna el tiempo artístico del clip.
- El clip comienza en 00:00 al recibir REPRODUCIR/LANZAR y avanza por muestras procesadas del host.
- `LANZAR CANCIÓN N` debe permitir integrarse a triggers globales ya existentes sin depender del orden físico de tracks.

## 3. Idioma

Toda superficie visible al operador debe quedar en español.

Se permiten sólo nombres técnicos inevitables: Art-Net, DMX, MIDI, VST3, IPv4, Ableton Live, REAPER, Windows, macOS.

La superficie nativa principal fue traducida y muestra identidad `R07 PRETEST`.
Queda pendiente verificar en host real que ningún diálogo del framework o del
sistema operativo exponga cadenas heredadas en inglés.

## 4. GATE A · TRANSMISIÓN ART-NET · P0

Implementado en código:

- TX contractual a 44 Hz.
- Worker de red fuera del callback de audio.
- Bind explícito a IPv4 local seleccionada para TX.
- `SO_BROADCAST` habilitado para broadcast dirigido.
- ArtDMX en buffer fijo.
- scheduler con deadline acumulativo para evitar deriva de `now + period`.
- contadores de paquetes, errores, errores consecutivos, misses de timing, fail-closed y publicaciones obsoletas.
- tres errores consecutivos de envío → fail-closed y desarme de autoridades.
- rearme posterior debe ser explícito.
- desarme/cierre → ráfaga de tres cuadros BLACKOUT.
- lease process-local para impedir dos instancias AEYLA sobre el mismo destino/universo.

Riesgos / NO declarar cerrado todavía:

- UDP `sendto()` puede devolver éxito aunque el cable esté desconectado o el nodo esté apagado; hace falta validar estado físico del enlace/NIC además del resultado de socket.
- falta prueba real REAPER → AEYLA → NIC → nodo U1 → DMX/luminaria.
- falta validar unicast y broadcast dirigido con hardware real.
- falta test de pérdida/reconexión de cable, NIC y nodo.
- falta soak de 8 h.

### Permisos y red para Windows

El transmisor usa UDP/6454 y Winsock normal. No requiere abrir un puerto privilegiado ni ejecutar como administrador sólo para crear/enviar un socket UDP.

Aun así, antes de la prueba se debe verificar:

1. Firewall de Windows: permitir AEYLA/REAPER/Ableton en la red privada utilizada para el test, o crear una excepción explícita para UDP 6454 si fuera necesario.
2. Perfil de red: preferir red privada/controlada y evitar redes públicas corporativas que bloqueen broadcast/UDP.
3. NIC TX: seleccionar explícitamente la interfaz Ethernet física correcta; no loopback, VPN, Wi-Fi secundario ni adaptadores virtuales.
4. IPv4 fija: confirmar IP y máscara compatibles con nodo y red Art-Net antes de abrir la ventana de prueba.
5. Evitar DHCP durante test si puede cambiar la dirección.
6. Desactivar temporalmente VPN, bridges o adaptadores virtuales que puedan alterar routing si interfieren.
7. Confirmar que ninguna segunda instancia AEYLA posee el mismo destino/universo.
8. Probar `ping`/ARP o herramienta equivalente hacia el nodo cuando aplique; esto no valida Art-Net por sí solo, pero confirma camino IP básico.
9. Validar recepción Art-Net con nodo/herramienta externa antes de empezar a evaluar funciones complejas del editor.
10. Llevar una configuración de red conocida y un plan B de unicast si el broadcast dirigido del lugar es filtrado.

## 5. GATE B · EDITOR DE CLIP DMX · P0

Implementado en código e interfaz:

- TOMA ORIGINAL preservada e inmutable.
- captura directa a disco con RAM acotada.
- lector/reproductor file-backed con caché fija.
- ENTRADA/SALIDA no destructivos.
- reproducción relativa por muestras.
- backend `CONSOLIDAR CLIP`: genera un nuevo `.aeylatake` usando sólo ENTRADA→SALIDA.
- el primer cuadro de ENTRADA pasa a ser 00:00 del consolidado.
- consolidación file-backed sin materializar toda la canción en RAM.
- reapertura/validación del consolidado mediante metadatos/checksum/cantidad de cuadros.
- botón visible `CONSOLIDAR CLIP` conectado al rango ENTRADA/SALIDA;
- el clip consolidado queda seleccionado y preparado como fuente de reproducción;
- el test automatizado comprueba que la TOMA ORIGINAL permanece idéntica byte por byte.

Pendiente para completar el editor avanzado:

- timeline de actividad DMX real;
- handles ENTRADA/SALIDA arrastrables;
- playhead + scrub/preview;
- zoom horizontal;
- navegación TOMA ORIGINAL / tomas / clip consolidado activo;
- revertir a RAW claramente;
- prueba de cerrar/abrir editor sin afectar runtime.

No implementar fades generales de los 512 canales hasta clasificar canales continuos/discretos; interpolar modos/strobe/macros puede producir valores no deseados.

## 6. GATE C · MIDI / OPERACIÓN DEL SHOW · P0

Contrato congelado:

- SELECCIONAR CANCIÓN
- SIGUIENTE
- ANTERIOR
- REPRODUCIR / RETRIGGER
- PAUSA
- REANUDAR
- DETENER / RESET
- LANZAR CANCIÓN N

`LANZAR CANCIÓN N` es el camino preferido para integración automática con la sesión de los chicos.

Pendiente: asignación configurable / MIDI Learn y ejecución definitiva respetando `sampleOffset` del evento MIDI.

## 7. GATE D · MEMORIA / ESTABILIDAD · P0

No Show Candidate hasta validar:

- captura larga sin crecimiento lineal de RAM;
- reproducción repetida de clips;
- cambio entre canciones;
- editor abierto/cerrado;
- STOP/HOLD/BLACKOUT/DISARM;
- pérdida de host;
- pérdida de NIC/nodo;
- cierre limpio sin procesos colgados;
- soak mínimo 8 h;
- Windows + REAPER;
- Ableton Live Windows/macOS;
- idioma visible 100% español.

## 8. Plan de prueba de campo · ventana máxima aproximada 2 h

### Antes de llegar al espacio — obligatorio

Debe quedar hecho en taller/casa:

- instalar el PRETEST exacto;
- abrir DAW y confirmar carga VST3;
- comprobar que el plugin abre/cierra repetidamente;
- preparar proyecto de prueba mínimo;
- preparar uno o más `.aeylatake` conocidos;
- verificar que RX/TX aparecen y que TX selecciona la NIC física;
- dejar firewall/permisos resueltos;
- llevar IP/máscara objetivo anotadas;
- llevar cable Ethernet probado;
- llevar un plan de unicast conocido;
- llevar un archivo/escena DMX de diagnóstico evidente: dimmer 0/100, rojo, verde, azul, strobe corto si es seguro.

### Orden de ejecución en campo

0–15 min:
- cableado;
- IP/máscara;
- NIC correcta;
- nodo visible;
- validar Art-Net U1 básico.

15–35 min:
- TX continuo 44 Hz;
- PLAY/STOP/HOLD/BLACKOUT;
- confirmar ERR=0 y respuesta física.

35–60 min:
- captura desde Avolites;
- detener;
- reabrir toma;
- reproducirla al nodo;
- comparar comportamiento visual con fuente original.

60–85 min:
- IN/OUT;
- consolidar clip;
- reproducir consolidado;
- repetir inicio/final varias veces.

85–105 min:
- triggers MIDI: lanzar canción, pausa, reanudar, detener, retrigger;
- probar que mover el Arrangement no reposiciona el clip.

105–120 min:
- desconectar/reconectar cable o nodo una vez de forma controlada;
- comprobar fail-closed/BLACKOUT/rearme;
- guardar logs/contadores/observaciones;
- repetir una canción completa sin intervención.

Si la prueba es interrumpida, prioridad absoluta:
1. TX físico estable.
2. captura y reproducción de una toma real.
3. consolidación IN/OUT.
4. MIDI.
5. pruebas de fallo.

## 9. Criterios de éxito de la prueba corta

Mínimo para considerar que la oportunidad no fue desperdiciada:

- nodo recibe U1 desde la NIC física correcta;
- Art-Net sale sostenido y visualmente estable;
- ERR de envío permanece en 0 durante operación normal;
- una toma real puede grabarse, cerrarse, cargarse y reproducirse;
- RAM no muestra crecimiento lineal evidente durante la captura;
- IN/OUT y consolidación producen un clip reproducible;
- PLAY/PAUSA/REANUDAR/STOP funcionan sin depender de la posición absoluta del DAW;
- BLACKOUT y DISARM funcionan de manera determinista;
- cerrar la interfaz gráfica no detiene el runtime;
- ningún cierre inesperado del DAW/plugin.

## 10. Regla de continuidad

No abrir nuevas funciones cosméticas hasta cerrar primero:

1. Art-Net TX físico.
2. editor DMX utilizable.
3. MIDI de show.
4. estabilidad.
5. español completo.

Los builds de esta etapa se denominan PRETEST. No usar `LISTO PARA SHOW` mientras exista un P0 abierto.
