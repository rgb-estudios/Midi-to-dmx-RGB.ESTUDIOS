from pathlib import Path
import re

PATH = Path("product/AeylaVisualDmx/AeylaRuntimeStatusControl.h")


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"{label}: expected one match, found {count}")
    return text.replace(old, new)


def replace_regex(text: str, pattern: str, replacement: str, label: str) -> str:
    updated, count = re.subn(pattern, replacement, text, count=1, flags=re.S)
    if count != 1:
        raise SystemExit(f"{label}: expected one regex match, found {count}")
    return updated


text = PATH.read_text(encoding="utf-8")
text = replace_once(text,
                    '#include <string>\n#include <utility>',
                    '#include <string>\n#include <string_view>\n#include <utility>',
                    'include string_view')
text = text.replace('R10.6 PRETEST', 'R10.7 PRETEST')

text = replace_once(text,
'''  void OnMouseUp(float x, float y, const IMouseMod& mod) override\n  {\n    (void)x;\n    (void)y;\n    (void)mod;\n    mDraggingMemory = -1;\n  }''',
'''  void OnMouseUp(float x, float y, const IMouseMod& mod) override\n  {\n    (void)x;\n    (void)y;\n    (void)mod;\n    mDraggingMemory = -1;\n  }\n\n  void OnTextEntryCompletion(const char* str, int valIdx) override\n  {\n    (void)valIdx;\n    if(mLiveNameEditIndex < 0) return;\n    const std::size_t index = static_cast<std::size_t>(mLiveNameEditIndex);\n    mLiveNameEditIndex = -1;\n    const std::string value = str == nullptr\n        ? std::string{}\n        : TrimText(str);\n    ReportLive(mPlug.RenameLiveMemoryFromUI(index, value));\n    SetDirty(false);\n  }''',
                    'text completion')

text = replace_once(text,
'''  static bool Empty(const WDL_String& value) noexcept\n  {\n    const char* text = value.Get();\n    return text == nullptr || text[0] == '\\0';\n  }''',
'''  static bool Empty(const WDL_String& value) noexcept\n  {\n    const char* text = value.Get();\n    return text == nullptr || text[0] == '\\0';\n  }\n\n  static std::string TrimText(std::string_view value)\n  {\n    while(!value.empty() &&\n          (value.front() == ' ' || value.front() == '\\t'))\n      value.remove_prefix(1U);\n    while(!value.empty() &&\n          (value.back() == ' ' || value.back() == '\\t'))\n      value.remove_suffix(1U);\n    return std::string(value);\n  }\n\n  void BeginLiveNameEdit(std::size_t index)\n  {\n    if(index >= mPlug.LiveMemoryCount() || index >= mLiveNameButtons.size())\n      return;\n    auto* ui = GetUI();\n    if(ui == nullptr) return;\n    const auto view = mPlug.LiveMemoryViewFromUI(index);\n    mLiveNameEditIndex = static_cast<int>(index);\n    ui->CreateTextEntry(*this,\n                        IText(11.0F, kText, "AeylaUI", EAlign::Near,\n                              EVAlign::Middle),\n                        mLiveNameButtons[index], view.name.c_str(), kNoValIdx);\n  }''',
                    'trim and name editor')

