# AEYLA — Punto de control R07 · 25 agosto 2026

Estado: **consolidado para continuar desarrollo sin reabrir decisiones ya tomadas**.

## Objetivo inmediato

Llegar a las pruebas oficiales con un complemento estable capaz de:

- recibir Art-Net desde Avolites;
- grabar un universo DMX sin crecimiento lineal de RAM;
- conservar tomas históricas sin cargarlas completas en memoria;
- editar una toma como una muestra temporal;
- consolidar una versión final por canción;
- seleccionar y disparar canciones por MIDI;
- reproducir con precisión derivada de muestras, pero sin depender de la posición absoluta del arreglo del DAW;
- transmitir Art-Net por una interfaz física seleccionada;
- funcionar durante periodos prolongados sin cierres inesperados ni crecimiento anormal de recursos;
- presentar toda la interfaz y documentación visible en español.

## Arquitectura congelada

```text
AVOLITES
   ↓ Art-Net U1
AEYLA · CAPTURA
   ↓ 44 Hz
TOMA BRUTA .aeylatake
   ↓
EDITOR DMX
   ↓ entrada / salida / cortes / desplazamientos / mantener / apagón / marcadores
CONSOLIDAR
   ↓
MUESTRA DMX FINAL DE CANCIÓN
   ↓
SESIÓN DAW EXISTENTE
   ↓ nota MIDI
AEYLA · REPRODUCTOR
   ↓ cursor relativo propio
ART-NET · INTERFAZ FÍSICA SELECCIONADA
   ↓
NODO U1
   ↓
DMX / LUMINARIAS
```

## Decisión crítica de transporte

Queda descartada la dependencia de la posición absoluta del arreglo del DAW.

AEYLA recibe comandos MIDI y mantiene un cursor relativo de reproducción por muestra DMX.

- REPRODUCIR / LANZAR: cursor = 0.
- PAUSA: cursor congelado y último cuadro DMX mantenido.
- REANUDAR: continúa desde el cursor congelado.
- REINICIAR: vuelve a 0.
- DETENER / REINICIAR: termina reproducción y vuelve a estado preparado.

El cursor avanza por cantidad de muestras procesadas, no por reloj de pared. Esto conserva precisión sin obligar a que las canciones estén en una pista, escena u orden específico dentro de Ableton Live o REAPER.

## Integración MIDI congelada

Comandos mínimos visibles:

- SELECCIONAR CANCIÓN;
- SIGUIENTE CANCIÓN;
- CANCIÓN ANTERIOR;
- REPRODUCIR / REINICIAR;
- PAUSA;
- REANUDAR;
- DETENER / REINICIAR;
- LANZAR CANCIÓN N.

LANZAR CANCIÓN N es el camino preferido para el show porque puede compartir el mismo disparo global que ya utiliza la sesión para audio, video y otros elementos.

Las notas deben ser configurables mediante APRENDER MIDI. No se obliga a modificar la estructura de la sesión existente.

## Estado ACTIVA / PREPARADA

- ACTIVA = canción que está reproduciéndose o pausada.
- PREPARADA = canción seleccionada para el próximo lanzamiento.

Cambiar PREPARADA nunca debe cortar ACTIVA. El cambio sólo se hace efectivo al recibir REPRODUCIR o LANZAR.

## Editor DMX congelado

El editor es un editor de muestra DMX, no un DAW paralelo.

Primera versión requerida:

- línea de tiempo;
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
- resumen visual de actividad DMX;
- volver a la toma bruta;
- consolidar nueva versión.

Después de consolidar, el punto de entrada se convierte en 00:00 de la muestra final.

No se permiten transiciones genéricas interpolando indiscriminadamente los 512 canales hasta clasificar canales seguros.

## Memoria y persistencia

Ruta de producción objetivo:

```text
muestreador 44 Hz
   ↓
cola SPSC fija 512 KiB
   ↓
hilo de escritura
   ↓
archivo temporal
   ↓
validación + suma de verificación + cierre durable
   ↓
.aeylatake final
```

Lectura:

- archivo permanece en disco;
- acceso por índice;
- caché fija actual de 128 cuadros / 64 KiB;
- no mantener múltiples tomas completas en RAM.

El camino heredado basado en vectores completos permanece únicamente de forma transitoria y debe desaparecer antes de declarar una versión candidata para show.

## Idioma bloqueado

Toda superficie visible del producto debe estar en español.

Esto incluye botones, estados, errores, seguridad, red, editor, biblioteca, instalador, ayuda y manual.

Se mantienen sin traducir sólo nombres propios y términos técnicos necesarios para compatibilidad: Art-Net, DMX, MIDI, VST3, AUv2, IPv4, Ableton Live, REAPER, Avolites y nombres internos de código que nunca sean visibles al usuario.

Vocabulario visible base:

- CAPTURA
- TOMA
- TOMA BRUTA
- MUESTRA DMX
- REPRODUCTOR
- REPRODUCIR
- PAUSA
- REANUDAR
- DETENER
- SIGUIENTE CANCIÓN
- CANCIÓN ANTERIOR
- SELECCIONAR CANCIÓN
- LANZAR CANCIÓN
- ACTIVA
- PREPARADA
- ARMADO
- DESARMADO
- MANTENER
- FALLA
- APAGÓN
- APRENDER MIDI

## Lo que ya existe en la rama

- escritor directo a disco con memoria acotada;
- cola fija de captura;
- validación y finalización segura de tomas;
- lector respaldado por archivo con caché fija;
- pruebas de lectura aleatoria;
- base del reproductor de muestra DMX;
- corrección de arquitectura desde posición absoluta del DAW hacia cursor relativo;
- PR de desarrollo aislado para no comprometer el baseline anterior.

## P0 antes de prueba oficial

1. Conectar la captura directa a disco al flujo visible del complemento.
2. Conectar el cursor relativo al flujo visible del reproductor.
3. Integrar desplazamiento exacto de eventos MIDI dentro del bloque de audio.
4. Implementar ACTIVA / PREPARADA y todos los comandos MIDI definidos.
5. Eliminar caché de tomas completas del flujo principal.
6. Completar auditoría de idioma visible y eliminar etiquetas en inglés.
7. Repetir salida Art-Net con interfaz física + nodo U1 + luminarias.
8. Probar REAPER y Ableton Live con la interfaz gráfica abierta y cerrada.
9. Probar pérdida y recuperación de interfaz, cable y nodo sin cierre inesperado.
10. Ejecutar campaña prolongada: mínimo 50 minutos de captura/reproducción y objetivo de 8 horas de resistencia.

## Criterio de no regresión

No volver a:

- sincronía artística por posición absoluta del arreglo del DAW;
- cronómetro de pared como reloj artístico;
- carga completa de todos los archivos DMX en RAM;
- dependencia del orden de pistas o escenas;
- notas MIDI fijas no configurables;
- cambio de canción preparada que interrumpa la canción activa;
- interfaz visible mezclando español e inglés.

## Próxima dirección de desarrollo

La prioridad no es estética adicional. La secuencia inmediata es:

**captura directa a disco integrada → transporte relativo MIDI integrado → biblioteca sin tomas completas en RAM → salida física repetida → pruebas de anfitrión → auditoría de idioma → editor visual avanzado → resistencia prolongada.**

Este documento se considera el punto de recuperación principal de R07 si el trabajo debe retomarse en otro chat o sesión.