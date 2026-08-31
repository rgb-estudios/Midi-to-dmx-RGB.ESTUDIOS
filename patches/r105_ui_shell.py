from pathlib import Path


def read(path):
    return Path(path).read_text(encoding='utf-8')


def write(path, text):
    Path(path).write_text(text, encoding='utf-8')


def replace_exact(path, old, new):
    text = read(path)
    count = text.count(old)
    if count != 1:
        raise SystemExit(f'{path}: exact block expected once, found {count}')
    write(path, text.replace(old, new))


def replace_between(path, start, end, new):
    text = read(path)
    if text.count(start) != 1 or text.count(end) != 1:
        raise SystemExit(f'{path}: range anchors not unique: {start!r} / {end!r}')
    a = text.index(start)
    b = text.index(end, a) + len(end)
    write(path, text[:a] + new + text[b:])

# -----------------------------------------------------------------------------
# Product-level workspace state. This keeps MainControl and RuntimeStatusControl
# in one navigation model without coupling either UI class to the other.
# 0=TOMA, 1=EN VIVO, 2=MIDI, 3=SISTEMA.
# -----------------------------------------------------------------------------
replace_exact(
    'product/AeylaVisualDmx/AeylaVisualDmx.h',
    '''  [[nodiscard]] bool GlobalBlackout() const noexcept\n  {\n    return mGlobalBlackout.load(std::memory_order_acquire);\n  }\n\n  [[nodiscard]] bool BackendReady() const noexcept\n''',
    '''  [[nodiscard]] bool GlobalBlackout() const noexcept\n  {\n    return mGlobalBlackout.load(std::memory_order_acquire);\n  }\n\n  // Canonical product workspace shared by the UI layers. This is presentation\n  // state only: changing workspace never touches ARM, blackout or transport.\n  void SetUiWorkspaceFromUI(int workspace) noexcept\n  {\n    mUiWorkspace.store(std::clamp(workspace, 0, 3),\n                       std::memory_order_release);\n  }\n\n  [[nodiscard]] int UiWorkspace() const noexcept\n  {\n    return mUiWorkspace.load(std::memory_order_acquire);\n  }\n\n  [[nodiscard]] bool BackendReady() const noexcept\n''')

replace_exact(
    'product/AeylaVisualDmx/AeylaVisualDmx.h',
    '''  std::atomic<bool> mOutputArmed{false};\n  std::atomic<bool> mGlobalBlackout{true};\n  std::atomic<bool> mEffectiveBlackout{true};\n''',
    '''  std::atomic<bool> mOutputArmed{false};\n  std::atomic<bool> mGlobalBlackout{true};\n  std::atomic<bool> mEffectiveBlackout{true};\n  std::atomic<int> mUiWorkspace{0};\n''')

# -----------------------------------------------------------------------------
# MainControl follows canonical workspace instead of owning a second nav state.
# -----------------------------------------------------------------------------
replace_exact(
    'product/AeylaVisualDmx/AeylaMainControl.h',
    '''  void Draw(IGraphics& g) override\n  {\n    BuildLayout();\n    g.FillRect(kBackground, mRECT);\n    DrawHeader(g);\n    DrawSetlist(g);\n    if(mWorkspaceView == WorkspaceView::take_editor)\n      DrawTakeEditor(g);\n    else if(mWorkspaceView == WorkspaceView::midi_show)\n      DrawMidiShow(g);\n    else\n      DrawRouting(g);\n  }\n''',
    '''  void Draw(IGraphics& g) override\n  {\n    SyncWorkspaceFromProduct();\n    BuildLayout();\n    g.FillRect(kBackground, mRECT);\n    DrawHeader(g);\n    DrawSetlist(g);\n    if(mWorkspaceView == WorkspaceView::take_editor)\n      DrawTakeEditor(g);\n    else if(mWorkspaceView == WorkspaceView::midi_show)\n      DrawMidiShow(g);\n    else\n      DrawRouting(g);\n  }\n''')