new_layout = r'''  void BuildLiveLayout()
  {
    const float left = mRECT.L + 16.0F;
    const float right = mRECT.R - 16.0F;
    const float top = Header().B + 8.0F;

    const float transportTop = top;
    const float transportGap = 8.0F;
    const float transportW = std::clamp(
        ((right - left) - transportGap * 3.0F) / 4.0F, 118.0F, 176.0F);
    const float transportTotal = transportW * 4.0F + transportGap * 3.0F;
    const float transportLeft = left + std::max(0.0F,
        ((right - left) - transportTotal) * 0.5F);
    for(std::size_t index = 0U; index < mLiveTransport.size(); ++index)
    {
      const float x = transportLeft +
          static_cast<float>(index) * (transportW + transportGap);
      mLiveTransport[index] = IRECT(x, transportTop, x + transportW,
                                   transportTop + 40.0F);
    }

    const float contentTop = transportTop + 52.0F;
    const float contentBottom = Footer().T - 38.0F;
    const float split = left + (right - left) * 0.31F;
    mLiveSetlistPanel = IRECT(left, contentTop, split - 9.0F, contentBottom);
    mLiveMemoryPanel = IRECT(split + 9.0F, contentTop, right, contentBottom);
    mLiveMessageRect = IRECT(left, contentBottom + 6.0F,
                             right, Footer().T - 5.0F);

    const std::size_t songCount = std::min<std::size_t>(
        mPlug.SongCount(), mLiveSongRows.size());
    const bool compactLiveSetlist = mLiveSetlistPanel.H() < 470.0F;
    const float rowTop = mLiveSetlistPanel.T +
        (compactLiveSetlist ? 64.0F : 104.0F);
    const float available = std::max(1.0F, mLiveSetlistPanel.B - rowTop - 8.0F);
    const float rowH = std::clamp(
        available / std::max<std::size_t>(songCount, 1U), 22.0F, 31.0F);
    for(std::size_t index = 0U; index < mLiveSongRows.size(); ++index)
    {
      if(index >= songCount) {
        mLiveSongRows[index] = {};
        continue;
      }
      const float y = rowTop + static_cast<float>(index) * rowH;
      mLiveSongRows[index] = IRECT(mLiveSetlistPanel.L + 8.0F, y,
                                   mLiveSetlistPanel.R - 8.0F,
                                   y + rowH - 2.0F);
    }

    const std::size_t memoryCount = mPlug.LiveMemoryCount();
    if(memoryCount <= 4U) mLiveMemoryPage = 0U;
    else mLiveMemoryPage = std::min<std::size_t>(mLiveMemoryPage, 1U);
    const std::size_t firstMemory = mLiveMemoryPage * 4U;

    const float headerY = mLiveMemoryPanel.T + 7.0F;
    const float headerH = 25.0F;
    const float addW = 92.0F;
    const float pageW = 46.0F;
    mLiveAddMemoryButton = IRECT(mLiveMemoryPanel.R - 9.0F - addW,
                                 headerY,
                                 mLiveMemoryPanel.R - 9.0F,
                                 headerY + headerH);
    mLivePageButtons[1] = IRECT(mLiveAddMemoryButton.L - 7.0F - pageW,
                                headerY,
                                mLiveAddMemoryButton.L - 7.0F,
                                headerY + headerH);
    mLivePageButtons[0] = IRECT(mLivePageButtons[1].L - 5.0F - pageW,
                                headerY,
                                mLivePageButtons[1].L - 5.0F,
                                headerY + headerH);

    mLiveMemoryCards.fill({});
    mLiveConfigButtons.fill({});
    mLiveMainControls.fill({});
    mLiveFaders.fill({});
    mLiveNameButtons.fill({});
    mLiveDmxButtons.fill({});
    mLiveMidiButtons.fill({});
    mLiveModeButtons.fill({});
    mLiveFadeButtons.fill({});
    mLiveBackButtons.fill({});

    const float gridL = mLiveMemoryPanel.L + 8.0F;
    const float gridR = mLiveMemoryPanel.R - 8.0F;
    const float gridT = mLiveMemoryPanel.T + 42.0F;
    const float gridB = mLiveMemoryPanel.B - 8.0F;
    const float gap = 9.0F;
    const float cardW = (gridR - gridL - gap) * 0.5F;
    const float cardH = (gridB - gridT - gap) * 0.5F;

    for(std::size_t local = 0U; local < 4U; ++local)
    {
      const std::size_t index = firstMemory + local;
      if(index >= memoryCount || index >= mLiveMemoryCards.size()) continue;
      const std::size_t col = local % 2U;
      const std::size_t row = local / 2U;
      const float x = gridL + static_cast<float>(col) * (cardW + gap);
      const float y = gridT + static_cast<float>(row) * (cardH + gap);
      const IRECT card(x, y, x + cardW, y + cardH);
      mLiveMemoryCards[index] = card;
      mLiveConfigButtons[index] = IRECT(card.R - 82.0F, card.T + 8.0F,
                                        card.R - 8.0F, card.T + 30.0F);
      mLiveMainControls[index] = IRECT(card.L + 10.0F, card.T + 56.0F,
                                       card.R - 10.0F, card.B - 10.0F);
      const auto control = mLiveMainControls[index];
      const float trackY = control.T + control.H() * 0.50F;
      mLiveFaders[index] = IRECT(control.L + 18.0F, trackY - 6.0F,
                                 control.R - 18.0F, trackY + 6.0F);

      mLiveNameButtons[index] = IRECT(card.L + 10.0F, card.T + 34.0F,
                                      card.R - 10.0F, card.T + 56.0F);
      const float cfgTop = card.T + 61.0F;
      const float cfgGap = 6.0F;
      const float cfgW = (card.W() - 26.0F) * 0.5F;
      const float cfgH = 25.0F;
      mLiveDmxButtons[index] = IRECT(card.L + 10.0F, cfgTop,
                                     card.L + 10.0F + cfgW, cfgTop + cfgH);
      mLiveMidiButtons[index] = IRECT(mLiveDmxButtons[index].R + cfgGap,
                                      cfgTop, card.R - 10.0F, cfgTop + cfgH);
      const float secondTop = cfgTop + cfgH + cfgGap;
      mLiveModeButtons[index] = IRECT(card.L + 10.0F, secondTop,
                                      card.L + 10.0F + cfgW, secondTop + cfgH);
      mLiveFadeButtons[index] = IRECT(mLiveModeButtons[index].R + cfgGap,
                                      secondTop, card.R - 10.0F,
                                      secondTop + cfgH);
      mLiveBackButtons[index] = IRECT(card.L + 10.0F, card.B - 29.0F,
                                      card.R - 10.0F, card.B - 8.0F);
    }
  }'''
