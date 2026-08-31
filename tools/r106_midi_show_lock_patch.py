from pathlib import Path


def patch(path: str, replacements):
    p = Path(path)
    text = p.read_text(encoding='utf-8')
    for old, new, expected in replacements:
        actual = text.count(old)
        if actual != expected:
            raise SystemExit(f'{path}: expected {expected}, found {actual}: {old!r}')
        text = text.replace(old, new)
    p.write_text(text, encoding='utf-8')

# Expose one canonical non-RT predicate so backend and UI use the exact same
# definition of "configuration is frozen because physical authority exists".
patch('product/AeylaVisualDmx/AeylaVisualDmx.h', [
    (
'''  [[nodiscard]] std::string ShowMidiStatus() const;
  [[nodiscard]] bool ShowMidiPreflightBusy() const noexcept
''',
'''  [[nodiscard]] std::string ShowMidiStatus() const;
  [[nodiscard]] bool ShowMidiConfigurationLocked() const noexcept
  {
    const auto output = mArtNetOutput.stats();
    return TakeOutputArmed() || OutputArmed() ||
           output.enabled || output.override_enabled;
  }
  [[nodiscard]] bool ShowMidiPreflightBusy() const noexcept
''', 1)
])

# Freeze all mapping/mode mutations while physical output is authoritative.
patch('product/AeylaVisualDmx/AeylaMidiShowIntegration.cpp', [
    (
'''aeyla::product::AuthoringResult AeylaVisualDmx::ToggleShowMidiFromUI()
{
  auto mapping = ShowMidiMapping();
''',
'''aeyla::product::AuthoringResult AeylaVisualDmx::ToggleShowMidiFromUI()
{
  if(ShowMidiConfigurationLocked())
    return {false, {},
            "Desarma la salida antes de cambiar el modo MIDI SHOW"};
  auto mapping = ShowMidiMapping();
''', 1),
    (
'''  if(!mapping.enabled && (TakeOutputArmed() || OutputArmed()))
    return {false, {},
            "Desarma cualquier autoridad antes de activar y precargar MIDI SHOW"};
  mapping.enabled = !mapping.enabled;
''',
'''  mapping.enabled = !mapping.enabled;
''', 1),
    (
'''  if(TakeOutputArmed() || OutputArmed())
    return {false, {},
            "Desarma la salida antes de cambiar el mapa MIDI del show"};
''',
'''  if(ShowMidiConfigurationLocked())
    return {false, {},
            "Desarma la salida antes de cambiar el mapa MIDI del show"};
''', 1),
    (
'''  if(TakeOutputArmed() || OutputArmed())
    return {false, {},
            "Desarma la salida antes de aprender notas MIDI del show"};
''',
'''  if(ShowMidiConfigurationLocked())
    return {false, {},
            "Desarma la salida antes de aprender notas MIDI del show"};
''', 1),
])

# Arming either authority cancels both a waiting Learn target and a note that
# reached the audio callback but has not yet been consolidated on the UI thread.
patch('product/AeylaVisualDmx/AeylaShowPlayerIntegration.cpp', [
    (
'''  if(NetworkConfigurationBusy())
    return {false, {}, "Espera a que termine el cambio de red antes de armar"};
''',
'''  mShowMidiLearnTarget.store(aeyla::runtime::ShowMidiLearnTarget::none,
                             std::memory_order_release);
  mPendingMidiLearnPacked.store(0U, std::memory_order_release);
  if(NetworkConfigurationBusy())
    return {false, {}, "Espera a que termine el cambio de red antes de armar"};
''', 1),
])

patch('product/AeylaVisualDmx/AeylaVisualDmx.cpp', [
    (
'''void AeylaVisualDmx::SetOutputArmed(bool armed)
{
  const std::scoped_lock lock(mModelMutex);
''',
'''void AeylaVisualDmx::SetOutputArmed(bool armed)
{
  if(armed)
  {
    mShowMidiLearnTarget.store(aeyla::runtime::ShowMidiLearnTarget::none,
                               std::memory_order_release);
    mPendingMidiLearnPacked.store(0U, std::memory_order_release);
  }
  const std::scoped_lock lock(mModelMutex);
''', 1),
])

