from pathlib import Path

path = Path('product/AeylaVisualDmx/AeylaRuntimeStatusControl.h')
text = path.read_text(encoding='utf-8')

replacements = [
    (
        '''    g.DrawText(IText(8.3F, kMuted, "AeylaUI",
                     EAlign::Far, EVAlign::Middle),
               "4 ACCESOS · DMX / MIDI DENTRO DE EDITAR",
''',
        '''    g.DrawText(IText(8.3F, kMuted, "AeylaUI",
                     EAlign::Far, EVAlign::Middle),
               "OPERACIÓN · EDITAR = DMX / MIDI / FADE",
'''
    ),
    (
        '''    const std::string dmx = view.configured
        ? "DMX " + std::to_string(view.channel_count) + " CH"
        : (view.learning ? "DMX PASO 1/2" : "DMX SIN CONFIGURAR");
''',
        '''    const std::string dmx = view.configured
        ? "DMX · " + std::to_string(view.channel_count) + " CH"
        : (view.learning ? "DMX · PASO 2/2" : "DMX · SIN APRENDER");
'''
    ),
    (
        '''  void DrawToggleOperation(
      IGraphics& g, std::size_t index,
      const aeyla::live_memory_session::MemoryView& view)
  {
    const auto& control = mLiveMainControls[index];
    const bool on = view.target_level > 0.5F;
    const bool ready = view.configured;
    g.FillRoundRect(on ? IColor(255, 12, 46, 31) : IColor(255, 10, 11, 15),
                    control, 6.0F);
    g.DrawRoundRect(on ? kGood : (ready ? kLine : kWarn),
                    control, 6.0F, nullptr, on ? 1.6F : 1.0F);
    g.DrawText(IText(19.0F, on ? kGood : (ready ? kText : kWarn),
                     "AeylaUI", EAlign::Center, EVAlign::Middle),
               ready ? (on ? "ON" : "OFF") : "CONFIGURA DMX",
               control.GetPadded(-6.0F));
    g.DrawText(IText(8.2F, kMuted, "AeylaUI",
                     EAlign::Center, EVAlign::Bottom),
               ready ? "BOTÓN / TOGGLE" : "el MIDI puede mapearse antes",
               control.GetPadded(-7.0F));
  }
''',
        '''  void DrawToggleOperation(
      IGraphics& g, std::size_t index,
      const aeyla::live_memory_session::MemoryView& view)
  {
    const auto& control = mLiveMainControls[index];
    const bool ready = view.configured;
    const bool actualOn = view.level > 0.5F;
    const bool targetOn = view.target_level > 0.5F;
    const bool transitioning = ready && view.transitioning;
    const IColor stateColor = !ready ? kWarn :
        (transitioning ? kWarn : (actualOn ? kGood : kText));
    const IColor border = !ready ? kWarn :
        (transitioning ? kWarn : (actualOn ? kGood : kLine));
    const IColor fill = actualOn
        ? IColor(255, 12, 46, 31)
        : (transitioning ? IColor(255, 38, 28, 12)
                         : IColor(255, 10, 11, 15));
    std::string state = "APRENDER DMX";
    if(ready)
      state = transitioning
          ? (targetOn ? "FADE → ON" : "FADE → OFF")
          : (actualOn ? "ON" : "OFF");
    const std::string fade = view.fade_ms == 100U ? "0.1 s"
        : (view.fade_ms == 1500U ? "1.5 s" : "1.0 s");
    const std::string detail = ready
        ? "BOTÓN · FADE " + fade
        : "TOCA PARA CONFIGURAR";

    g.FillRoundRect(fill, control, 6.0F);
    g.DrawRoundRect(border, control, 6.0F, nullptr,
                    actualOn || transitioning ? 1.6F : 1.0F);
    g.DrawText(IText(19.0F, stateColor,
                     "AeylaUI", EAlign::Center, EVAlign::Middle),
               state.c_str(), control.GetPadded(-6.0F));
    g.DrawText(IText(8.2F, kMuted, "AeylaUI",
                     EAlign::Center, EVAlign::Bottom),
               detail.c_str(), control.GetPadded(-7.0F));
  }
'''
    ),
    (
        '''    if(!ready)
    {
      g.DrawText(IText(14.0F, kWarn, "AeylaUI",
                       EAlign::Center, EVAlign::Middle),
                 "CONFIGURA DMX", control);
      g.DrawText(IText(8.2F, kMuted, "AeylaUI",
                       EAlign::Center, EVAlign::Bottom),
                 "el MIDI CC puede mapearse antes", control.GetPadded(-7.0F));
      return;
    }
''',
        '''    if(!ready)
    {
      g.DrawText(IText(14.0F, kWarn, "AeylaUI",
                       EAlign::Center, EVAlign::Middle),
                 "APRENDER DMX", control);
      g.DrawText(IText(8.2F, kMuted, "AeylaUI",
                       EAlign::Center, EVAlign::Bottom),
                 "TOCA PARA CONFIGURAR", control.GetPadded(-7.0F));
      return;
    }
'''
    ),
    (
        '''    else {
      instruction = "DMX: deja esta memoria OFF en Avolites para el paso 1. MIDI puede aprenderse antes o después.";
    }
''',
        '''    else {
      instruction = "DMX: memoria OFF → captura 1/2. Luego ON → captura 2/2.";
    }
'''
    ),
]

for old, new in replacements:
    count = text.count(old)
    if count != 1:
        raise SystemExit(f'Expected exactly one match, found {count}: {old[:100]!r}')
    text = text.replace(old, new, 1)

path.write_text(text, encoding='utf-8')