text = replace_regex(text,
                     r'  void BuildLiveLayout\(\)\n  \{.*?\n  \}\n\n  void DrawLive\(IGraphics& g\)',
                     new_layout + '\n\n  void DrawLive(IGraphics& g)',
                     'BuildLiveLayout')

new_memories = r'''  void DrawLiveMemories(IGraphics& g)
  {
    g.FillRoundRect(kPanel, mLiveMemoryPanel, 7.0F);
    g.DrawRoundRect(IColor(145, kLine.R, kLine.G, kLine.B),
                    mLiveMemoryPanel, 7.0F, nullptr, 1.0F);

    const std::size_t memoryCount = mPlug.LiveMemoryCount();
    const std::size_t first = mLiveMemoryPage * 4U;
    const std::size_t last = std::min<std::size_t>(first + 4U, memoryCount);
    const std::string heading = "MEMORIAS EN VIVO · " +
        std::to_string(memoryCount) + "/8";
    g.DrawText(IText(11.5F, kText, "AeylaUI",
                     EAlign::Near, EVAlign::Middle),
               heading.c_str(),
               IRECT(mLiveMemoryPanel.L + 11.0F, mLiveMemoryPanel.T + 5.0F,
                     mLivePageButtons[0].L - 8.0F, mLiveMemoryPanel.T + 31.0F));

    Button(g, mLivePageButtons[0], "1–4",
           mLiveMemoryPage == 0U ? IColor(255, 31, 23, 39) : kRaised,
           mLiveMemoryPage == 0U ? kBrand : kLine,
           mLiveMemoryPage == 0U ? kBrand : kMuted, 8.2F);
    Button(g, mLivePageButtons[1], "5–8",
           mLiveMemoryPage == 1U ? IColor(255, 31, 23, 39) : kRaised,
           mLiveMemoryPage == 1U ? kBrand : kLine,
           memoryCount > 4U ? (mLiveMemoryPage == 1U ? kBrand : kMuted) : kFaint,
           8.2F);
    if(memoryCount < 8U)
      Button(g, mLiveAddMemoryButton, "+ MEMORIA", kRaised, kLine, kText, 8.2F);
    else
      Button(g, mLiveAddMemoryButton, "8 / 8", IColor(255, 14, 16, 20),
             IColor(120, kLine.R, kLine.G, kLine.B), kFaint, 8.2F);

    for(std::size_t index = first; index < last; ++index)
      DrawMemoryCard(g, index);

    if(last == first)
      g.DrawText(IText(10.0F, kMuted, "AeylaUI",
                       EAlign::Center, EVAlign::Middle),
                 "AÑADE UNA MEMORIA PARA ESTA PÁGINA",
                 IRECT(mLiveMemoryPanel.L + 20.0F, mLiveMemoryPanel.T + 58.0F,
                       mLiveMemoryPanel.R - 20.0F, mLiveMemoryPanel.B - 20.0F));
  }

  void DrawMemoryCard(IGraphics& g, std::size_t index)
  {
    const auto view = mPlug.LiveMemoryViewFromUI(index);
    const bool configuring = mLiveConfigIndex == static_cast<int>(index);
    const bool active = view.level > 0.005F || view.target_level > 0.005F;
    const auto& card = mLiveMemoryCards[index];

    const IColor accent = configuring ? kBrand :
        (view.learning || view.midi_learning ? kBrand :
            (active ? kGood : (view.configured ? kLine : kWarn)));
    const IColor fill = configuring
        ? IColor(255, 20, 17, 25)
        : (active ? IColor(255, 9, 27, 19) : IColor(255, 15, 17, 21));
    g.FillRoundRect(fill, card, 7.0F);
    g.DrawRoundRect(IColor(configuring || active ? 220 : 115,
                           accent.R, accent.G, accent.B),
                    card, 7.0F, nullptr, configuring || active ? 1.3F : 1.0F);
    g.FillRoundRect(accent,
                    IRECT(card.L + 1.5F, card.T + 11.0F,
                          card.L + 4.0F, card.B - 11.0F), 1.2F);

    g.DrawText(IText(12.4F, active ? kGood : kText, "AeylaUI",
                     EAlign::Near, EVAlign::Middle),
               view.name.c_str(),
               IRECT(card.L + 11.0F, card.T + 4.0F,
                     card.R - 94.0F, card.T + 29.0F));

    Button(g, mLiveConfigButtons[index],
           configuring ? "EDITANDO" : "EDITAR",
           configuring ? IColor(255, 30, 20, 37) : IColor(255, 18, 20, 25),
           configuring ? kBrand : IColor(130, kLine.R, kLine.G, kLine.B),
           configuring ? kBrand : kMuted, 8.0F);

    if(configuring)
    {
      DrawMemoryConfig(g, index, view);
      return;
    }

    const std::string dmx = view.configured
        ? "DMX " + std::to_string(view.channel_count) + " CH"
        : (view.learning ? "DMX · PASO 2/2" : "DMX SIN APRENDER");
    const std::string midi = MidiLabel(view);
    g.DrawText(IText(8.2F, view.configured ? kGood : kWarn, "AeylaUI",
                     EAlign::Near, EVAlign::Middle),
               dmx.c_str(),
               IRECT(card.L + 11.0F, card.T + 28.0F,
                     card.L + card.W() * 0.53F, card.T + 50.0F));
    g.DrawText(IText(8.2F,
                     view.midi_kind == aeyla::live_memory_session::MidiBindingKind::none
                         ? (view.midi_learning ? kBrand : kMuted) : kBrand,
                     "AeylaUI", EAlign::Far, EVAlign::Middle),
               midi.c_str(),
               IRECT(card.L + card.W() * 0.47F, card.T + 28.0F,
                     card.R - 11.0F, card.T + 50.0F));

    if(view.mode == aeyla::output::LiveMemoryControlMode::toggle)
      DrawToggleOperation(g, index, view);
    else
      DrawFaderOperation(g, index, view);
  }'''
