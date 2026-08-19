# AEYLA — Contrato de red y memoria del runtime

Estado: PRODUCT / SHOW-READY GATE

## 1. La red pertenece a AEYLA, no al DAW

El host (REAPER, Ableton Live o Logic Pro) no configura Art-Net para AEYLA.

AEYLA debe enumerar directamente los adaptadores IPv4 activos del sistema y permitir seleccionar de forma explícita:

- NIC de entrada RX;
- NIC de salida TX;
- IPv4 local asociada;
- prefijo/máscara informativa;
- universo / Art-Net Port-Address;
- destino Art-Net TX.

RX y TX pueden usar adaptadores distintos.

Ejemplo soportado:

Avolites -> USB-C Ethernet 2.0.0.20 -> AEYLA RX
AEYLA TX -> Ethernet integrado 10.10.10.20 -> node 10.10.10.50

Cambiar RX reinicia solamente el listener de captura.
Cambiar TX desarma inmediatamente cualquier salida física y exige ARM explícito.

`ANY / 0.0.0.0` queda reservado para diagnóstico y no constituye una configuración SHOW READY.

## 2. Permisos

### Operación normal

No requiere elevar el DAW.

AEYLA puede, con permisos normales del usuario:

- descubrir adaptadores;
- leer nombre e IPv4 actual;
- seleccionar una NIC;
- hacer bind de RX a una IPv4 local;
- hacer bind/source de TX desde una IPv4 local;
- enviar y recibir UDP Art-Net.

### Modificar IP/máscara del sistema

Cambiar la configuración IPv4 real de Windows/macOS es una operación privilegiada del sistema.

Nunca ejecutar REAPER/Ableton/Logic completo como Administrator/root para permitirlo.

Si el producto añade en el futuro `SET ADAPTER IP`, debe existir un helper separado con privilegio mínimo que:

1. reciba sólo adaptador + IP + máscara/prefijo solicitados;
2. valide que el adaptador sigue siendo el mismo;
3. pida elevación explícita del SO;
4. aplique la modificación;
5. devuelva resultado auditable;
6. termine inmediatamente.

El plugin continúa sin privilegios elevados.

## 3. Presupuesto de memoria

Un universo DMX completo representa 512 bytes por frame.

A 44 Hz durante 50 min:

`512 * 44 * 3000 = 67,584,000 bytes`, aproximadamente 64.45 MiB de payload DMX bruto.

Ese tamaño es pequeño en disco, pero AEYLA no debe multiplicarlo por Takes históricas ni mantener crecimiento lineal innecesario en RAM.

### Producción

La captura final debe ser stream-to-disk:

Art-Net RX -> estado DMX actual -> sampler 44 Hz -> ring/queue acotada -> writer `.aeylatake.tmp`.

Objetivo para frames pendientes en RAM:

- preferido <= 1 MiB;
- máximo operativo <= 4 MiB.

La cola es acotada. Si el disco/USB/SSD deja de aceptar datos y la cola alcanza el límite, la aplicación debe declarar fallo de almacenamiento; nunca continuar indicando REC seguro.

## 4. Persistencia durante REC

La Take en producción no espera hasta STOP para existir.

Mientras REC está activo se escribe progresivamente una Take temporal recuperable.

Checkpoint durable periódico objetivo: aproximadamente 1 segundo, sujeto a medición de latencia del volumen.

STOP:

1. deja de aceptar nuevos frames;
2. vacía la cola de escritura;
3. fija frame-count/duración;
4. completa checksum;
5. hace flush/sync;
6. valida el archivo;
7. promociona atómicamente `.tmp` a `.aeylatake`.

Un crash no debe convertir automáticamente toda la grabación anterior en pérdida total. El recovery de `.tmp` es un gate posterior obligatorio.

## 5. Playback

La biblioteca completa permanece en disco.

No cargar todas las canciones ni todas las versiones de Takes en RAM.

La arquitectura de producción debe usar lectura file-backed, mmap o streaming de frames con prefetch acotado.

Data rate de playback para un universo a 44 Hz: aproximadamente 22 KiB/s, trivial para un SSD/USB sano.

El buffer de lectura puede mantenerse pequeño porque AEYLA necesita como máximo el frame actual y una ventana de prefetch limitada.

## 6. Caché

- Metadata/header de todas las Takes: permitido en RAM.
- Payload completo de Takes históricas: prohibido como diseño permanente.
- Como máximo una Take activa puede quedar materializada completamente durante gates intermedios de desarrollo.
- Cambiar de Song/Take debe liberar el payload anterior cuando no sea requerido por playback/HOLD.

## 7. Telemetría mínima

La UI/diagnóstico debe exponer:

- NIC RX seleccionada;
- NIC TX seleccionada;
- IPv4 RX/TX;
- universo;
- destino TX;
- RX LIVE/LOST;
- frames grabados;
- queue/buffer de escritura;
- errores/overflow de storage;
- Take library activa;
- memoria estimada de payload/cache de AEYLA.

No llenar la vista SHOW con esta información. Debe vivir en Routing/Diagnostics.

## 8. Acceptance gates

### Network

- dos NIC físicas simultáneas;
- RX Avolites por NIC A;
- TX node/Capture por NIC B;
- cambiar NIC TX fuerza DISARM;
- desconectar/reconectar ambas rutas;
- Wi-Fi presente no captura accidentalmente autoridad Art-Net;
- ninguna configuración depende del DAW.

### Memory

Medir delta de working set del proceso host:

- idle;
- REC 10 min;
- REC 50 min;
- playback 50 min;
- 15 Songs con varias Takes históricas en disco;
- 50 ciclos load/play/stop/unload.

PASS final:

- buffer de captura estable con duración;
- ausencia de crecimiento lineal durante streamed REC;
- ausencia de acumulación por historial de Takes;
- sin leak observable tras ciclos repetidos;
- ningún dropout de audio provocado por I/O de archivo o red.
