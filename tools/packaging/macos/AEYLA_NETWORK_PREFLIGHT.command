#!/usr/bin/env bash
set -euo pipefail

printf '\nAEYLA Visual DMX — preflight de red macOS\n'
printf '========================================\n\n'
printf 'Este comando NO cambia la red. Sólo muestra interfaces y direcciones actuales.\n'
printf 'Configura la IPv4 del adaptador Ethernet en Ajustes del Sistema > Red antes de abrir Ableton.\n\n'

if [[ -x /usr/sbin/networksetup ]]; then
  printf 'Hardware detectado:\n'
  /usr/sbin/networksetup -listallhardwareports || true
fi

printf '\nIPv4 activas por dispositivo:\n'
if [[ -x /usr/sbin/networksetup ]]; then
  while IFS= read -r device; do
    [[ -n "$device" ]] || continue
    ip="$(/usr/sbin/ipconfig getifaddr "$device" 2>/dev/null || true)"
    [[ -n "$ip" ]] || continue
    mask_hex="$(/sbin/ifconfig "$device" 2>/dev/null | awk '/inet / {for(i=1;i<=NF;i++) if($i=="netmask") print $(i+1); exit}')"
    media="$(/sbin/ifconfig "$device" 2>/dev/null | awk -F'media: ' '/media: / {print $2; exit}')"
    printf '  %-8s  IPv4 %-15s  netmask %-12s  %s\n' "$device" "$ip" "${mask_hex:-?}" "${media:-medio no informado}"
  done < <(/usr/sbin/networksetup -listallhardwareports | awk '/Device:/{print $2}')
fi

printf '\nChecklist AEYLA:\n'
printf '  1. Usa Ethernet/USB-Ethernet/Thunderbolt-Ethernet, no Wi-Fi.\n'
printf '  2. La IPv4 debe estar en la misma subred que Avolites/nodo/Capture.\n'
printf '  3. Abre Ableton DESPUÉS de dejar lista la NIC.\n'
printf '  4. En AEYLA pulsa REESCANEAR y confirma RX/TX sobre esa IPv4.\n'
printf '  5. Antes de show, ARM debe producir carrier Art-Net aun con PLAY detenido.\n\n'

read -r -p 'Presiona Enter para cerrar…' _