text = replace_regex(text,
                     r'  void DrawLiveMemories\(IGraphics& g\)\n  \{.*?\n  \}\n\n  void DrawToggleOperation\(',
                     new_memories + '\n\n  void DrawToggleOperation(',
                     'DrawLiveMemories + card')

# Make operation controls more executor-like and fader more legible.
text = replace_once(text,
'''    g.DrawText(IText(19.0F, stateColor,\n                     "AeylaUI", EAlign::Center, EVAlign::Middle),''',
'''    g.DrawText(IText(21.5F, stateColor,\n                     "AeylaUI", EAlign::Center, EVAlign::Middle),''',
                    'toggle state size')
text = replace_once(text,
'''    g.FillRoundRect(IColor(255, 5, 7, 9), track, 5.0F);\n    if(normalized > 0.0F)\n      g.FillRoundRect(kBrand, IRECT(track.L, track.T, handleX, track.B), 5.0F);\n    g.DrawRoundRect(kLine, track, 5.0F, nullptr, 1.0F);\n    const IRECT handle(handleX - 8.0F, track.T - 12.0F,\n                       handleX + 8.0F, track.B + 12.0F);\n    g.FillRoundRect(kText, handle, 4.0F);\n    g.DrawRoundRect(kBrand, handle, 4.0F, nullptr, 1.1F);''',
'''    g.FillRoundRect(IColor(255, 5, 7, 9), track, 6.0F);\n    if(normalized > 0.0F)\n    {\n      g.FillRoundRect(IColor(70, kBrand.R, kBrand.G, kBrand.B),\n                      IRECT(track.L, track.T - 5.0F, handleX, track.B + 5.0F),\n                      7.0F);\n      g.FillRoundRect(kBrand, IRECT(track.L, track.T, handleX, track.B), 6.0F);\n    }\n    g.DrawRoundRect(IColor(170, kLine.R, kLine.G, kLine.B),\n                    track, 6.0F, nullptr, 1.0F);\n    const IRECT handle(handleX - 9.0F, track.T - 11.0F,\n                       handleX + 9.0F, track.B + 11.0F);\n    g.FillRoundRect(kText, handle, 5.0F);\n    g.DrawRoundRect(kBrand, handle, 5.0F, nullptr, 1.2F);''',
                    'fader styling')

