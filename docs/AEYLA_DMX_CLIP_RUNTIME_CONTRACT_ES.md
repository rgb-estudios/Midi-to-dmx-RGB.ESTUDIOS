# AEYLA — Contrato de ejecución de muestra DMX

Estado: **R07 / arquitectura congelada para integración MIDI**  
Ámbito: captura Art-Net, persistencia, edición no destructiva, consolidación, reproducción por eventos MIDI y salida Art-Net de un universo.

## 1. Principio de producto

AEYLA no reemplaza al DAW ni a Avolites.

```text
AVOLITES
  autoría creativa / programación de iluminación
        ↓ Art-Net U1
AEYLA · CAPTURA
  muestreo determinista a 44 Hz
        ↓
TOMA BRUTA .aeylatake
  fuente original e inmutable
        ↓
AEYLA · EDITOR DMX
  entrada / salida / corte / desplazamiento / mantener / apagón / marcadores
        ↓
CONSOLIDAR
  el punto de entrada pasa a ser 00:00 del archivo final
        ↓
MUESTRA DMX CONSOLIDADA
        ↓
DAW
  audio / MIDI / disparos globales / video / otros complementos
        ↓ notas MIDI
AEYLA · REPRODUCTOR
  selección + reproducir / pausa / siguiente / lanzar
        ↓
ART-NET · SALIDA
  interfaz física elegida
        ↓
NODO U1 → DMX
```

El orden físico de pistas, escenas o clips dentro del DAW **no define la posición artística del DMX**. El DAW actúa como anfitrión y enrutador de eventos; AEYLA reproduce la muestra DMX seleccionada desde su propio cursor relativo.

## 2. Regla principal de tiempo

La reproducción artística no depende de la posición absoluta de la línea de tiempo del DAW y tampoco usa el reloj del sistema como reloj artístico.

Después de consolidar, cada muestra posee una línea de tiempo relativa propia:

```text
0 muestras ───────────────────────── final de la muestra
```

Al recibir REPRODUCIR o LANZAR, AEYLA pone el cursor relativo en cero y avanza contando únicamente las muestras de audio procesadas mientras la muestra DMX se encuentra reproduciendo.

```text
cursorMuestras += muestrasProcesadas
cuadroDMX = floor(cursorMuestras * cuadrosDMXPorSegundo / frecuenciaMuestreo)
```

Un evento MIDI dentro de un bloque debe respetar su desplazamiento exacto dentro del bloque (`sampleOffset` en el código). Las muestras anteriores al evento no pueden adelantar una reproducción recién lanzada.

Consecuencias obligatorias:

- REPRODUCIR / LANZAR inicia la muestra consolidada desde 00:00.
- PAUSA conserva el cursor y mantiene el último cuadro DMX válido.
- REANUDAR continúa desde el mismo cursor relativo.
- REINICIAR vuelve a 00:00 de forma determinista.
- el final de la muestra mantiene el último cuadro válido salvo una política explícita diferente.
- mover, reordenar o renombrar pistas del DAW no desplaza el DMX.
- mover el cursor, crear bucles o saltar en la línea de tiempo global del DAW no reposiciona AEYLA por sí solo.
- únicamente un comando MIDI explícito cambia selección, transporte o posición de la muestra DMX.
- cerrar la interfaz gráfica no puede detener el motor de ejecución.
- un renderizado sin reproducción en tiempo real inhibe siempre la salida física Art-Net.

El reloj del sistema queda prohibido para calcular la posición artística del DMX. Sólo puede utilizarse para supervisión de vida y seguridad.

## 3. Captura

### Entrada

- Art-Net, un universo.
- recepción ligada explícitamente a una IPv4 local elegida.
- tasa normalizada: **44 Hz**.
- una fuente Art-Net queda autoritativa durante cada grabación.

### Persistencia de producción

La grabación de producción no puede acumular la toma completa en RAM.

Ruta requerida:

