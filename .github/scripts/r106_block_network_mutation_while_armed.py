from pathlib import Path

show_path = Path('product/AeylaVisualDmx/AeylaShowPlayerIntegration.cpp')
main_path = Path('product/AeylaVisualDmx/AeylaMainControl.h')

show = show_path.read_text(encoding='utf-8')
main = main_path.read_text(encoding='utf-8')

# Cycle RX/TX: both functions shared this exact precondition before R10.6.
old_cycle = '''  if(direction == 0 || NetworkConfigurationBusy() || TakeRecording())
    return false;
'''
new_cycle = '''  if(direction == 0 || NetworkConfigurationBusy() || TakeRecording() ||
     TakeOutputArmed() || OutputArmed())
    return false;
'''
if show.count(old_cycle) != 2:
    raise SystemExit(f'Expected two Cycle RX/TX guards, found {show.count(old_cycle)}')
show = show.replace(old_cycle, new_cycle)

old_apply = '''  if(TakeRecording())
    return {false, {}, "Detén GRABAR antes de cambiar la red TX"};
  if(NetworkConfigurationBusy())
    return {false, {}, "Espera a que termine el cambio de red actual"};
'''
new_apply = '''  if(TakeRecording())
    return {false, {}, "Detén GRABAR antes de cambiar la red TX"};
  if(TakeOutputArmed() || OutputArmed())
    return {false, {}, "Desarma la salida física antes de cambiar la red TX"};
  if(NetworkConfigurationBusy())
    return {false, {}, "Espera a que termine el cambio de red actual"};
'''
if show.count(old_apply) != 1:
    raise SystemExit(f'Expected one ApplyTxNetwork guard, found {show.count(old_apply)}')
show = show.replace(old_apply, new_apply, 1)

old_route = '''    const bool routeSelectionBlocked =
        networkBusy || mPlug.TakeRecording();
'''
new_route = '''    const bool physicalAuthority = output.enabled || output.override_enabled;
    const bool routeSelectionBlocked =
        networkBusy || mPlug.TakeRecording() ||
        mPlug.TakeOutputArmed() || mPlug.OutputArmed() || physicalAuthority;
'''
if main.count(old_route) != 1:
    raise SystemExit(f'Expected routeSelectionBlocked once, found {main.count(old_route)}')
main = main.replace(old_route, new_route, 1)

old_apply_block = '''    const bool networkApplyBlocked =
        networkBusy || mPlug.TakeRecording();
    Button(g, mApplyNetworkButton,
           networkBusy ? "APLICANDO RED · ESPERA" :
               (mPlug.TakeRecording() ? "BLOQUEADA · GRABANDO" :
                                        "APLICAR IP Y PREPARAR ART-NET"),
'''
new_apply_block = '''    const bool networkApplyBlocked =
        networkBusy || mPlug.TakeRecording() ||
        mPlug.TakeOutputArmed() || mPlug.OutputArmed() || physicalAuthority;
    Button(g, mApplyNetworkButton,
           networkBusy ? "APLICANDO RED · ESPERA" :
               (mPlug.TakeRecording() ? "BLOQUEADA · GRABANDO" :
                  (physicalAuthority || mPlug.TakeOutputArmed() || mPlug.OutputArmed()
                      ? "BLOQUEADA · SALIDA ARMADA"
                      : "APLICAR IP Y PREPARAR ART-NET")),
'''
if main.count(old_apply_block) != 1:
    raise SystemExit(f'Expected networkApplyBlocked once, found {main.count(old_apply_block)}')
main = main.replace(old_apply_block, new_apply_block, 1)

# physicalAuthority was previously declared later in DrawRouting. Reuse the
# earlier declaration so the rendering and mutation lock share one truth.
old_late_authority = '''    const bool physicalAuthority = output.enabled || output.override_enabled;
    char transmission[220];
'''
new_late_authority = '''    char transmission[220];
'''
if main.count(old_late_authority) != 1:
    raise SystemExit(f'Expected one late physicalAuthority declaration, found {main.count(old_late_authority)}')
main = main.replace(old_late_authority, new_late_authority, 1)

old_apply_simple = '''  void ApplySimpleNetwork()
  {
    if(mPlug.NetworkConfigurationBusy())
    {
      mMessage = mPlug.NetworkConfigurationStatus();
      return;
    }
'''
new_apply_simple = '''  void ApplySimpleNetwork()
  {
    if(mPlug.NetworkConfigurationBusy())
    {
      mMessage = mPlug.NetworkConfigurationStatus();
      return;
    }
    if(mPlug.TakeOutputArmed() || mPlug.OutputArmed() ||
       mPlug.ArtNetOutputStatus().enabled ||
       mPlug.ArtNetOutputStatus().override_enabled)
    {
      mMessage = "Desarma la salida antes de aplicar cambios de red.";
      return;
    }
'''
if main.count(old_apply_simple) != 1:
    raise SystemExit(f'Expected ApplySimpleNetwork once, found {main.count(old_apply_simple)}')
main = main.replace(old_apply_simple, new_apply_simple, 1)

# RX previous/next failure text: there are two identical blocks.
old_rx_msg = '''        mMessage = mPlug.NetworkConfigurationBusy()
            ? "Espera a que termine el cambio de red actual."
            : (mPlug.TakeRecording()
                ? "No se puede cambiar el adaptador RX mientras se graba."
                : "No se detectaron adaptadores RX seleccionables.");
'''
new_rx_msg = '''        mMessage = mPlug.NetworkConfigurationBusy()
            ? "Espera a que termine el cambio de red actual."
            : (mPlug.TakeRecording()
                ? "No se puede cambiar el adaptador RX mientras se graba."
                : ((mPlug.TakeOutputArmed() || mPlug.OutputArmed())
                    ? "Desarma la salida antes de cambiar el adaptador RX."
                    : "No se detectaron adaptadores RX seleccionables."));
'''
if main.count(old_rx_msg) != 2:
    raise SystemExit(f'Expected two RX failure blocks, found {main.count(old_rx_msg)}')
main = main.replace(old_rx_msg, new_rx_msg)

old_tx_msg = '''        mMessage = mPlug.NetworkConfigurationBusy()
            ? "Espera a que termine el cambio de red actual."
            : (mPlug.TakeRecording()
                ? "Detén y guarda la toma antes de cambiar el adaptador TX."
                : "No se detectaron adaptadores TX seleccionables.");
'''
new_tx_msg = '''        mMessage = mPlug.NetworkConfigurationBusy()
            ? "Espera a que termine el cambio de red actual."
            : (mPlug.TakeRecording()
                ? "Detén y guarda la toma antes de cambiar el adaptador TX."
                : ((mPlug.TakeOutputArmed() || mPlug.OutputArmed())
                    ? "Desarma la salida antes de cambiar el adaptador TX."
                    : "No se detectaron adaptadores TX seleccionables."));
'''
if main.count(old_tx_msg) != 2:
    raise SystemExit(f'Expected two TX failure blocks, found {main.count(old_tx_msg)}')
main = main.replace(old_tx_msg, new_tx_msg)

show_path.write_text(show, encoding='utf-8')
main_path.write_text(main, encoding='utf-8')