new_config = r'''  void DrawMemoryConfig(
      IGraphics& g, std::size_t index,
      const aeyla::live_memory_session::MemoryView& view)
  {
    const std::string dmxLabel = view.learning
        ? "2/2 CAPTURAR ON"
        : "1/2 CAPTURAR OFF";
    const std::string midiLabel = view.midi_learning
        ? (view.mode == aeyla::output::LiveMemoryControlMode::toggle
               ? "ESPERANDO NOTE…" : "MUEVE CC…")
        : (view.mode == aeyla::output::LiveMemoryControlMode::toggle
               ? "APRENDER NOTE" : "APRENDER CC");
    const std::string modeLabel =
        view.mode == aeyla::output::LiveMemoryControlMode::toggle
            ? "MODO · BOTÓN" : "MODO · FADER";
    const std::string fadeLabel = view.fade_ms == 100U ? "FADE · 0.1 s"
        : (view.fade_ms == 1500U ? "FADE · 1.5 s" : "FADE · 1.0 s");

    Button(g, mLiveNameButtons[index], "NOMBRE · " + view.name,
           IColor(255, 12, 14, 19), kBrand, kText, 8.5F);
    Button(g, mLiveDmxButtons[index], dmxLabel,
           view.learning ? IColor(255, 30, 20, 37) : kRaised,
           view.learning ? kBrand : kWarn,
           view.learning ? kBrand : kWarn, 8.0F);
    Button(g, mLiveMidiButtons[index], midiLabel,
           view.midi_learning ? IColor(255, 30, 20, 37) : kRaised,
           view.midi_learning ? kBrand : kLine,
           view.midi_learning ? kBrand : kText, 8.0F);
    Button(g, mLiveModeButtons[index], modeLabel, kRaised, kLine,
           view.mode == aeyla::output::LiveMemoryControlMode::fader
               ? kBrand : kText, 8.0F);
    Button(g, mLiveFadeButtons[index], fadeLabel, kRaised, kLine,
           kText, 8.0F);

    const auto& card = mLiveMemoryCards[index];
    std::string instruction;
    IColor instructionColor = kMuted;
    if(view.learning) {
      instruction = "Avolites: enciende SÓLO esta memoria y captura ON.";
      instructionColor = kBrand;
    }
    else if(view.midi_learning) {
      instruction = view.mode == aeyla::output::LiveMemoryControlMode::toggle
          ? "MIDI: presiona la Note. El primer toque sólo asigna."
          : "MIDI: mueve el CC. El primer movimiento sólo asigna.";
      instructionColor = kBrand;
    }
    else {
      instruction = "NOMBRE / DMX / MIDI / MODO / FADE";
    }
    g.DrawText(IText(7.8F, instructionColor, "AeylaUI",
                     EAlign::Near, EVAlign::Middle),
               instruction.c_str(),
               IRECT(card.L + 11.0F, mLiveFadeButtons[index].B + 3.0F,
                     card.R - 11.0F, mLiveBackButtons[index].T - 2.0F));

    Button(g, mLiveBackButtons[index], "CERRAR EDICIÓN",
           IColor(255, 24, 17, 31), kBrand, kBrand, 8.2F);
  }'''