```text
muestreador 44 Hz
   ↓
cola SPSC fija de 512 KiB
   ↓
hilo dedicado de escritura
   ↓
.aeylatake.tmp
   ↓ DETENER
conteo final + suma de verificación + fsync + validación
   ↓
renombrado atómico
   ↓
.aeylatake
```

- la entrega de cuadros al escritor no asigna memoria, no espera y no hace operaciones de disco en el camino de tiempo crítico.
- desborde de cola = falla visible; nunca se omiten cuadros silenciosamente.
- puntos de control durables periódicos.
- un archivo incompleto nunca reemplaza la última toma válida.

## 4. Reproducción respaldada por archivo

Una toma histórica no se mantiene completa en memoria.

- validación de suma de verificación al abrir.
- acceso aleatorio por índice de cuadro.
- caché fija actual: **128 cuadros / 64 KiB**.
- cargar una muestra no requiere mantener todo el contenido en RAM.
- las operaciones de archivo ocurren fuera del hilo de audio.

La muestra consolidada puede conservar una referencia a la toma bruta más una receta de edición o materializar un archivo optimizado de reproducción. La fuente bruta permanece inmutable.

## 5. Selección y transporte por MIDI

La interfaz de control mínima del show es independiente de la línea de tiempo del DAW.

Comandos visibles mínimos:

- **SELECCIONAR CANCIÓN**: deja una canción preparada.
- **SIGUIENTE CANCIÓN**: avanza la selección preparada.
- **CANCIÓN ANTERIOR**: retrocede la selección preparada.
- **REPRODUCIR / REINICIAR**: inicia la selección desde 00:00.
- **PAUSA**: congela cursor y DMX en el último cuadro válido.
- **REANUDAR**: continúa desde el cursor congelado.
- **DETENER / REINICIAR**: detiene y vuelve al estado preparado.
- **LANZAR CANCIÓN N**: selecciona una canción concreta y la inicia desde 00:00 en el mismo instante MIDI.

**LANZAR CANCIÓN N** es el camino recomendado cuando un disparo global de la sesión debe activar simultáneamente audio, luz, video y otros elementos.

Los números de nota no se fijan en el motor. Deben ser asignables mediante **APRENDER MIDI** para integrarse con una sesión existente sin obligar a reorganizarla.

## 6. Canción activa y canción preparada

Para evitar cortes accidentales, AEYLA distingue dos estados funcionales:

- **ACTIVA**: muestra que está reproduciéndose o en pausa.
- **PREPARADA**: canción seleccionada para el próximo REPRODUCIR / LANZAR.

SIGUIENTE CANCIÓN y SELECCIONAR CANCIÓN modifican la canción PREPARADA sin interrumpir la ACTIVA. REPRODUCIR / LANZAR convierte la PREPARADA en ACTIVA y comienza desde 00:00.

Esto permite preparar la siguiente canción durante la ejecución de la actual sin modificar el DMX que está saliendo.

## 7. Autoridad de salida

La salida Art-Net mantiene dos autoridades mutuamente priorizadas:

1. salida del modelo semántico interno;
2. reproducción de muestra DMX.

Mientras la reproducción DMX está armada y posee una canción ACTIVA válida, la muestra DMX tiene prioridad.

Estados visibles mínimos:

- **DESARMADO**: sin autoridad física de reproducción.
- **ARMADO / LISTO**: sistema preparado; canción PREPARADA disponible.
- **REPRODUCIENDO**: cursor relativo avanzando.
- **PAUSADO / MANTENER**: cursor congelado; conserva el último cuadro.
- **FINALIZADO / MANTENER**: muestra terminada; conserva el último cuadro.
- **SALIDA INHIBIDA**: no se permite transmisión física.
- **FALLA**: error de archivo, anfitrión o almacenamiento; cierre por seguridad.

## 8. DAW y supervisor de vida

El DAW sigue siendo necesario como anfitrión del complemento y fuente de eventos MIDI, pero su posición absoluta no gobierna la muestra DMX.

El bloque de audio cumple tres funciones:

1. entregar la cantidad de muestras procesadas para avanzar el cursor relativo;
2. entregar eventos MIDI con su desplazamiento exacto dentro del bloque;
3. demostrar que el anfitrión sigue vivo.

