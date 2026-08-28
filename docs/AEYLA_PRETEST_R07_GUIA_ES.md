# AEYLA — Guía de prueba R07

Estado: **PRETEST / NO APTO TODAVÍA PARA SHOW**

Esta guía corresponde a la arquitectura congelada de AEYLA para captura, recorte y reproducción DMX desde un DAW sin depender de la posición absoluta del arreglo.

## Objetivo de esta prueba

Validar primero el camino crítico de un universo:

```text
Avolites → Art-Net RX → AEYLA → toma DMX en disco
                              ↓
                         ENTRADA / SALIDA
                              ↓
                      reproducción relativa
                              ↓
                    Art-Net TX por NIC física
                              ↓
                         nodo U1 → DMX
```

No se debe utilizar esta revisión en un show hasta completar pruebas físicas, hosts reales y resistencia prolongada.

## Preparación

1. Cierra instancias antiguas de AEYLA y REAPER antes de instalar/reemplazar el VST3.
2. Abre una sola instancia del plugin.
3. Selecciona el adaptador RX que recibe Art-Net desde Avolites.
4. Selecciona el adaptador TX conectado a la red/nodo de salida.
5. Verifica IP y máscara del adaptador TX y aplica la red.
6. Mantén APAGÓN activo mientras preparas la sesión.
7. Selecciona o crea la canción que vas a probar.

## Prueba A — recepción

Con Avolites transmitiendo Art-Net continuo:

- RX debe indicar ACTIVO.
- Debe aparecer IP de origen.
- El contador de paquetes debe aumentar.
- Los errores de secuencia deben permanecer estables o en cero bajo una red limpia.

Si RX no está activo, no continúes a grabación.

## Prueba B — grabación directa a disco

1. Presiona GRABAR.
2. La primera vez selecciona una carpeta local o SSD para la biblioteca de tomas.
3. Deja grabar al menos 3–5 minutos durante la primera campaña.
4. Ejecuta cambios rápidos, fades, strobes y estados estáticos desde Avolites.
5. Presiona GRABAR nuevamente para cerrar la toma.

Resultado esperado:

- la interfaz indica grabación a disco;
- se crea un `.aeylatake` válido;
- no se acumula la toma completa en RAM;
- la captura informa 44 Hz;
- ningún overflow o error de almacenamiento queda oculto.

## Prueba C — recorte tipo muestra

Con la salida física desarmada y la reproducción pausada:

- mueve ENTRADA en pasos de ±0,1 s y ±1 s;
- mueve SALIDA en pasos de ±0,1 s y ±1 s;
- restaura ENTRADA / SALIDA;
- verifica que la grabación original no sea modificada.

El recorte R07 es no destructivo y se aplica al rango de reproducción desde disco. La consolidación material de un nuevo archivo final sigue siendo un gate posterior.

## Prueba D — reproducción relativa

1. Carga/reproduce la toma seleccionada.
2. El clip debe iniciar desde su ENTRADA como tiempo relativo 00:00.
3. PAUSA debe mantener el último estado DMX.
4. REANUDAR debe continuar desde el mismo cursor relativo.
5. Cambiar o mover la posición absoluta del arreglo del DAW no debe seleccionar directamente un cuadro DMX.

El runtime usa muestras procesadas consecutivas para avanzar el cursor. Un salto grande o discontinuidad del arreglo se considera SEEK y se ignora para el tiempo artístico del clip.

## Prueba E — salida Art-Net física

Sólo después de validar A–D:

1. Configura un destino Art-Net válido.
2. Desactiva APAGÓN.
3. Arma la salida de la toma.
4. Reproduce el clip.
5. Confirma en el nodo/luminarias que el universo U1 responde.

Registrar:

- paquetes TX;
- errores TX;
- continuidad visual;
- latencia perceptible;
- comportamiento al pausar/reanudar;
- comportamiento al desconectar/reconectar el nodo.

Ante un fallo de host, renderizado sin conexión o pérdida del pulso de vida, la autoridad física debe deshabilitarse de forma segura.

## Prueba F — memoria y estabilidad

En el Administrador de tareas registra RAM al inicio y luego de:

- 10 minutos;
- 30 minutos;
- 50 minutos;
- varias campañas GRABAR / CERRAR / REPRODUCIR / PAUSAR / REANUDAR.

La memoria puede fluctuar por host, UI y cachés, pero no debe crecer proporcionalmente a la duración de cada toma ni acumular cinco payloads completos por canción.

## Automatización MIDI integrada en este checkpoint

La arquitectura de show queda definida así:

- SELECCIONAR CANCIÓN;
- SIGUIENTE CANCIÓN;
- CANCIÓN ANTERIOR;
- REPRODUCIR / REINICIAR;
- PAUSA;
- REANUDAR;
- DETENER / REINICIAR;
- opcional: LANZAR CANCIÓN N.

Estas acciones son asignables desde `MIDI / SHOW` y respetan el `sampleOffset`
del evento. El orden de tracks, escenas o clips en Ableton Live/REAPER no
gobierna la posición artística del DMX. Su validación dentro del host sigue
siendo un gate abierto de PRETEST.

## Criterio de aprobación de esta revisión

R07 PRETEST sólo se considera aprobado cuando:

- compila y valida en CI;
- abre en REAPER sin crash;
- captura a disco correctamente;
- reproduce desde disco;
- recorta ENTRADA/SALIDA sin cargar el Take completo;
- transmite Art-Net por la NIC física seleccionada;
- no presenta crecimiento anormal de RAM durante una campaña prolongada.

MIDI Learn, consolidación material y editor gráfico están integrados en código;
siguen denominándose PRETEST hasta comprobar interacción, sincronía y Art-Net
continuo dentro de los hosts y el hardware reales.