text = replace_regex(text,
                     r'  void DrawMemoryConfig\(\n      IGraphics& g, std::size_t index,\n      const aeyla::live_memory_session::MemoryView& view\)\n  \{.*?\n  \}\n\n  static std::string MidiLabel',
                     new_config + '\n\n  static std::string MidiLabel',
                     'DrawMemoryConfig')

# Page controls and add-memory interaction before transport.
text = replace_once(text,
'''  void HandleLiveMouseDown(float x, float y)\n  {\n    BuildLiveLayout();\n\n    if(Contains(mLiveTransport[0], x, y))''',
'''  void HandleLiveMouseDown(float x, float y)\n  {\n    BuildLiveLayout();\n\n    if(Contains(mLivePageButtons[0], x, y))\n    {\n      mLiveMemoryPage = 0U;\n      mLiveConfigIndex = -1;\n      mDraggingMemory = -1;\n      SetDirty(false);\n      return;\n    }\n    if(Contains(mLivePageButtons[1], x, y))\n    {\n      if(mPlug.LiveMemoryCount() > 4U)\n      {\n        mLiveMemoryPage = 1U;\n        mLiveConfigIndex = -1;\n        mDraggingMemory = -1;\n      }\n      SetDirty(false);\n      return;\n    }\n    if(Contains(mLiveAddMemoryButton, x, y) && mPlug.LiveMemoryCount() < 8U)\n    {\n      const auto result = mPlug.AddLiveMemoryFromUI();\n      ReportLive(result);\n      if(result.succeeded)\n      {\n        const std::size_t count = mPlug.LiveMemoryCount();\n        mLiveMemoryPage = (count - 1U) / 4U;\n        mLiveConfigIndex = static_cast<int>(count - 1U);\n        mDraggingMemory = -1;\n      }\n      SetDirty(false);\n      return;\n    }\n\n    if(Contains(mLiveTransport[0], x, y))''',
                    'page/add handlers')