El reloj del sistema sólo se permite como vigilancia de vida:

- cada bloque válido actualiza la señal de vida;
- si el anfitrión deja de procesar por el umbral de seguridad, AEYLA inhibe la autoridad física;
- la señal de vida jamás calcula qué cuadro artístico corresponde.

La transmisión y el avance no dependen de `OnIdle`, del repintado ni de que la
ventana del complemento permanezca abierta. Minimizar/cerrar la UI sólo detiene
redibujos; el worker de 44 Hz y el cursor por muestras continúan mientras el
anfitrión siga procesando audio. Si el anfitrión suspende esos bloques durante
el umbral de seguridad, AEYLA desarma por seguridad en vez de cambiar a un reloj
visual o inventar tiempo artístico.

Tres errores UDP consecutivos enclavan `fail-closed`, desarman tanto la salida
del modelo como la autoridad de la toma y exigen un rearme explícito. Un error
aislado se informa como transitorio sin interrumpir inmediatamente la toma.

DETENER, mover el cursor o crear un bucle en la línea de tiempo del DAW no reposiciona AEYLA salvo que la sesión envíe además un comando MIDI definido para hacerlo.

## 9. Editor DMX

El editor debe comportarse como editor de una muestra, no como un DAW paralelo.

Primera etapa segura:

- línea de tiempo temporal;
- cursor de reproducción;
- ampliación y desplazamiento;
- punto de entrada;
- punto de salida;
- dividir;
- recortar;
- desplazar;
- mantener estado;
- apagón;
- marcadores;
- actividad DMX resumida;
- volver a la toma bruta;
- consolidar una nueva versión.

Después de CONSOLIDAR, el punto de entrada pasa a ser 00:00 de la muestra final. El reproductor del show no necesita conocer la posición que ese fragmento ocupaba originalmente dentro de la toma bruta.

### Restricción inicial de seguridad

No interpolar linealmente los 512 canales para fabricar transiciones genéricas.

Canales de estrobo, modo, macro u otras funciones discretas podrían atravesar valores no deseados. Las transiciones continuas sólo podrán habilitarse después de clasificar canales y atributos seguros.

## 10. Integración con la sesión existente

La sesión de los músicos puede conservar su estructura actual.

Ejemplo conceptual:

```text
DISPARO GLOBAL · CANCIÓN 06
   ├─ audio / playback existente
   ├─ video / visuales
   ├─ otros disparos
   └─ MIDI → AEYLA: LANZAR CANCIÓN 06
```

AEYLA no exige que CANCIÓN 06 esté en la sexta pista, en una posición fija del arreglo ni en un orden particular de escenas. La identidad de la muestra se resuelve por comando MIDI.

Para sesiones donde ya existe un disparo maestro por canción, LANZAR CANCIÓN N es el camino recomendado. Para operación manual o ensayo también deben existir SELECCIONAR, SIGUIENTE, ANTERIOR, REPRODUCIR, PAUSA, REANUDAR y DETENER.

La consecuencia de producto queda bloqueada: **la sesión de los músicos no debe reorganizarse para acomodar AEYLA. AEYLA se adapta a los disparos existentes.**

## 11. RAM y estabilidad

Objetivos de arquitectura:

- cola de captura: 512 KiB fija;
- caché de lectura: 64 KiB fija;
- históricos: índice y metadatos en disco, no vectores completos de cuadros;
- crecimiento de RAM por duración de toma: aproximadamente plano;
- cambiar de canción o toma no debe acumular contenidos históricos.

La implementación heredada basada en `std::vector<DmxUniverse>` permanece únicamente como compatibilidad temporal y debe salir del camino de producto antes de declarar una versión candidata para show.

## 12. Idioma y lenguaje de producto

**Toda superficie visible para el usuario debe estar en español.** Esta regla es P0 y forma parte del criterio de aceptación del producto.

Incluye:

- nombres de botones;
- estados;
- mensajes de error;
- avisos de seguridad;
- ventanas de configuración;
- selector de red;
- editor DMX;
- biblioteca de canciones y tomas;
- instalador;
- ayuda contextual;
- manual y documentación entregable.

