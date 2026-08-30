# AEYLA R10 — FIELD PRETEST ES

**Estado:** PRETEST · NO SHOW CANDIDATE · NO SHOW READY

Este gate valida el binario físico de R10 sobre la arquitectura R09.2. Debe ejecutarse con un receptor Art-Net real o una captura de red verificable. Un PASS de CI no sustituye estas pruebas.

## 0. Identidad del build

1. Abrir `BUILD_ID.txt`.
2. Confirmar `SOURCE_COMMIT` igual al commit del paquete probado.
3. Confirmar `HOST_STATE_SCHEMA=1.4`.
4. Confirmar `STATUS=PRETEST_NOT_SHOW_CANDIDATE`.
5. Verificar `SHA256SUMS.txt` antes de instalar.

## 1. Instalación limpia

### Windows / REAPER
1. Cerrar REAPER, Ableton y cualquier host VST3.
2. Ejecutar `CLEAN_INSTALL_AEYLA.cmd`.
3. Abrir REAPER y hacer rescan VST3 si es necesario.
4. Confirmar en la interfaz `R10 PRETEST`.

### macOS / Ableton
1. Cerrar Ableton, Logic y cualquier host de audio.
2. Ejecutar el instalador incluido para VST3/AUv2.
3. Confirmar arquitectura universal arm64 + x86_64 y que el host carga una sola instancia del build esperado.

## 2. Red y carrier continuo — gate crítico R09.2

Preparación:
- Configurar RX desde Avolites y TX Art-Net hacia el nodo/receptor de prueba.
- APAGÓN debe salir sólo por acción explícita.
- ARM debe ser siempre manual.

Prueba:
1. Con DAW detenido, desactivar APAGÓN y pulsar `ARMAR SALIDA`.
2. Confirmar paquetes ArtDMX continuos a ~44 Hz durante al menos 10 s sin reproducción.
3. Iniciar PLAY.
4. Detener/pausar el DAW y dejarlo detenido **más de 750 ms**.
5. Confirmar que:
   - ARM sigue activo;
   - el socket/endpoint no desaparece;
   - el receptor sigue recibiendo ArtDMX;
   - el último frame DMX queda en HOLD;
   - no se requiere re-ARM.
6. Reanudar PLAY y confirmar continuidad.
7. Repetir el ciclo PLAY → STOP/PAUSE >750 ms → PLAY **3 veces**.

**FAIL inmediato:** cualquier STOP/PAUSE normal del DAW desarma Art-Net, genera blackout no solicitado o requiere rearmar.

## 3. Pérdida real de reloj / fail-closed

1. Dejar el último estado conocido del host en RUNNING=true.
2. Provocar pérdida real del callback/reloj del host.
3. Confirmar que AEYLA entra en fail-closed y retira autoridad.
4. Confirmar que la recuperación de autoridad requiere ARM explícito.

No confundir esta prueba con un STOP/PAUSE normal, que debe conservar carrier.

## 4. Captura R09.1 — ZERO explícito sin MTC

Contrato:
- REC START y REC STOP son comandos separados e idempotentes.
- Defaults: N42 = REC START, N43 = REC STOP.
- Ambos son aprendibles/persistentes de forma independiente.
- N41 permanece PANIC.
- AEYLA no depende de MTC para el ZERO de captura.

Prueba:
1. Seleccionar canción y biblioteca.
2. Confirmar Art-Net RX válido desde Avolites.
3. Con DAW detenido, ejecutar REC START.
4. Confirmar que el RAW comienza exactamente en ese comando y que repetir START no reinicia ZERO.
5. Ejecutar el show/captura.
6. Ejecutar REC STOP.
7. Confirmar que repetir STOP sin captura activa se ignora y nunca se transforma en START.
8. Confirmar que el archivo RAW `.aeylatake` se conserva sin modificación destructiva.

## 5. Aislamiento de tomas por canción