replace_exact(
    'product/AeylaVisualDmx/AeylaMainControl.h',
    '''    if(Contains(mTakeEditorTab, x, y))\n    {\n      mWorkspaceView = WorkspaceView::take_editor;\n      mMessage = "TOMA / EDICIÓN · captura, recorte y reproducción de muestras DMX.";\n      SetDirty(false);\n      return;\n    }\n    if(Contains(mNetworkOutputTab, x, y))\n    {\n      mWorkspaceView = WorkspaceView::network_output;\n      mMessage = "RED / SALIDA · adaptadores, IPv4, armado y telemetría Art-Net.";\n      SetDirty(false);\n      return;\n    }\n    if(Contains(mMidiShowTab, x, y))\n    {\n      mWorkspaceView = WorkspaceView::midi_show;\n      mMessage = "MIDI / SHOW · automatización sincronizada por muestras del DAW.";\n      SetDirty(false);\n      return;\n    }\n''',
    '''    if(Contains(mTakeEditorTab, x, y))\n    {\n      mPlug.SetUiWorkspaceFromUI(0);\n      SyncWorkspaceFromProduct();\n      mMessage = "TOMA · captura, edición y reproducción DMX.";\n      SetDirty(false);\n      return;\n    }\n    if(Contains(mNetworkOutputTab, x, y))\n    {\n      mPlug.SetUiWorkspaceFromUI(3);\n      SyncWorkspaceFromProduct();\n      mMessage = "SISTEMA · red, salida Art-Net y diagnóstico.";\n      SetDirty(false);\n      return;\n    }\n    if(Contains(mMidiShowTab, x, y))\n    {\n      mPlug.SetUiWorkspaceFromUI(2);\n      SyncWorkspaceFromProduct();\n      mMessage = "MIDI · automatización y Learn del show.";\n      SetDirty(false);\n      return;\n    }\n''')

replace_exact(
    'product/AeylaVisualDmx/AeylaMainControl.h',
    '''  static bool Contains(const IRECT& rect, float x, float y) noexcept\n  {\n    return x >= rect.L && x <= rect.R && y >= rect.T && y <= rect.B;\n  }\n''',
    '''  void SyncWorkspaceFromProduct() noexcept\n  {\n    switch(mPlug.UiWorkspace())\n    {\n      case 2: mWorkspaceView = WorkspaceView::midi_show; break;\n      case 3: mWorkspaceView = WorkspaceView::network_output; break;\n      default: mWorkspaceView = WorkspaceView::take_editor; break;\n    }\n  }\n\n  static bool Contains(const IRECT& rect, float x, float y) noexcept\n  {\n    return x >= rect.L && x <= rect.R && y >= rect.T && y <= rect.B;\n  }\n''')

# -----------------------------------------------------------------------------
# RuntimeStatus becomes the canonical shell: brand + 4 tabs + network + safety.
# -----------------------------------------------------------------------------
replace_exact(
    'product/AeylaVisualDmx/AeylaRuntimeStatusControl.h',
    '''  void Draw(IGraphics& g) override\n  {\n    if(mLiveOpen)\n      DrawLive(g);\n    else\n      DrawNormal(g);\n    DrawOperatorFrame(g);\n  }\n\n  bool IsHit(float x, float y) const override\n  {\n    if(mLiveOpen)\n      return Contains(mRECT, x, y);\n    if(Contains(ArchiveButton(), x, y)) return true;\n    const auto topLive = TopLiveTab();\n    if(topLive.W() > 0.0F && Contains(topLive, x, y)) return true;\n    const auto compactLive = CompactLiveButton();\n    if(compactLive.W() > 0.0F && Contains(compactLive, x, y)) return true;\n    if(mFileMenuOpen && Contains(FileMenuPanel(), x, y)) return true;\n    return Footer().Contains(x, y);\n  }\n''',
    '''  void Draw(IGraphics& g) override\n  {\n    mLiveOpen = mPlug.UiWorkspace() == 1;\n    if(mLiveOpen)\n      DrawLive(g);\n    else\n      DrawNormal(g);\n    DrawOperatorFrame(g);\n  }\n\n  bool IsHit(float x, float y) const override\n  {\n    if(Contains(Header(), x, y) || Contains(Footer(), x, y)) return true;\n    if(mPlug.UiWorkspace() == 1) return Contains(mRECT, x, y);\n    if(mFileMenuOpen && Contains(FileMenuPanel(), x, y)) return true;\n    return false;\n  }\n''')

