from pathlib import Path

path = Path('product/AeylaVisualDmx/AeylaMainControl.h')
text = path.read_text(encoding='utf-8')


def replace_exact(old: str, new: str, expected: int = 1) -> None:
    global text
    actual = text.count(old)
    if actual != expected:
        raise SystemExit(f'expected {expected}, found {actual}: {old!r}')
    text = text.replace(old, new)

replace_exact(
'''    if(Contains(mLocalNetworkField, x, y))
    {
      BeginTextEdit(EditKind::local_network, mLocalNetworkField,
                    mLocalNetworkText);
      return;
    }
''',
'''    if(Contains(mLocalNetworkField, x, y))
    {
      const auto output = mPlug.ArtNetOutputStatus();
      if(mPlug.NetworkConfigurationBusy())
        mMessage = "Espera a que termine el cambio de red actual.";
      else if(mPlug.TakeRecording())
        mMessage = "Detén y guarda la toma antes de editar la red TX.";
      else if(mPlug.TakeOutputArmed() || mPlug.OutputArmed() ||
              output.enabled || output.override_enabled)
        mMessage = "Desarma la salida antes de editar la red TX.";
      else
      {
        BeginTextEdit(EditKind::local_network, mLocalNetworkField,
                      mLocalNetworkText);
        return;
      }
      SetDirty(false);
      return;
    }
''')

replace_exact(
'''    else if(mEditKind == EditKind::local_network)
    {
      mLocalNetworkText = value;
      mMessage = "Red editada · presiona APLICAR IP Y PREPARAR ART-NET.";
    }
''',
'''    else if(mEditKind == EditKind::local_network)
    {
      const auto output = mPlug.ArtNetOutputStatus();
      if(mPlug.NetworkConfigurationBusy() || mPlug.TakeRecording() ||
         mPlug.TakeOutputArmed() || mPlug.OutputArmed() ||
         output.enabled || output.override_enabled)
      {
        RestoreNetworkFieldFromSelectedTx();
        mMessage = "Edición descartada · desarma la salida y espera que la red esté libre.";
      }
      else
      {
        mLocalNetworkText = value;
        mMessage = "Red editada · aún no aplicada · presiona APLICAR IP Y PREPARAR ART-NET.";
      }
    }
''')

replace_exact(
'''    Field(g, mLocalNetworkField, "IPv4 TX / MÁSCARA DE SUBRED",
          mLocalNetworkText.empty() ? "clic para configurar" : mLocalNetworkText,
          mLocalNetworkText.empty() ? kWarn : kText);
    const bool networkApplyBlocked =
        networkBusy || mPlug.TakeRecording() ||
        mPlug.TakeOutputArmed() || mPlug.OutputArmed() || physicalAuthority;
''',
'''    Field(g, mLocalNetworkField,
          routeSelectionBlocked ? "IPv4 TX / MÁSCARA · BLOQUEADA"
                                : "IPv4 TX / MÁSCARA DE SUBRED",
          mLocalNetworkText.empty()
              ? (routeSelectionBlocked ? "DESARMA PARA EDITAR" : "clic para configurar")
              : mLocalNetworkText,
          routeSelectionBlocked ? kWarn :
              (mLocalNetworkText.empty() ? kWarn : kText));
    const bool networkApplyBlocked = routeSelectionBlocked;
''')

path.write_text(text, encoding='utf-8')
print('R10.6 network field lock patch applied')
