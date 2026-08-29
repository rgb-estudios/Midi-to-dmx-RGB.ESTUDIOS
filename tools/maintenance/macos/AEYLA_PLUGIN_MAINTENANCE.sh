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

verify_bundle() {
  local path="$1"
  if [[ ! -e "$path" ]]; then
    echo "ERROR: bundle AEYLA ausente despues de instalar: $path" >&2
    exit 7
  fi
  if command -v xattr >/dev/null 2>&1; then
    # Internal PRETEST artifacts can inherit browser/chat quarantine. Clear it
    # only on the exact AEYLA bundle; never touch the host, other plug-ins or a
    # global plug-in directory.
    xattr -dr com.apple.quarantine "$path" 2>/dev/null || true
  fi
  if command -v codesign >/dev/null 2>&1; then
    codesign --verify --deep --strict --verbose=2 "$path"
    log "Firma del bundle verificada: $path"
  fi
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
      if command -v codesign >/dev/null 2>&1; then
        codesign --verify --deep --strict "$path" >/dev/null 2>&1 \
          && log "  firma: OK" \
          || log "  firma: INVALIDA / revisar instalacion"
      fi
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
    local vst3_target="$vst3_root/$BUNDLE_VST3"
    mkdir -p "$vst3_root"
    rm -rf -- "$vst3_target"
    /usr/bin/ditto "$SOURCE_VST3" "$vst3_target"
    verify_bundle "$vst3_target"
    log "VST3 instalado para el usuario: $vst3_target"
  fi

  if [[ -n "$SOURCE_AU" ]]; then
    if [[ ! -d "$SOURCE_AU" || "$(basename "$SOURCE_AU")" != "$BUNDLE_AU" ]]; then
      echo "ERROR: fuente AU invalida: $SOURCE_AU" >&2
      exit 6
    fi
    local au_root="$HOME/Library/Audio/Plug-Ins/Components"
    local au_target="$au_root/$BUNDLE_AU"
    mkdir -p "$au_root"
    rm -rf -- "$au_target"
    /usr/bin/ditto "$SOURCE_AU" "$au_target"
    verify_bundle "$au_target"
    log "AU instalado para el usuario: $au_target"
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
