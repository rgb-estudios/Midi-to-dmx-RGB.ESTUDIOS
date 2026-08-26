# AEYLA · Estado de Gates R07

Fecha de consolidación: 25/26 agosto 2026

## Arquitectura congelada

Avolites → Art-Net U1 → AEYLA CAPTURA → TOMA ORIGINAL `.aeylatake` → EDITOR DMX → CONSOLIDAR CLIP → comandos MIDI del DAW → reproductor relativo AEYLA → Art-Net 44 Hz → NIC física → nodo → DMX.

La posición absoluta del Arrangement de Ableton Live/REAPER no gobierna el clip. El DAW entrega bloques de audio y comandos MIDI; AEYLA conserva un cursor relativo propio.

Toda superficie visible para operador debe quedar en español, salvo nombres técnicos inevitables: Art-Net, DMX, MIDI, VST3, IPv4, Ableton Live y REAPER.

---

## GATE A · TRANSMISIÓN ART-NET · P0

### Implementado en código

- [x] Cadencia contractual de TX fijada a 44 Hz incluso frente a rutas heredadas de 30/40 Hz.
- [x] Bind a IPv4 local seleccionada para TX.
- [x] Worker de red separado del callback de audio.
- [x] Paquete ArtDMX generado en buffer fijo; sin vector/heap por cada envío.
- [x] Deadline acumulativo de transmisión para evitar deriva por `now + period`.
- [x] Métricas internas: paquetes enviados, blackout, errores, errores consecutivos, deadlines perdidos, fail-closed, generaciones obsoletas.
- [x] Tres errores explícitos consecutivos → desarme de ambas autoridades y fail-closed.
- [x] Rearme posterior a fail-closed requiere una acción explícita de habilitación.
- [x] Desarme/cierre → ráfaga de 3 cuadros BLACKOUT, no un único datagrama.
- [x] Lease process-local para impedir dos instancias AEYLA sobre el mismo destino/universo.
- [x] Test automatizado específico R07 para normalización 44 Hz y ráfaga BLACKOUT.

### Pendiente antes de cerrar Gate A

- [ ] CI completo verde en Windows/macOS/Linux para el último commit.
- [ ] Mostrar métricas TX críticas en interfaz en español.
- [ ] Test físico: REAPER → AEYLA → NIC seleccionada → nodo U1 → DMX/luminaria.
- [ ] Validar unicast y broadcast dirigido con hardware real.
- [ ] Desconectar cable/NIC/nodo durante reproducción y comprobar comportamiento.
- [ ] Confirmar que `sendto()` del SO refleja o no la pérdida física; si no, añadir verificación de salud de enlace/NIC por separado.
- [ ] Soak 8 h con RAM/CPU/errores registrados.

**Estado Gate A:** EN DESARROLLO / NO CERRADO.

---

## GATE B · EDITOR DE CLIP DMX · P0

### Implementado en código

- [x] TOMA ORIGINAL preservada como archivo independiente.
- [x] Captura directa a disco con RAM acotada.
- [x] Reproductor file-backed con caché fija.
- [x] ENTRADA/SALIDA no destructivos.
- [x] Reproducción relativa por muestras, independiente de la posición absoluta del DAW.
- [x] Backend `CONSOLIDAR CLIP`: copia sólo el rango ENTRADA→SALIDA a un nuevo `.aeylatake`.
- [x] El primer cuadro de ENTRADA pasa a ser 00:00 del consolidado.
- [x] Consolidación file-backed con regulación del buffer; no materializa la canción completa en RAM.
- [x] Reapertura y validación del archivo consolidado: checksum, metadatos, cantidad de cuadros y extremos.
- [x] Test automatizado: preservación byte por byte del RAW, 44 Hz, identidad de Song y exactitud de IN/OUT.
- [x] Botón/acción visible `CONSOLIDAR CLIP` conectado al backend real.
- [x] El consolidado pasa a ser el clip activo y queda preparado para reproducción.

### Pendiente antes de cerrar Gate B

- [ ] Timeline de actividad DMX real, no sólo una barra de duración.
- [ ] Handles ENTRADA/SALIDA arrastrables.
- [ ] Playhead y scrub/preview.
- [ ] Zoom horizontal.
- [ ] Navegación de versiones: TOMA ORIGINAL / tomas / clip consolidado activo.
- [ ] Revertir a RAW en una acción clara.
- [ ] No introducir fades de 512 canales sin clasificación de canales continuos/discretos.
- [ ] Prueba de editor abierto/cerrado mientras la reproducción continúa.

**Estado Gate B:** FLUJO BÁSICO IN/OUT→CONSOLIDAR INTEGRADO / EDITOR AVANZADO AÚN NO CERRADO.

---

## GATE C · MIDI / OPERACIÓN DEL SHOW · P0

Controles congelados:

- SELECCIONAR CANCIÓN
- SIGUIENTE
- ANTERIOR
- REPRODUCIR / RETRIGGER
- PAUSA
- REANUDAR
- DETENER / RESET
- LANZAR CANCIÓN N

Regla: `LANZAR CANCIÓN N` debe poder integrarse en los triggers globales ya existentes de la sesión sin depender del orden físico de tracks ni de la posición del Arrangement.

Pendiente: puente definitivo y configurable/MIDI Learn con orden por `sampleOffset`.

---

## GATE D · ESTABILIDAD / PRUEBA OFICIAL

No se considera Show Candidate hasta aprobar:

1. REAPER Windows + Art-Net físico.
2. Ableton Live Windows/macOS.
3. Editor abierto/cerrado sin afectar runtime.
4. Grabación larga sin crecimiento lineal de RAM.
5. Reproducción y cambio de clips repetidos.
6. BLACKOUT / DISARM / pérdida de host / render offline.
7. Soak mínimo de 8 horas.
8. Revisión de idioma visible 100% español.

## Regla de entrega

No presentar un binario como `LISTO PARA SHOW` mientras un P0 permanezca abierto. Los builds intermedios deben denominarse PRETEST y especificar exactamente qué gate se está probando.
