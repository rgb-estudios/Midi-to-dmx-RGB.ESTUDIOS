# Resumen maestro — AEYLA Visual DMX

## Objetivo

Construir una herramienta que permita diseñar, corregir y exportar iluminación sin comprar Ableton. El computador final utiliza un VST3 dentro de Ableton y recibe notas MIDI; el diseñador trabaja en una aplicación standalone gratuita.

## Dos aplicaciones, un solo motor

### Editor standalone

Se utiliza en el computador del diseñador para:

- crear bloques visuales, gradientes, animaciones, imágenes y videos;
- muestrear esos contenidos en 14 posiciones lógicas de PAR LED;
- trabajar temporalmente con 10 equipos físicos sin perder el diseño de 14;
- crear paletas, looks, escenas y executors;
- asignar notas MIDI;
- definir perfiles semánticos y patch físico;
- probar Art-Net o interfaces USB compatibles;
- exportar un único archivo `.aeylashow`.

No necesita Ableton.

### Runtime VST3

Se instala en el computador que ya tiene Ableton para:

- cargar el archivo `.aeylashow`;
- recibir notas MIDI;
- ejecutar escenas y animaciones;
- convertir atributos semánticos a DMX;
- transmitir Art-Net o USB-DMX compatible;
- ofrecer ARM, blackout, estado de conexión y recarga del show.

No lee ni analiza audio. Puede leer opcionalmente la posición del transporte para mantener una animación o video sincronizado.

## Principio técnico central

La programación nunca conoce el número físico del canal. Produce atributos:

```text
Dimmer · Shutter · Strobe · R · G · B · W · A · UV · Lime
Macro · Speed · Reset · Zoom · Fan · Haze
```

El perfil de la luminaria decide dónde sale cada atributo.

Ejemplo:

```text
Look: Red=100%, Green=20%, Dimmer=70%

Modelo A: Dimmer→1, Red→3, Green→4
Modelo B: Red→1, Green→3, Dimmer→8
```

El look no cambia. Solo cambia el perfil.

## Flujo de correcciones

1. Abrir el último `.aeylashow` aprobado en standalone.
2. Guardar una versión nueva.
3. Corregir color, video, animación, escena, perfil o patch.
4. Validar y previsualizar.
5. Exportar un nuevo `.aeylashow` completo.
6. Enviar el archivo al computador final.
7. El operador reemplaza el archivo y pulsa `Reload Show`.
8. Las notas MIDI y la sesión Ableton siguen iguales, salvo que se cambie deliberadamente el mapa MIDI.
9. Conservar siempre la versión anterior para rollback.

## Estado real actual

Ya existe:

- documentación integral;
- arquitectura y reglas de seguridad;
- núcleo C++ semántico;
- compilador de perfiles a DMX;
- codificador de paquetes ArtDMX;
- pruebas automatizadas de reordenamiento de canales;
- esquema inicial de perfiles y proyectos;
- prototipo visual navegable del editor;
- CI preparado para Windows y Linux;
- instrucciones para futuros agentes.

Todavía no existe:

- aplicación standalone compilada para Windows;
- VST3;
- envío UDP Art-Net real;
- backend USB-DMX;
- runtime de executors/capas completo;
- decodificación de video;
- validación con hardware;
- prueba dentro de Ableton.

Por lo tanto, este paquete es una fundación ejecutable y auditable, no una versión lista para show.

## Orden obligatorio de desarrollo

1. Publicar repositorio privado y ejecutar CI.
2. Terminar el runtime central y sus pruebas.
3. Implementar Art-Net real y probar con nodo.
4. Construir editor standalone alpha.
5. Construir VST3 alpha.
6. Integrar imagen/video.
7. Implementar USB DMX Pro.
8. Probar la sesión final de Ableton.
9. Soak test, fallos y recuperación.
10. Ensayo completo y release de show.

## Criterio visual

El aspecto visual es un requisito funcional:

- canvas dominante;
- interfaz oscura, espacial y limpia;
- executors legibles y siempre accesibles;
- estado de salida y seguridad persistente;
- perfiles editables sin apariencia de planilla genérica;
- rig 10 muestra las cuatro posiciones ausentes como puntos fantasma;
- uso contenido del color, reservado para contenido, estados y alertas.

Consultar `VISUAL_DESIGN_SYSTEM.md` antes de cualquier cambio UI.
