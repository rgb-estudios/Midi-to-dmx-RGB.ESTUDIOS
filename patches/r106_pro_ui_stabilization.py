from pathlib import Path


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"{label}: expected exactly 1 match, found {count}")
    return text.replace(old, new, 1)


# -----------------------------------------------------------------------------
# Main workspace: remove legacy shell hit-zones/drawing and align the editor
# palette with the new RGB Live Control visual language.
# -----------------------------------------------------------------------------
main_path = Path("product/AeylaVisualDmx/AeylaMainControl.h")
main = main_path.read_text(encoding="utf-8")

main = replace_once(
    main,
    """    g.FillRect(kBackground, mRECT);\n    DrawHeader(g);\n    DrawSetlist(g);\n""",
    """    g.FillRect(kBackground, mRECT);\n    // R10.6: the canonical shell/header is owned exclusively by\n    // AeylaRuntimeStatusControl. MainControl renders workspace content only.\n    DrawSetlist(g);\n""",
    "main legacy header draw",
)

main = replace_once(
    main,
    """    if(Contains(mTakeEditorTab, x, y))\n    {\n      mPlug.SetUiWorkspaceFromUI(0);\n      SyncWorkspaceFromProduct();\n      mMessage = \"TOMA · captura, edición y reproducción DMX.\";\n      SetDirty(false);\n      return;\n    }\n    if(Contains(mNetworkOutputTab, x, y))\n    {\n      mPlug.SetUiWorkspaceFromUI(3);\n      SyncWorkspaceFromProduct();\n      mMessage = \"SISTEMA · red, salida Art-Net y diagnóstico.\";\n      SetDirty(false);\n      return;\n    }\n    if(Contains(mMidiShowTab, x, y))\n    {\n      mPlug.SetUiWorkspaceFromUI(2);\n      SyncWorkspaceFromProduct();\n      mMessage = \"MIDI · automatización y Learn del show.\";\n      SetDirty(false);\n      return;\n    }\n\n    if(Contains(mBlackoutButton, x, y))\n    {\n      const bool enable = !mPlug.GlobalBlackout();\n      mPlug.SetBlackoutFromUI(enable);\n      mMessage = enable\n          ? \"APAGÓN TOTAL · DMX 0 continuo; ARM se conserva.\"\n          : (mPlug.EffectiveBlackout()\n                 ? \"APAGÓN DESACTIVADO · el negro del show no bloquea la toma; arma manualmente.\"\n                 : \"APAGÓN DESACTIVADO · el armado sigue siendo manual.\");\n      SetDirty(false);\n      return;\n    }\n\n    if(Contains(mTakeArmButton, x, y))\n    {\n      Report(mPlug.ToggleTakeOutputArmFromUI());\n      SetDirty(false);\n      return;\n    }\n\n""",
    """    // R10.6: never leave hidden duplicate controls below the canonical shell.\n    // RuntimeStatusControl owns every header hit-zone (navigation, ARM, APAGÓN).\n    if(Contains(mHeader, x, y))\n      return;\n\n""",
    "main legacy header hit-zones",
)

main = replace_once(
    main,
    """  inline static const IColor kPanelSelected{255, 30, 25, 30};\n  inline static const IColor kLine{255, 43, 48, 59};\n  inline static const IColor kLineStrong{255, 73, 80, 95};\n  inline static const IColor kText{255, 235, 238, 242};\n  inline static const IColor kMuted{255, 135, 143, 157};\n  inline static const IColor kFaint{255, 88, 95, 108};\n  inline static const IColor kAccent{255, 229, 48, 61};\n  inline static const IColor kAccentDark{255, 84, 25, 33};\n  inline static const IColor kGood{255, 70, 205, 137};\n  inline static const IColor kWarn{255, 238, 159, 64};\n""",
    """  inline static const IColor kPanelSelected{255, 27, 22, 34};\n  inline static const IColor kLine{255, 43, 48, 59};\n  inline static const IColor kLineStrong{255, 73, 80, 95};\n  inline static const IColor kText{255, 235, 238, 242};\n  inline static const IColor kMuted{255, 135, 143, 157};\n  inline static const IColor kFaint{255, 88, 95, 108};\n  inline static const IColor kAccent{255, 202, 145, 255};\n  inline static const IColor kAccentDark{255, 53, 35, 67};\n  inline static const IColor kCyan{255, 68, 214, 255};\n  inline static const IColor kDanger{255, 231, 45, 55};\n  inline static const IColor kDangerDark{255, 66, 18, 25};\n  inline static const IColor kGood{255, 70, 205, 137};\n  inline static const IColor kWarn{255, 238, 159, 64};\n""",
    "main palette",
)