replace_between(
    'product/AeylaVisualDmx/AeylaRuntimeStatusControl.h',
    '''  void OnMouseDown(float x, float y, const IMouseMod& mod) override\n  {''',
    '''  void OnMouseDrag(float x, float y, float dX, float dY,\n                   const IMouseMod& mod) override\n  {''',
    '''  void OnMouseDown(float x, float y, const IMouseMod& mod) override\n  {\n    (void)mod;\n\n    // The shell is always first. Workspace navigation is presentation-only and\n    // can never touch physical authority. ARM and APAGÓN are explicit actions.\n    for(std::size_t index = 0U; index < 4U; ++index)\n    {\n      if(!Contains(NavTab(index), x, y)) continue;\n      mPlug.SetUiWorkspaceFromUI(static_cast<int>(index));\n      mLiveOpen = index == 1U;\n      mFileMenuOpen = false;\n      mLiveConfigIndex = -1;\n      mDraggingMemory = -1;\n      if(mLiveOpen)\n        mLiveMessage = "EN VIVO · operación limpia; CONFIGURAR abre Learn, modo y fade sólo para una memoria.";\n      SetDirty(false);\n      return;\n    }\n\n    if(Contains(HeaderArmButton(), x, y))\n    {\n      ReportLive(mPlug.ToggleTakeOutputArmFromUI());\n      SetDirty(false);\n      return;\n    }\n\n    if(Contains(HeaderBlackoutButton(), x, y))\n    {\n      const bool enable = !mPlug.GlobalBlackout();\n      mPlug.SetBlackoutFromUI(enable);\n      mLiveMessageError = false;\n      mLiveMessage = enable\n          ? "APAGÓN TOTAL · DMX 0 continuo · ARM y carrier conservados"\n          : "APAGÓN LIBERADO · vuelve el estado subyacente sin rearmar";\n      SetDirty(false);\n      return;\n    }\n\n    if(mPlug.UiWorkspace() == 1)\n    {\n      HandleLiveMouseDown(x, y);\n      return;\n    }\n\n    if(Contains(ArchiveButton(), x, y))\n    {\n      mFileMenuOpen = !mFileMenuOpen;\n      SetDirty(false);\n      return;\n    }\n\n    if(mFileMenuOpen)\n    {\n      BuildFileMenuButtons();\n      for(std::size_t index = 0U; index < mFileButtons.size(); ++index)\n      {\n        if(!Contains(mFileButtons[index], x, y)) continue;\n        mFileMenuOpen = false;\n        HandleFileAction(index);\n        return;\n      }\n      if(!Contains(FileMenuPanel(), x, y))\n      {\n        mFileMenuOpen = false;\n        SetDirty(false);\n      }\n    }\n  }\n\n  void OnMouseDrag(float x, float y, float dX, float dY,\n                   const IMouseMod& mod) override\n  {''')

# Header geometry. Disable legacy standalone EN VIVO accessors.
replace_exact(
    'product/AeylaVisualDmx/AeylaRuntimeStatusControl.h',
    '''  [[nodiscard]] IRECT Footer() const noexcept\n  {\n    return IRECT(mRECT.L, mRECT.B - 46.0F, mRECT.R, mRECT.B);\n  }\n''',
    '''  [[nodiscard]] IRECT Header() const noexcept\n  {\n    return IRECT(mRECT.L, mRECT.T, mRECT.R, mRECT.T + 82.0F);\n  }\n\n  [[nodiscard]] IRECT NavTab(std::size_t index) const noexcept\n  {\n    const float left = mRECT.L + 14.0F;\n    const float width = std::clamp((mRECT.W() - 28.0F) * 0.105F, 88.0F, 112.0F);\n    const float gap = 5.0F;\n    const float x = left + static_cast<float>(index) * (width + gap);\n    return IRECT(x, mRECT.T + 49.0F, x + width, mRECT.T + 78.0F);\n  }\n\n  [[nodiscard]] IRECT HeaderBlackoutButton() const noexcept\n  {\n    return IRECT(mRECT.R - 150.0F, mRECT.T + 10.0F,\n                 mRECT.R - 12.0F, mRECT.T + 40.0F);\n  }\n\n  [[nodiscard]] IRECT HeaderArmButton() const noexcept\n  {\n    return IRECT(mRECT.R - 260.0F, mRECT.T + 10.0F,\n                 mRECT.R - 158.0F, mRECT.T + 40.0F);\n  }\n\n  [[nodiscard]] IRECT Footer() const noexcept\n  {\n    return IRECT(mRECT.L, mRECT.B - 46.0F, mRECT.R, mRECT.B);\n  }\n''')

replace_exact(
    'product/AeylaVisualDmx/AeylaRuntimeStatusControl.h',
    '''  [[nodiscard]] IRECT CompactLiveButton() const noexcept\n  {\n    if(mRECT.W() >= 1080.0F) return {};\n    const auto footer = Footer();\n    return IRECT(footer.R - 226.0F, footer.T + 7.0F,\n                 footer.R - 126.0F, footer.B - 7.0F);\n  }\n\n  [[nodiscard]] IRECT TopLiveTab() const noexcept\n  {\n    if(mRECT.W() < 1080.0F) return {};\n    return IRECT(mRECT.L + 624.0F, mRECT.T + 26.0F,\n                 mRECT.L + 724.0F, mRECT.T + 72.0F);\n  }\n''',
    '''  [[nodiscard]] IRECT CompactLiveButton() const noexcept { return {}; }\n  [[nodiscard]] IRECT TopLiveTab() const noexcept { return {}; }\n''')

