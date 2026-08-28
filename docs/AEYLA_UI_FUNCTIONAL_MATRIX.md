# AEYLA — matriz funcional de UI

Checkpoint: `R07 PRETEST / P0 network-output repair`
Regla: visible = funcional, bloqueado con causa o no presente.

| Superficie | Evento → handler → efecto | Persistencia | Estado actual |
|---|---|---:|---|
| NUEVO | clic → `NewProjectFromUI` → bundle nuevo seguro | sí | **Implemented**; host por revalidar |
| ABRIR | clic → diálogo → carga transaccional | sí | **Implemented**; host por revalidar |
| GUARDAR / GUARDAR COMO | clic → ZIP atómico + read-back + `.bak` | sí | **Implemented**; host por revalidar |
| APAGÓN | clic → desarma modelo+toma → DMX cero | host/show | **Implemented** |
| ARMAR SALIDA DE TOMA | clic → gates → arma o explica causa exacta | ARM nunca persiste | **Implemented / Simulated**; nodo pendiente |
| TOMA / EDICIÓN | clic → workspace de captura/editor | sesión UI | **Implemented** |
| RED / SALIDA | clic → workspace de red/telemetría | sesión UI | **Implemented** |
| Lista de canciones | clic selecciona; doble clic renombra; nueva hasta 15 | sí | **Implemented** |
| GRABAR NUEVA TOMA | clic → captura Art-Net U1 a disco; segundo clic finaliza RAW | sí, archivo | **Implemented / Simulated** |
| REPRODUCIR / DETENER | clic → cursor relativo por muestras / pausa HOLD | runtime | **Implemented / Simulated** |
| Timeline y cabezal | clic/arrastre → scrub libre si está desarmado | edit state | **Implemented** |
| Handles / marcar IN-OUT | arrastre, timecode o ±1f → rango no destructivo | edit state | **Implemented** |
| Zoom / pan | rueda, `-/+`, `Shift+arrastre` | sesión UI | **Implemented** |
| Versiones / volver a RAW | flechas/acción → selecciona archivo y rango reales | sesión | **Implemented** |
| CONSOLIDAR CLIP | clic → nuevo `.aeylatake`, RAW byte-identical | sí, archivo | **Implemented** |
| Adaptador RX/TX | flechas → selecciona NIC; cambiar TX desarma | sesión | **Implemented** |
| IPv4 AEYLA / MÁSCARA | texto → parser estricto → helper UAC → alias/validación/rollback | sistema Windows | **Implemented / Simulated**; UAC físico pendiente |
| Telemetría TX | worker → paquetes/errores/autoridad visible | no | **Implemented** |
| Runtime con editor cerrado | audio publica muestras/heartbeat; workers de clip y UDP continúan sin `OnIdle` | no | **Simulated**; REAPER real pendiente |
| Offline render | hard disarm + blackout | no | **Implemented / Simulated**; host pendiente |
| SHOW MODE | no presente | UI state | **Specified** |

## Límites visibles de esta entrega

- No hay delete/reorder, undo/redo ni clasificación de canales para fades.
- El Programmer semántico y Show Mode permanecen fuera de esta superficie R07;
  no se dibujan como controles disponibles.
- Art-Net está conectado al producto, pero no existe evidencia de nodo/PAR
  físico ni detección positiva de recepción.
- Nada en esta matriz autoriza uso de show sin CI actual, hosts reales,
  hardware, soak y ensayo.