Se permiten en inglés únicamente nombres propios, protocolos, formatos, extensiones y términos técnicos cuya traducción perjudique claridad o compatibilidad, por ejemplo: Art-Net, DMX, MIDI, VST3, AUv2, Ableton Live, REAPER, Avolites, IPv4 y nombres de funciones internas de código.

El código fuente puede conservar identificadores internos en inglés cuando sea técnicamente conveniente; **ninguno de esos identificadores debe filtrarse a la interfaz visible**.

Vocabulario visible bloqueado:

- Capture → **CAPTURA**
- Take → **TOMA**
- Raw Take → **TOMA BRUTA**
- Clip → **MUESTRA DMX** cuando corresponda al archivo reproducible
- Player → **REPRODUCTOR**
- Play → **REPRODUCIR**
- Pause → **PAUSA**
- Resume → **REANUDAR**
- Stop → **DETENER**
- Next Song → **SIGUIENTE CANCIÓN**
- Previous Song → **CANCIÓN ANTERIOR**
- Select Song → **SELECCIONAR CANCIÓN**
- Launch Song → **LANZAR CANCIÓN**
- Active → **ACTIVA**
- Queued → **PREPARADA**
- Armed → **ARMADO**
- Disarmed → **DESARMADO**
- Hold → **MANTENER**
- Fault → **FALLA**
- Blackout → **APAGÓN**
- MIDI Learn → **APRENDER MIDI**

No se incorporarán nuevas etiquetas visibles en inglés sin una razón técnica explícita.

## 13. P0 antes de prueba oficial

1. conectar la captura principal al camino directo a disco;
2. conectar el reproductor principal al cursor relativo gobernado por comandos MIDI;
3. respetar el desplazamiento exacto de eventos MIDI dentro de cada bloque;
4. implementar ACTIVA / PREPARADA + SELECCIONAR / SIGUIENTE / ANTERIOR / REPRODUCIR / PAUSA / REANUDAR / DETENER / LANZAR CANCIÓN;
5. eliminar la caché de múltiples tomas completas por canción;
6. repetir salida Art-Net física con interfaz seleccionada, nodo U1 y DMX real;
7. verificar funcionamiento con la interfaz gráfica cerrada;
8. campañas repetidas LANZAR / PAUSA / REANUDAR / REINICIAR / SIGUIENTE / guardar / reabrir;
9. pérdida y recuperación de red o nodo sin cierre inesperado;
10. falla de almacenamiento visible y cierre por seguridad;
11. auditar todas las cadenas visibles y eliminar inglés de la interfaz.

## 14. Pruebas de resistencia

- sesión completa de 10 canciones;
- campañas repetidas CAPTURAR / DETENER / CARGAR / CONSOLIDAR / LANZAR;
- 50 minutos de captura y reproducción con RAM aproximadamente plana;
- disparos MIDI simultáneos con otros elementos del show;
- prueba prolongada mínima de 8 horas;
- medir RAM, CPU, hilos, identificadores abiertos, errores de transmisión y cuadros obsoletos descartados.

## 15. Estado congelado de esta revisión

Las primitivas de captura directa a disco y lectura respaldada por archivo ya existen en la rama de desarrollo. La arquitectura anterior que vinculaba la posición artística del DMX a la posición absoluta de la línea de tiempo del DAW queda **descartada**.

La dirección válida desde R07 es:

**Avolites programa → AEYLA captura una toma bruta → AEYLA edita y consolida una muestra DMX → la sesión existente envía comandos MIDI → AEYLA reproduce desde un cursor relativo propio → Art-Net sale por la interfaz física seleccionada → nodo U1 → DMX.**

Esta revisión congela además el español como idioma visible obligatorio del producto.

Todavía no se declara la rama lista para show. La aceptación depende de integrar este flujo en el complemento visible, probarlo en REAPER y Ableton Live con la sesión real, verificar interfaz física + nodo + luminarias y completar las pruebas prolongadas.