# Replace old brand/live access with one unified shell.
replace_between(
    'product/AeylaVisualDmx/AeylaRuntimeStatusControl.h',
    '''  void DrawNormal(IGraphics& g)\n  {''',
    '''  void DrawFooter(IGraphics& g)\n  {''',
    '''  void DrawNormal(IGraphics& g)\n  {\n    DrawUnifiedHeader(g);\n    DrawFooter(g);\n    if(mFileMenuOpen)\n      DrawFileMenu(g);\n  }\n\n  void DrawUnifiedHeader(IGraphics& g)\n  {\n    const auto header = Header();\n    g.FillRect(kBackground, header);\n    g.DrawLine(kLine, header.L, header.B - 1.0F, header.R, header.B - 1.0F,\n               nullptr, 1.0F);\n\n    g.DrawText(IText(18.5F, kText, "AeylaUI", EAlign::Near, EVAlign::Middle),\n               "RGB LIVE CONTROL",\n               IRECT(header.L + 14.0F, header.T + 4.0F,\n                     header.L + 245.0F, header.T + 31.0F));\n    g.DrawText(IText(9.0F, kMuted, "AeylaUI", EAlign::Near, EVAlign::Middle),\n               "RGB ESTUDIOS · SHOW / AEYLA · R10.5 PRETEST",\n               IRECT(header.L + 14.0F, header.T + 28.0F,\n                     header.L + 330.0F, header.T + 45.0F));\n\n    // Sparse Campo Vivo signature: identity cue, not decoration.\n    g.DrawLine(IColor(255, 232, 166, 201), header.L + 14.0F, header.T + 43.0F,\n               header.L + 104.0F, header.T + 43.0F, nullptr, 1.0F);\n    g.DrawLine(kBrand, header.L + 23.0F, header.T + 46.0F,\n               header.L + 123.0F, header.T + 46.0F, nullptr, 1.0F);\n    g.DrawLine(kCyan, header.L + 14.0F, header.T + 47.5F,\n               header.L + 94.0F, header.T + 47.5F, nullptr, 1.0F);\n\n    static constexpr std::array<const char*, 4U> tabs{\n        "TOMA", "EN VIVO", "MIDI", "SISTEMA"};\n    const int active = mPlug.UiWorkspace();\n    for(std::size_t index = 0U; index < tabs.size(); ++index)\n    {\n      const auto tab = NavTab(index);\n      const bool selected = active == static_cast<int>(index);\n      g.DrawText(IText(10.4F, selected ? kText : kMuted, "AeylaUI",\n                       EAlign::Center, EVAlign::Middle), tabs[index], tab);\n      if(selected)\n        g.FillRect(kBrand, IRECT(tab.L + 12.0F, tab.B - 2.0F,\n                                tab.R - 12.0F, tab.B));\n    }\n\n    const auto tx = mPlug.ArtNetOutputStatus();\n    const float statusRight = HeaderArmButton().L - 8.0F;\n    const float statusLeft = std::max(header.L + 470.0F, statusRight - 156.0F);\n    if(statusRight - statusLeft > 70.0F)\n    {\n      const std::string net = mPlug.BackendReady()\n          ? "ART-NET · " + std::to_string(tx.configured_fps) + " Hz"\n          : "ART-NET · SIN SALIDA";\n      Pill(g, IRECT(statusLeft, header.T + 10.0F, statusRight, header.T + 40.0F),\n           net, mPlug.BackendReady() ? kGood : kWarn);\n    }\n\n    const bool armed = mPlug.TakeOutputArmed() || mPlug.OutputArmed();\n    Button(g, HeaderArmButton(), armed ? "DESARMAR" : "ARMAR",\n           armed ? IColor(255, 18, 31, 24) : kRaised,\n           armed ? kGood : kLine, armed ? kGood : kText, 9.6F);\n\n    const bool blackout = mPlug.GlobalBlackout();\n    Button(g, HeaderBlackoutButton(),\n           blackout ? "APAGÓN TOTAL ACTIVO" : "APAGÓN TOTAL",\n           blackout ? kDanger : IColor(255, 26, 13, 17), kDanger,\n           blackout ? kText : kDanger, 8.9F);\n  }\n\n  void DrawFooter(IGraphics& g)\n  {''')

