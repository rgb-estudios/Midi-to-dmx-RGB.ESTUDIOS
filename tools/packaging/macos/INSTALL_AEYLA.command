#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
exec "$SCRIPT_DIR/AEYLA_PLUGIN_MAINTENANCE.sh" clean-install \
  "$SCRIPT_DIR/VST3/AeylaVisualDmx.vst3" \
  "$SCRIPT_DIR/AUv2/AeylaVisualDmx.component"