main = replace_once(
    main,
    """    g.DrawText(IText(12.0F,\n                     mPlug.TakeRecording() ? kAccent :\n                         (compact && !mMessage.empty() ? kWarn : kMuted),\n""",
    """    g.DrawText(IText(12.0F,\n                     mPlug.TakeRecording() ? kDanger :\n                         (compact && !mMessage.empty() ? kWarn : kMuted),\n""",
    "recording status color",
)

main = replace_once(
    main,
    """          g.FillRect(IColor(220, 229, 48, 61),\n""",
    """          g.FillRect(IColor(220, 68, 214, 255),\n""",
    "timeline motion color",
)

main = replace_once(
    main,
    """    Button(g, mRecordButton,\n           mPlug.TakeRecording() ? \"DETENER + GUARDAR TOMA\" :\n               (recordBlocked ? \"GRABACIÓN BLOQUEADA\" : \"GRABAR NUEVA TOMA\"),\n           mPlug.TakeRecording() ? kAccentDark :\n               (recordBlocked ? IColor(255, 54, 42, 22) : kPanelRaised),\n           mPlug.TakeRecording() ? kAccent :\n               (recordBlocked ? kWarn : kLineStrong),\n           recordBlocked ? kWarn : kText);\n""",
    """    Button(g, mRecordButton,\n           mPlug.TakeRecording() ? \"DETENER + GUARDAR TOMA\" :\n               (recordBlocked ? \"GRABACIÓN BLOQUEADA\" : \"GRABAR NUEVA TOMA\"),\n           mPlug.TakeRecording() ? kDangerDark :\n               (recordBlocked ? IColor(255, 54, 42, 22) : kPanelRaised),\n           mPlug.TakeRecording() ? kDanger :\n               (recordBlocked ? kWarn : kLineStrong),\n           mPlug.TakeRecording() ? kDanger :\n               (recordBlocked ? kWarn : kText));\n""",
    "record button palette",
)

main_path.write_text(main, encoding="utf-8")


# -----------------------------------------------------------------------------
# Canonical shell + EN VIVO: interaction fixes and a stronger show-facing visual
# hierarchy based on the approved concept render.
# -----------------------------------------------------------------------------
runtime_path = Path("product/AeylaVisualDmx/AeylaRuntimeStatusControl.h")
runtime = runtime_path.read_text(encoding="utf-8")

runtime = replace_once(
    runtime,
    """    if(mPlug.UiWorkspace() == 1)\n    {\n      HandleLiveMouseDown(x, y);\n      return;\n    }\n\n    if(Contains(ArchiveButton(), x, y))\n    {\n      mFileMenuOpen = !mFileMenuOpen;\n      SetDirty(false);\n      return;\n    }\n\n    if(mFileMenuOpen)\n    {\n      BuildFileMenuButtons();\n      for(std::size_t index = 0U; index < mFileButtons.size(); ++index)\n      {\n        if(!Contains(mFileButtons[index], x, y)) continue;\n        mFileMenuOpen = false;\n        HandleFileAction(index);\n        return;\n      }\n      if(!Contains(FileMenuPanel(), x, y))\n      {\n        mFileMenuOpen = false;\n        SetDirty(false);\n      }\n    }\n""",
    """    // File operations remain reachable from every workspace, including\n    // EN VIVO. The old ordering let the live surface swallow ARCHIVO clicks.\n    if(Contains(ArchiveButton(), x, y))\n    {\n      mFileMenuOpen = !mFileMenuOpen;\n      SetDirty(false);\n      return;\n    }\n\n    if(mFileMenuOpen)\n    {\n      BuildFileMenuButtons();\n      for(std::size_t index = 0U; index < mFileButtons.size(); ++index)\n      {\n        if(!Contains(mFileButtons[index], x, y)) continue;\n        mFileMenuOpen = false;\n        HandleFileAction(index);\n        return;\n      }\n      if(!Contains(FileMenuPanel(), x, y))\n      {\n        // Safety: dismissing the menu never clicks through into a live control.\n        mFileMenuOpen = false;\n        SetDirty(false);\n        return;\n      }\n    }\n\n    if(mPlug.UiWorkspace() == 1)\n    {\n      HandleLiveMouseDown(x, y);\n      return;\n    }\n""",
    "archive ordering in live workspace",
)

runtime = replace_once(
    runtime,
    "RGB ESTUDIOS · SHOW / AEYLA · R10.5 PRETEST",
    "RGB ESTUDIOS · SHOW / AEYLA · R10.6 PRETEST",
    "runtime version label",
)

