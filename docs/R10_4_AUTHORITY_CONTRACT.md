# R10.4 PRETEST — contrato de autoridad física

Estado: PRETEST / NO SHOW READY / NO MERGE A MAIN.

## Regla operacional

Después de un ARM explícito, la autoridad física Art-Net debe permanecer activa durante operación normal del show.

NO pueden desarmar ni reactivar APAGÓN TOTAL:
- cambiar de pestaña/workspace del plugin;
- cerrar/reabrir o perder foco de la ventana del editor;
- seleccionar PREPARADA, PREV o NEXT;
- cambiar entre canciones preparadas;
- PLAY, PAUSA, HOLD o STOP normal del DAW;
- silencio temporal de ProcessBlock mientras el último estado conocido del DAW es detenido/hold.

Sólo pueden retirar autoridad en operación normal:
- DESARMAR explícito del operador;
- APAGÓN TOTAL/PANIC explícito.

Persisten como fail-closed excepcionales:
- render offline;
- backend Art-Net realmente no disponible / fail-closed de socket;
- proyecto inválido;
- runtime fault;
- shutdown;
- cambio físico de red TX que requiera reconstruir el socket.

## PREPARADA vs AL AIRE

Seleccionar otra canción sólo cambia PREPARADA. No toca la toma AL AIRE, no modifica ARM, no activa blackout y no retira el carrier. La sustitución de AL AIRE ocurre únicamente por una acción explícita de reproducción/GO.

## Gate físico R10.4

ARM una sola vez y repetir al menos 10 ciclos de cambio de pestaña/ventana + PREV/NEXT/selección directa + PLAY/HOLD/STOP. Durante todo el bloque el receptor Art-Net debe conservar paquetes, ARM debe seguir activo y APAGÓN TOTAL debe permanecer desactivado. Después, DESARMAR y PANIC deben retirar autoridad de forma inmediata y determinista.
