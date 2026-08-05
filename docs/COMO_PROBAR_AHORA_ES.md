# Cómo probar el paquete actual

## Prototipo visual

En Windows:

1. Descomprimir el repositorio.
2. Abrir `prototype/ui/index.html` con Chrome, Edge u Opera.
3. Probar:
   - selección de `Crimson Field`, `Cold Wave`, `Infernal Noise` y `Deep Red`;
   - cambio de rig 10 a 14;
   - selección de un punto de luminaria sobre el canvas;
   - controles de intensidad, velocidad, extracción de blanco, ámbar y UV;
   - botón `EDIT` del perfil;
   - flash blanco momentáneo;
   - blackout momentáneo;
   - ARM OUTPUT visual;
   - exportar/importar JSON de prototipo.

Este prototipo no transmite DMX. Su función es validar el diseño visual y el flujo de interacción.

## Núcleo C++

Requiere CMake y compilador C++20:

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Las pruebas verifican:

- reordenamiento semántico de canales;
- multiplicación de color cuando no existe dimmer físico;
- rig 10 dentro de 14 posiciones lógicas;
- estructura del paquete ArtDMX.

## Qué observar y reportar

Para la revisión visual:

- qué información sobra;
- qué controles faltan;
- qué nombres no se entienden;
- tamaño y posición de executors;
- legibilidad a distancia;
- si el canvas se siente suficientemente importante;
- si la edición de perfiles parece rápida ante un cambio de luminaria;
- si el flujo de exportar/corregir es comprensible.

No reportar como defecto que todavía no envíe DMX o no abra Ableton: esas funciones corresponden a etapas posteriores ya documentadas.