runtime = replace_once(
    runtime,
    """    for(std::size_t index = 0U; index < tabs.size(); ++index)\n    {\n      const auto tab = NavTab(index);\n      const bool selected = active == static_cast<int>(index);\n      g.DrawText(IText(10.4F, selected ? kText : kMuted, \"AeylaUI\",\n                       EAlign::Center, EVAlign::Middle), tabs[index], tab);\n      if(selected)\n        g.FillRect(kBrand, IRECT(tab.L + 12.0F, tab.B - 2.0F,\n                                tab.R - 12.0F, tab.B));\n    }\n""",
    """    for(std::size_t index = 0U; index < tabs.size(); ++index)\n    {\n      const auto tab = NavTab(index);\n      const bool selected = active == static_cast<int>(index);\n      if(selected)\n      {\n        g.FillRoundRect(IColor(255, 31, 23, 39), tab, 6.0F);\n        g.DrawRoundRect(kBrand, tab, 6.0F, nullptr, 1.0F);\n      }\n      else\n        g.DrawRoundRect(IColor(150, 47, 51, 62), tab, 6.0F, nullptr, 1.0F);\n      g.DrawText(IText(10.4F, selected ? kText : kMuted, \"AeylaUI\",\n                       EAlign::Center, EVAlign::Middle), tabs[index], tab);\n      if(selected)\n        g.FillRoundRect(kBrand, IRECT(tab.L + 18.0F, tab.B - 2.5F,\n                                     tab.R - 18.0F, tab.B), 1.2F);\n    }\n""",
    "navigation visual hierarchy",
)

runtime = replace_once(
    runtime,
    """    const float rowTop = mLiveSetlistPanel.T + 50.0F;\n    const float available = std::max(1.0F, mLiveSetlistPanel.B - rowTop - 8.0F);\n""",
    """    const bool compactLiveSetlist = mLiveSetlistPanel.H() < 470.0F;\n    const float rowTop = mLiveSetlistPanel.T +\n        (compactLiveSetlist ? 64.0F : 104.0F);\n    const float available = std::max(1.0F, mLiveSetlistPanel.B - rowTop - 8.0F);\n""",
    "live setlist layout",
)

runtime = replace_once(
    runtime,
    """    static constexpr std::array<const char*, 4U> transport{\n        \"PREV\", \"PLAY / GO\", \"HOLD\", \"NEXT\"};\n    for(std::size_t index = 0U; index < mLiveTransport.size(); ++index)\n      Button(g, mLiveTransport[index], transport[index],\n             index == 1U && mPlug.TakePlaying()\n                 ? IColor(255, 10, 44, 25) : kRaised,\n             index == 1U ? kGood : kLine,\n             index == 1U ? kGood : kText, 9.7F);\n""",
    """    static constexpr std::array<const char*, 4U> transport{\n        \"PREV\", \"PLAY / GO\", \"HOLD\", \"NEXT\"};\n    for(std::size_t index = 0U; index < mLiveTransport.size(); ++index)\n    {\n      IColor fill = kRaised;\n      IColor border = kLine;\n      IColor text = kText;\n      if(index == 1U)\n      {\n        fill = mPlug.TakePlaying() ? IColor(255, 10, 44, 25)\n                                   : IColor(255, 13, 28, 20);\n        border = kGood;\n        text = kGood;\n      }\n      else if(index == 2U)\n      {\n        fill = IColor(255, 38, 28, 12);\n        border = kWarn;\n        text = kWarn;\n      }\n      Button(g, mLiveTransport[index], transport[index],\n             fill, border, text, 9.7F);\n    }\n""",
    "live transport styling",
)