1. Canción A: grabar `Toma A1`.
2. Canción B: grabar `Toma B1` con contenido DMX claramente distinto.
3. Volver a A: grabar `Toma A2` diferente de A1.
4. Revisar biblioteca:
   - A debe mostrar sus versiones A1/A2;
   - B debe mantener B1;
   - ninguna grabación debe sobrescribir archivos de otra canción.
5. Reproducir A → B → A y confirmar que cada canción carga su archivo/rango correcto.
6. Cerrar/reabrir la sesión y repetir A → B → A.

## 6. PREPARADA vs AL AIRE

1. Armar y reproducir la canción A.
2. Seleccionar NEXT o hacer click en canción B.
3. Confirmar que B queda **PREPARADA** y A permanece **AL AIRE**.
4. Confirmar que PREV/NEXT por sí solos nunca reemplazan el frame/toma activa.
5. Ejecutar PLAY/GO explícito para B.
6. Confirmar cambio atómico de autoridad a B, sin mezcla accidental ni salto a otra versión.
7. Repetir B → C → B.

## 7. Memorias EN VIVO — aprendizaje seguro desde Avolites

Memorias iniciales:
- FRONTAL
- HUMO / HAZE
- BASE BLANCA
- TEST LUMINARIAS

Para cada memoria:
1. En Avolites dejar **la memoria a aprender en OFF** y mantener el resto del estado normal.
2. En AEYLA pulsar `APRENDER` — este es el snapshot baseline.
3. Activar **sólo esa memoria** en Avolites.
4. Pulsar `APRENDER` nuevamente — este es el snapshot target.
5. Confirmar que AEYLA informa una máscara de canales plausible, no 512 canales.
6. Volver Avolites al estado normal antes de probar la memoria desde AEYLA.
7. Con una canción reproduciéndose, activar la memoria AEYLA.
8. Confirmar que sólo sus canales cambian y el resto de la canción permanece intacto.
9. Probar fades 0.1 / 1.0 / 1.5 s.
10. En memorias tipo FADER, recorrer 0 → 25 → 50 → 100 → 0 % y verificar continuidad.

**FAIL inmediato:** una memoria altera canales que no cambiaron entre los snapshots OFF/ON.

## 8. Overlap / reveal

1. Crear dos memorias que compartan al menos un canal de prueba.
2. Activar la primera.
3. Activar la segunda: la más reciente debe ganar en el canal compartido.
4. Desactivar la segunda con fade.
5. Confirmar que revela suavemente la primera o el valor actual de la canción/HOLD; no debe caer forzosamente a cero.

## 9. DISARM, PANIC y seguridad

1. Activar una memoria EN VIVO.
2. Ejecutar DISARM.
3. Confirmar que todas las memorias vuelven a nivel OFF.
4. Re-ARM: ninguna memoria debe reaparecer automáticamente.
5. Activar memoria nuevamente y ejecutar N41 PANIC / APAGÓN.
6. Confirmar:
   - blackout + desarme;
   - memorias en OFF;
   - ninguna acción MIDI puede ARMAR o quitar APAGÓN;
   - salir de APAGÓN no auto-arma.

## 10. Persistencia — pendiente R10

Mientras el bloque de persistencia R10 no esté implementado y validado, las definiciones de memoria EN VIVO deben considerarse temporales para la vida de la instancia. **No aprobar Show Candidate** basándose en memorias hasta que el schema/persistencia y el MIDI Learn estén cerrados.

## Criterio de salida

R10 puede pasar de PRETEST técnico a candidato de campo sólo si:
- todos los gates anteriores pasan en REAPER/Windows;
- se repiten carrier, PREPARADA/AL AIRE y PANIC en Ableton Live 12/macOS;
- no aparecen xruns/errores de red atribuibles al plugin;
- MIDI Learn y persistencia R10 están implementados y tienen sus propios gates.

Hasta entonces: **PRETEST_NOT_SHOW_CANDIDATE**.