# Conservative UI cleanup: same geometry, less text, truthful lock state.
patch('product/AeylaVisualDmx/AeylaMainControl.h', [
    ('               "AUTOMATIZACIÓN MIDI DE SHOW",\n',
     '               "MIDI SHOW · MAPEO Y CONTROL",\n', 1),
    (
'''    const auto mapping = mPlug.ShowMidiMapping();
    g.DrawText(IText(18.0F, kText, "AeylaUI", EAlign::Near, EVAlign::Middle),
''',
'''    const auto mapping = mPlug.ShowMidiMapping();
    const bool midiConfigLocked = mPlug.ShowMidiConfigurationLocked();
    g.DrawText(IText(18.0F, kText, "AeylaUI", EAlign::Near, EVAlign::Middle),
''', 1),
    (
'''    Button(g, mMidiEnableButton,
           mapping.enabled ? "MIDI SHOW ACTIVO" : "ACTIVAR MIDI SHOW",
           mapping.enabled ? IColor(255, 18, 51, 38) : kPanelRaised,
           mapping.enabled ? kGood : kLineStrong,
           mapping.enabled ? kGood : kText);
    Button(g, mMidiChannelPrevious, "<", kPanelRaised, kLineStrong);
    const std::string channel = "CANAL " + std::to_string(mapping.channel);
    Button(g, mMidiChannelField, channel.c_str(), kPanelRaised, kLineStrong,
           kText);
    Button(g, mMidiChannelNext, ">", kPanelRaised, kLineStrong);
''',
'''    const std::string midiModeLabel = midiConfigLocked
        ? (mapping.enabled ? "MIDI SHOW ACTIVO · BLOQUEADO"
                           : "MIDI SHOW OFF · BLOQUEADO")
        : (mapping.enabled ? "MIDI SHOW ACTIVO" : "ACTIVAR MIDI SHOW");
    Button(g, mMidiEnableButton, midiModeLabel.c_str(),
           midiConfigLocked ? IColor(255, 54, 42, 22) :
               (mapping.enabled ? IColor(255, 18, 51, 38) : kPanelRaised),
           midiConfigLocked ? kWarn :
               (mapping.enabled ? kGood : kLineStrong),
           midiConfigLocked ? kWarn :
               (mapping.enabled ? kGood : kText));
    Button(g, mMidiChannelPrevious, "<",
           midiConfigLocked ? IColor(255, 35, 31, 25) : kPanelRaised,
           midiConfigLocked ? kWarn : kLineStrong,
           midiConfigLocked ? kWarn : kText);
    const std::string channel = "CANAL " + std::to_string(mapping.channel) +
        (midiConfigLocked ? " · BLOQUEADO" : "");
    Button(g, mMidiChannelField, channel.c_str(), kPanelRaised,
           midiConfigLocked ? kWarn : kLineStrong,
           midiConfigLocked ? kWarn : kText);
    Button(g, mMidiChannelNext, ">",
           midiConfigLocked ? IColor(255, 35, 31, 25) : kPanelRaised,
           midiConfigLocked ? kWarn : kLineStrong,
           midiConfigLocked ? kWarn : kText);
''', 1),
    ('        "CANCIÓN ANTERIOR",\n', '        "ANTERIOR",\n', 1),
    ('        "SIGUIENTE CANCIÓN",\n', '        "SIGUIENTE",\n', 1),
    ('        "PLAY / REINICIAR DESDE CERO",\n', '        "PLAY / REINICIAR",\n', 1),
    ('        "STOP / RESET A CERO",\n', '        "STOP / CERO",\n', 1),
    ('        "REC START · INICIO CAPTURA",\n', '        "REC START",\n', 1),
    ('        "REC STOP · FIN CAPTURA",\n', '        "REC STOP",\n', 1),
    ('        "LANZAR CANCIONES 01–15"};\n', '        "LANZAR CANCIÓN 01–15"};\n', 1),
    (
'''      Button(g, mMidiLearnButtons[index],
             waiting ? "ESPERANDO NOTA…" : "APRENDER MIDI",
             waiting ? IColor(255, 54, 42, 22) : kPanelRaised,
             waiting ? kWarn : kLineStrong,
             waiting ? kWarn : kText);
''',
'''      Button(g, mMidiLearnButtons[index],
             midiConfigLocked ? "BLOQUEADO" :
                 (waiting ? "ESPERANDO NOTA…" : "APRENDER MIDI"),
             (midiConfigLocked || waiting) ? IColor(255, 54, 42, 22) :
                                             kPanelRaised,
             (midiConfigLocked || waiting) ? kWarn : kLineStrong,
             (midiConfigLocked || waiting) ? kWarn : kText);
''', 1),
    ('                   ? "SINCRONÍA: MUESTRAS DEL DAW · sin deriva acumulativa"\n'
     '                   : "SINCRONÍA: MUESTRAS DEL DAW · sin reloj global ni deriva acumulativa",\n',
     '                   ? "RELOJ · MUESTRAS DEL DAW · SIN DERIVA ACUMULATIVA"\n'
     '                   : "RELOJ · MUESTRAS DEL DAW · SIN DERIVA ACUMULATIVA",\n', 1),
    (
'''                 ("CAPTURA: N" + std::to_string(mapping.capture_start_note) +
                  " REC START · N" + std::to_string(mapping.capture_stop_note) +
                  " REC STOP · CERO = REC START").c_str(),
''',
'''                 ("CAPTURA · N" + std::to_string(mapping.capture_start_note) +
                  " REC START / CERO · N" + std::to_string(mapping.capture_stop_note) +
                  " REC STOP").c_str(),
''', 1),
    (
'''                 ("CAPTURA DMX: N" + std::to_string(mapping.capture_start_note) +
                  " REC START fija CERO · N" +
                  std::to_string(mapping.capture_stop_note) +
                  " REC STOP finaliza · el plugin no usa MTC.").c_str(),
''',
'''                 ("CAPTURA · N" + std::to_string(mapping.capture_start_note) +
                  " REC START / CERO · N" +
                  std::to_string(mapping.capture_stop_note) +
                  " REC STOP").c_str(),
''', 1),
    (
'''                 "MIDI nunca arma Art-Net ni desactiva APAGÓN. Prepara la salida manualmente antes del show.",
''',
'''                 "SEGURIDAD · MIDI NO ARMA ART-NET NI LIBERA APAGÓN",
''', 1),
])

print('R10.6 MIDI SHOW live-lock patch applied')