runtime = replace_once(
    runtime,
    """    const std::size_t prepared = mPlug.ActiveSongIndex();\n    const int live = mPlug.ActiveTakeSongIndex();\n    std::string summary = \"PREPARADA \";\n    if(prepared < mPlug.SongCount())\n      summary += std::to_string(prepared + 1U) + \" · \" + mPlug.SongName(prepared);\n    else\n      summary += \"—\";\n    if(live >= 0 && static_cast<std::size_t>(live) < mPlug.SongCount())\n      summary += \"   |   AL AIRE \" + std::to_string(live + 1) + \" · \" +\n                 mPlug.SongName(static_cast<std::size_t>(live));\n    else\n      summary += \"   |   AL AIRE —\";\n    g.DrawText(IText(8.4F, kMuted, \"AeylaUI\",\n                     EAlign::Near, EVAlign::Middle),\n               summary.c_str(),\n               IRECT(mLiveSetlistPanel.L + 11.0F, mLiveSetlistPanel.T + 24.0F,\n                     mLiveSetlistPanel.R - 10.0F, mLiveSetlistPanel.T + 46.0F));\n""",
    """    const std::size_t prepared = mPlug.ActiveSongIndex();\n    const int live = mPlug.ActiveTakeSongIndex();\n    const bool compactStatus = mLiveSetlistPanel.H() < 470.0F;\n\n    std::string airName = \"—\";\n    if(live >= 0 && static_cast<std::size_t>(live) < mPlug.SongCount())\n      airName = std::to_string(live + 1) + \" · \" +\n                mPlug.SongName(static_cast<std::size_t>(live));\n    std::string preparedName = \"—\";\n    if(prepared < mPlug.SongCount())\n      preparedName = std::to_string(prepared + 1U) + \" · \" +\n                     mPlug.SongName(prepared);\n\n    if(compactStatus)\n    {\n      const IRECT status(mLiveSetlistPanel.L + 9.0F,\n                         mLiveSetlistPanel.T + 29.0F,\n                         mLiveSetlistPanel.R - 9.0F,\n                         mLiveSetlistPanel.T + 57.0F);\n      g.FillRoundRect(IColor(255, 12, 14, 19), status, 5.0F);\n      g.DrawRoundRect(kLine, status, 5.0F, nullptr, 1.0F);\n      const std::string compact = \"AIRE \" + airName +\n          \"   ·   PREP \" + preparedName;\n      g.DrawText(IText(8.2F, kMuted, \"AeylaUI\",\n                       EAlign::Near, EVAlign::Middle),\n                 compact.c_str(), status.GetPadded(-7.0F));\n    }\n    else\n    {\n      const IRECT air(mLiveSetlistPanel.L + 9.0F,\n                      mLiveSetlistPanel.T + 29.0F,\n                      mLiveSetlistPanel.R - 9.0F,\n                      mLiveSetlistPanel.T + 61.0F);\n      const IRECT prep(mLiveSetlistPanel.L + 9.0F,\n                       mLiveSetlistPanel.T + 66.0F,\n                       mLiveSetlistPanel.R - 9.0F,\n                       mLiveSetlistPanel.T + 98.0F);\n      g.FillRoundRect(IColor(255, 43, 13, 20), air, 5.0F);\n      g.DrawRoundRect(kDanger, air, 5.0F, nullptr, 1.0F);\n      g.DrawText(IText(7.8F, kDanger, \"AeylaUI\",\n                       EAlign::Near, EVAlign::Top),\n                 \"AL AIRE\", air.GetPadded(-7.0F));\n      g.DrawText(IText(10.6F, kText, \"AeylaUI\",\n                       EAlign::Near, EVAlign::Bottom),\n                 airName.c_str(), air.GetPadded(-7.0F));\n\n      g.FillRoundRect(IColor(255, 8, 23, 31), prep, 5.0F);\n      g.DrawRoundRect(kCyan, prep, 5.0F, nullptr, 1.0F);\n      g.DrawText(IText(7.8F, kCyan, \"AeylaUI\",\n                       EAlign::Near, EVAlign::Top),\n                 \"PREPARADA\", prep.GetPadded(-7.0F));\n      g.DrawText(IText(10.6F, kText, \"AeylaUI\",\n                       EAlign::Near, EVAlign::Bottom),\n                 preparedName.c_str(), prep.GetPadded(-7.0F));\n    }\n""",
    "live on-air/prepared hierarchy",
)

runtime = replace_once(
    runtime,
    """    Button(g, mLiveConfigButtons[index],\n           configuring ? \"CONFIGURANDO\" : \"CONFIGURAR\",\n""",
    """    Button(g, mLiveConfigButtons[index],\n           configuring ? \"EDITANDO\" : \"EDITAR\",\n""",
    "memory edit label",
)

runtime = replace_once(
    runtime,
    """    g.DrawText(IText(17.0F, on ? kGood : (ready ? kText : kWarn),\n""",
    """    g.DrawText(IText(19.0F, on ? kGood : (ready ? kText : kWarn),\n""",
    "toggle emphasis",
)

runtime = replace_once(
    runtime,
    """    const IRECT handle(handleX - 7.0F, track.T - 11.0F,\n                       handleX + 7.0F, track.B + 11.0F);\n""",
    """    const IRECT handle(handleX - 8.0F, track.T - 12.0F,\n                       handleX + 8.0F, track.B + 12.0F);\n""",
    "fader handle size",
)

runtime = replace_once(
    runtime,
    """    const float badgeW = recording ? 72.0F : 82.0F;\n    const IRECT badge(mRECT.MW() - badgeW * 0.5F, mRECT.T + 5.0F,\n                      mRECT.MW() + badgeW * 0.5F, mRECT.T + 30.0F);\n    g.FillRoundRect(IColor(255, 9, 10, 13), badge, 5.0F);\n    g.DrawRoundRect(inner, badge, 5.0F, nullptr, 1.2F);\n    g.DrawText(IText(10.3F, base, \"AeylaUI\",\n                     EAlign::Center, EVAlign::Middle),\n               recording ? \"● REC\" : \"▶ PLAY\", badge);\n""",
    """    // Header/footer already communicate REC/PLAY. Keep only the peripheral\n    // pulse so operator state is visible without covering navigation or status.\n""",
    "operator badge overlap",
)

runtime_path.write_text(runtime, encoding="utf-8")

print("R10.6 professional UI stabilization patch applied")
