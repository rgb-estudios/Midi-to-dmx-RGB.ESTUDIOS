#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

printf '[AEYLA] Instalación recomendada para Ableton Live: VST3 solamente.\n'
printf '[AEYLA] Cierra Ableton Live antes de continuar.\n'

exec "$SCRIPT_DIR/AEYLA_PLUGIN_MAINTENANCE.sh" clean-install \
  "$SCRIPT_DIR/VST3/AeylaVisualDmx.vst3" \
  ""