# OpenLive becomes a nav change instead of a modal flag only.
replace_exact(
    'product/AeylaVisualDmx/AeylaRuntimeStatusControl.h',
    '''  void OpenLiveWorkspace()\n  {\n    mLiveOpen = true;\n    mFileMenuOpen = false;\n''',
    '''  void OpenLiveWorkspace()\n  {\n    mPlug.SetUiWorkspaceFromUI(1);\n    mLiveOpen = true;\n    mFileMenuOpen = false;\n''')

# Live layout now lives below the same canonical shell and above same footer.
replace_exact(
    'product/AeylaVisualDmx/AeylaRuntimeStatusControl.h',
    '''    const float left = mRECT.L + 16.0F;\n    const float right = mRECT.R - 16.0F;\n    const float top = mRECT.T + 14.0F;\n\n    mLiveCloseButton = IRECT(right - 78.0F, top, right, top + 32.0F);\n    mLivePanicButton = IRECT(right - 206.0F, top, right - 86.0F, top + 32.0F);\n    mLiveArmButton = IRECT(right - 344.0F, top, right - 214.0F, top + 32.0F);\n\n    const float transportTop = top + 48.0F;\n''',
    '''    const float left = mRECT.L + 16.0F;\n    const float right = mRECT.R - 16.0F;\n    const float top = Header().B + 8.0F;\n\n    mLiveCloseButton = {};\n    mLivePanicButton = {};\n    mLiveArmButton = {};\n\n    const float transportTop = top;\n''')

replace_exact(
    'product/AeylaVisualDmx/AeylaRuntimeStatusControl.h',
    '''    const float contentTop = transportTop + 46.0F;\n    const float contentBottom = mRECT.B - 58.0F;\n''',
    '''    const float contentTop = transportTop + 44.0F;\n    const float contentBottom = Footer().T - 38.0F;\n''')

replace_exact(
    'product/AeylaVisualDmx/AeylaRuntimeStatusControl.h',
    '''    mLiveMessageRect = IRECT(left, contentBottom + 7.0F,\n                             right, mRECT.B - 10.0F);\n''',
    '''    mLiveMessageRect = IRECT(left, contentBottom + 6.0F,\n                             right, Footer().T - 5.0F);\n''')

# Remove duplicate product/safety header from EN VIVO. Canonical shell is drawn
# after content so it is always visually and interactively authoritative.
replace_between(
    'product/AeylaVisualDmx/AeylaRuntimeStatusControl.h',
    '''    g.FillRect(kBackground, mRECT);\n\n    g.DrawText(IText(21.0F, kText, "AeylaUI",''',
    '''    static constexpr std::array<const char*, 4U> transport{''',
    '''    g.FillRect(kBackground, mRECT);\n\n    static constexpr std::array<const char*, 4U> transport{''')

replace_exact(
    'product/AeylaVisualDmx/AeylaRuntimeStatusControl.h',
    '''    g.DrawText(IText(9.8F, mLiveMessageError ? kDanger : kMuted,\n                     "AeylaUI", EAlign::Near, EVAlign::Middle),\n               mLiveMessage.c_str(), mLiveMessageRect);\n  }\n''',
    '''    g.DrawText(IText(9.8F, mLiveMessageError ? kDanger : kMuted,\n                     "AeylaUI", EAlign::Near, EVAlign::Middle),\n               mLiveMessage.c_str(), mLiveMessageRect);\n    DrawUnifiedHeader(g);\n    DrawFooter(g);\n    if(mFileMenuOpen)\n      DrawFileMenu(g);\n  }\n''')

# Legacy close action, if ever reached by an old layout state, returns to TOMA.
replace_exact(
    'product/AeylaVisualDmx/AeylaRuntimeStatusControl.h',
    '''    if(Contains(mLiveCloseButton, x, y))\n    {\n      mLiveOpen = false;\n      mLiveConfigIndex = -1;\n      mDraggingMemory = -1;\n      SetDirty(false);\n      return;\n    }\n''',
    '''    if(Contains(mLiveCloseButton, x, y))\n    {\n      mPlug.SetUiWorkspaceFromUI(0);\n      mLiveOpen = false;\n      mLiveConfigIndex = -1;\n      mDraggingMemory = -1;\n      SetDirty(false);\n      return;\n    }\n''')

# Update stale message semantics and visible build identity.
for path in ['product/AeylaVisualDmx/AeylaRuntimeStatusControl.h',
             'product/AeylaVisualDmx/AeylaMainControl.h']:
    text = read(path)
    text = text.replace('R10.4 PRETEST', 'R10.5 PRETEST')
    text = text.replace('APAGÓN ACTIVO · salida desarmada.',
                        'APAGÓN TOTAL · DMX 0 continuo; ARM se conserva.')
    write(path, text)

print('R10.5 unified UI shell patch applied')