text = replace_once(text,
'''    for(std::size_t index = 0U; index < mLiveMemoryCards.size(); ++index)\n    {\n      const auto view = mPlug.LiveMemoryViewFromUI(index);''',
'''    const std::size_t memoryCount = mPlug.LiveMemoryCount();\n    const std::size_t firstMemory = mLiveMemoryPage * 4U;\n    const std::size_t lastMemory = std::min<std::size_t>(\n        firstMemory + 4U, memoryCount);\n    for(std::size_t index = firstMemory; index < lastMemory; ++index)\n    {\n      const auto view = mPlug.LiveMemoryViewFromUI(index);''',
                    'paged memory click loop')

text = replace_once(text,
'''      if(mLiveConfigIndex == static_cast<int>(index))\n      {\n        if(Contains(mLiveDmxButtons[index], x, y))''',
'''      if(mLiveConfigIndex == static_cast<int>(index))\n      {\n        if(Contains(mLiveNameButtons[index], x, y))\n        {\n          BeginLiveNameEdit(index);\n          return;\n        }\n        if(Contains(mLiveDmxButtons[index], x, y))''',
                    'name edit click')

text = replace_once(text,
'''    mLiveMessage = "EN VIVO · selecciona una memoria para operar; EDITAR abre DMX/MIDI/modo/fade sólo en esa memoria.";''',
'''    mLiveMessage = "EN VIVO · 4 memorias por página; EDITAR abre nombre / DMX / MIDI / modo / fade.";''',
                    'open live message')

text = replace_once(text,
'''  int mLiveConfigIndex{-1};\n  int mDraggingMemory{-1};''',
'''  int mLiveConfigIndex{-1};\n  int mDraggingMemory{-1};\n  int mLiveNameEditIndex{-1};\n  std::size_t mLiveMemoryPage{0U};''',
                    'live state members')

text = replace_once(text,
'''  std::array<IRECT, 4U> mLiveMemoryCards{};\n  std::array<IRECT, 4U> mLiveConfigButtons{};\n  std::array<IRECT, 4U> mLiveMainControls{};\n  std::array<IRECT, 4U> mLiveFaders{};\n  std::array<IRECT, 4U> mLiveDmxButtons{};\n  std::array<IRECT, 4U> mLiveMidiButtons{};\n  std::array<IRECT, 4U> mLiveModeButtons{};\n  std::array<IRECT, 4U> mLiveFadeButtons{};\n  std::array<IRECT, 4U> mLiveBackButtons{};''',
'''  std::array<IRECT, 2U> mLivePageButtons{};\n  IRECT mLiveAddMemoryButton{};\n  std::array<IRECT, 8U> mLiveMemoryCards{};\n  std::array<IRECT, 8U> mLiveConfigButtons{};\n  std::array<IRECT, 8U> mLiveMainControls{};\n  std::array<IRECT, 8U> mLiveFaders{};\n  std::array<IRECT, 8U> mLiveNameButtons{};\n  std::array<IRECT, 8U> mLiveDmxButtons{};\n  std::array<IRECT, 8U> mLiveMidiButtons{};\n  std::array<IRECT, 8U> mLiveModeButtons{};\n  std::array<IRECT, 8U> mLiveFadeButtons{};\n  std::array<IRECT, 8U> mLiveBackButtons{};''',
                    'memory rect arrays')

# Reset page/name editing when changing workspace so hidden edits never survive navigation.
text = replace_once(text,
'''      mLiveConfigIndex = -1;\n      mDraggingMemory = -1;\n      if(mLiveOpen)''',
'''      mLiveConfigIndex = -1;\n      mDraggingMemory = -1;\n      mLiveNameEditIndex = -1;\n      if(mLiveOpen)''',
                    'workspace reset name edit')

PATH.write_text(text, encoding="utf-8")
print("R10.7 paged/renamable EN VIVO UI patch applied")
