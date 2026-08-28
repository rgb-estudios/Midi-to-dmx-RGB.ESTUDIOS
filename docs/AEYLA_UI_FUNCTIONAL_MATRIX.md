# AEYLA — matriz funcional de UI

Checkpoint: `R07 PRETEST / auditoría integral pendiente de CI`
Regla: visible = funcional, bloqueado con causa o no presente.

| Superficie | Evento → handler → efecto | Persistencia | Estado actual |
|---|---|---:|---|
| NUEVO | clic → `NewProjectFromUI` → bundle nuevo seguro | sí | **Simulated** backend; host por revalidar |
| ABRIR | clic → diálogo → carga transaccional | sí | **Simulated** backend; diálogo host pendiente |
| GUARDAR / GUARDAR COMO | clic → ZIP atómico + read-back + `.bak` | sí | **Simulated** backend; diálogo host pendiente |
| APAGÓN | clic → alterna el enclavamiento global; ON desarma modelo+toma → DMX cero | host/show | **Simulated**; separado del negro artístico de Cue |
| ARMAR SALIDA DE TOMA | clic → gates globales → arma aunque el Song no tenga Cue, o explica causa exacta | ARM nunca persiste | **Simulated**; nodo pendiente |
| TOMA / EDICIÓN | clic → workspace de captura/editor | sesión UI | **Scaffolded**; interacción host pendiente |
| MIDI / SHOW | clic → mapa, canal, Learn, PREPARADA/ACTIVA y diagnóstico | mapa sí; ARM no | **Simulated**; interacción host pendiente |
| RED / SALIDA | clic → workspace de red/telemetría | sesión UI | **Scaffolded**; interacción host pendiente |
| Lista de canciones | clic selecciona; doble clic renombra; nueva hasta 15 | sí | **Scaffolded**; interacción host pendiente |
| GRABAR NUEVA TOMA | clic → captura Art-Net U1 a disco; segundo clic finaliza TOMA BRUTA | sí, archivo | **Simulated** con Art-Net loopback |
| REPRODUCIR / DETENER | clic → cursor relativo por muestras / pausa HOLD | runtime | **Simulated** con reloj host/monotónico |
| Timeline y cabezal | clic/arrastre → scrub libre si está desarmado | edit state | **Scaffolded**; interacción host pendiente |
| Handles / marcar ENTRADA-SALIDA | arrastre, timecode o ±1f → rango no destructivo | edit state | **Scaffolded**; interacción host pendiente |
| Zoom / pan | rueda, `-/+`, `Shift+arrastre` | sesión UI | **Scaffolded**; interacción host pendiente |
| Versiones / volver a TOMA BRUTA | flechas/acción → selecciona archivo y rango reales | sesión | **Simulated** backend; interacción pendiente |
| CONSOLIDAR MUESTRA DMX | clic → nuevo `.aeylatake`, toma bruta byte-identical | sí, archivo | **Simulated** backend |
| Adaptador RX/TX | flechas → selecciona NIC; cambiar TX desarma | sesión | **Scaffolded**; NIC host pendiente |
| IPv4 AEYLA / MÁSCARA | texto → parser estricto → helper UAC → alias/validación/rollback | sistema Windows | **Simulated** protocolo; UAC físico pendiente |
| Telemetría RX/TX/AUTORIDAD | workers → señal/edad/paquetes/saltos/errores/retrasos/fail-closed | no | **Scaffolded**; render host pendiente |
| Runtime con editor cerrado | audio publica muestras/heartbeat; workers de clip y UDP continúan sin `OnIdle` | no | **Simulated**; REAPER real pendiente |
| Offline render | hard disarm + blackout | no | **Simulated**; host pendiente |
| ANTERIOR / SIGUIENTE MIDI | nota → cambia sólo PREPARADA; ACTIVA continúa | mapa MIDI | **Simulated** |
| PLAY / PAUSA / STOP MIDI | nota → transporte relativo con `sampleOffset` exacto | mapa MIDI | **Simulated** |
| LANZAR CANCIÓN 01–15 | nota → selección + cambio precargado sin desarme/blackout | mapa MIDI | **Simulated** |

## Límites visibles de esta entrega

- No hay delete/reorder, undo/redo ni clasificación de canales para fades.
- El Programmer semántico completo permanece fuera de esta superficie R07;
  `MIDI / SHOW` controla únicamente las tomas DMX grabadas.
- Art-Net está conectado al producto, pero no existe evidencia de nodo/PAR
  físico ni detección positiva de recepción.
- Nada en esta matriz autoriza uso de show sin CI actual, hosts reales,
  hardware, soak y ensayo.
