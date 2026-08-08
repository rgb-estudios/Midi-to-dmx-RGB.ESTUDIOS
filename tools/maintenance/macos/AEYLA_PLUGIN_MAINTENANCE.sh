#!/usr/bin/env bash
set -euo pipefail

ACTION="${1:-audit}"
SOURCE_VST3="${2:-}"
SOURCE_AU="${3:-}"

BUNDLE_VST3="AeylaVisualDmx.vst3"
BUNDLE_AU="AeylaVisualDmx.component"

log() { printf '[AEYLA] %s\n' "$*"; }

assert_hosts_closed() {
  if pgrep -f '(^|/)(REAPER|Logic Pro)( |$)|Ableton Live' >/dev/null 2>&1; then
    echo "ERROR: cierra REAPER, Ableton o Logic antes de mantener AEYLA." >&2
    exit 2
  fi
}

remove_exact() {
  local path="$1"
  if [[ ! -e "$path" ]]; then
    log "No instalado: $path"
    return
  fi

  log "Eliminando SOLO AEYLA: $path"
  case "$path" in
    /Library/*)
      sudo rm -rf -- "$path"
      ;;
    "$HOME"/*)
      rm -rf -- "$path"
      ;;
    *)
      echo "ERROR: ruta fuera del contrato AEYLA: $path" >&2
      exit 3
      ;;
  esac
}

audit() {
  log "Auditando instalaciones AEYLA conocidas"
  local paths=(
    "$HOME/Library/Audio/Plug-Ins/VST3/$BUNDLE_VST3"
    "/Library/Audio/Plug-Ins/VST3/$BUNDLE_VST3"
    "$HOME/Library/Audio/Plug-Ins/Components/$BUNDLE_AU"
    "/Library/Audio/Plug-Ins/Components/$BUNDLE_AU"
  )

  local path
  for path in "${paths[@]}"; do
    if [[ -e "$path" ]]; then
      log "ENCONTRADO: $path"
    else
      log "ausente: $path"
    fi
  done
}

clean() {
  assert_hosts_closed
  remove_exact "$HOME/Library/Audio/Plug-Ins/VST3/$BUNDLE_VST3"
  remove_exact "/Library/Audio/Plug-Ins/VST3/$BUNDLE_VST3"
  remove_exact "$HOME/Library/Audio/Plug-Ins/Components/$BUNDLE_AU"
  remove_exact "/Library/Audio/Plug-Ins/Components/$BUNDLE_AU"
  log "No se han borrado caches globales de VST3/AU ni archivos .aeylashow."
}

install() {
  assert_hosts_closed

  if [[ -z "$SOURCE_VST3" && -z "$SOURCE_AU" ]]; then
    echo "ERROR: install requiere al menos una fuente VST3 o AU." >&2
    exit 4
  fi

  if [[ -n "$SOURCE_VST3" ]]; then
    if [[ ! -d "$SOURCE_VST3" || "$(basename "$SOURCE_VST3")" != "$BUNDLE_VST3" ]]; then
      echo "ERROR: fuente VST3 invalida: $SOURCE_VST3" >&2
      exit 5
    fi
    local vst3_root="$HOME/Library/Audio/Plug-Ins/VST3"
    mkdir -p "$vst3_root"
    rm -rf -- "$vst3_root/$BUNDLE_VST3"
    cp -R "$SOURCE_VST3" "$vst3_root/$BUNDLE_VST3"
    log "VST3 instalado para el usuario: $vst3_root/$BUNDLE_VST3"
  fi

  if [[ -n "$SOURCE_AU" ]]; then
    if [[ ! -d "$SOURCE_AU" || "$(basename "$SOURCE_AU")" != "$BUNDLE_AU" ]]; then
      echo "ERROR: fuente AU invalida: $SOURCE_AU" >&2
      exit 6
    fi
    local au_root="$HOME/Library/Audio/Plug-Ins/Components"
    mkdir -p "$au_root"
    rm -rf -- "$au_root/$BUNDLE_AU"
    cp -R "$SOURCE_AU" "$au_root/$BUNDLE_AU"
    log "AU instalado para el usuario: $au_root/$BUNDLE_AU"
  fi
}

case "$ACTION" in
  audit)
    audit
    ;;
  clean)
    clean
    audit
    ;;
  install)
    install
    audit
    ;;
  clean-install)
    clean
    install
    audit
    ;;
  *)
    echo "Uso: $0 {audit|clean|install|clean-install} [AeylaVisualDmx.vst3] [AeylaVisualDmx.component]" >&2
    exit 64
    ;;
esac

log "Operacion terminada. No se eliminan shows ni proyectos del DAW."
