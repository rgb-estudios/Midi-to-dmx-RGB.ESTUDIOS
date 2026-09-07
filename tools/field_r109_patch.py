from pathlib import Path


def replace_once(path: str, old: str, new: str) -> None:
    p = Path(path)
    text = p.read_text(encoding="utf-8")
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"{path}: expected one match, found {count}")
    p.write_text(text.replace(old, new, 1), encoding="utf-8")


# 1) EN VIVO DMX Learn: never learn while AEYLA is transmitting, recover the
# RX listener exactly on this explicit authoring action, and require a fresh
# packet before accepting a snapshot. Take REC code remains untouched.
replace_once(
    "product/AeylaVisualDmx/AeylaVisualDmx.h",
'''  [[nodiscard]] aeyla::product::AuthoringResult LearnLiveMemoryFromAvolitesFromUI(
      std::size_t index)
  {
    aeyla::live_memory_session::register_runtime(
        this, &mArtNetOutput, &mArtNetCapture);
    const auto result = aeyla::live_memory_session::learn_from_avolites(this, index);
    if(result.succeeded) CommitLiveMemoryPersistenceIfDirtyFromUI();
    return {result.succeeded, {}, result.message};
  }
''',
'''  [[nodiscard]] aeyla::product::AuthoringResult LearnLiveMemoryFromAvolitesFromUI(
      std::size_t index)
  {
    // DMX Learn must observe Avolites only. If AEYLA owns physical Art-Net
    // authority on the same show LAN, directed-broadcast TX can be received by
    // our RX socket and contaminate OFF/ON snapshots. Authoring therefore
    // requires an explicit disarmed state; this never changes ARM automatically.
    const auto output = mArtNetOutput.stats();
    if(TakeOutputArmed() || OutputArmed() || output.enabled ||
       output.override_enabled)
      return {false, {},
              "APRENDER DMX · DESARMA la salida AEYLA primero para evitar capturar su propio Art-Net"};
    if(TakeRecording())
      return {false, {}, "APRENDER DMX · detén GRABAR antes de capturar una memoria"};

    // Take REC already recovers its RX listener before capture. EN VIVO used
    // to skip that preflight, so a fresh plug-in instance could attempt Learn
    // with Art-Net RX stopped. Recover only on this non-realtime UI action.
    auto capture = mArtNetCapture.stats();
    if(!capture.running)
    {
      if(NetworkInterfaceCount() == 0U)
        (void)RefreshNetworkInterfacesFromUI();
      else
        RestartCaptureInputFromRouting();

      // Avolites normally publishes at show cadence. Give the newly opened UDP
      // socket a short bounded window to receive its first packet; never wait
      // on the audio/runtime thread and never arm TX as a side effect.
      for(int attempt = 0; attempt < 30; ++attempt)
      {
        capture = mArtNetCapture.stats();
        if(capture.running && capture.signal_present &&
           capture.last_packet_age_ms <= 150.0)
          break;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }
    }
    else
      capture = mArtNetCapture.stats();

    if(!capture.running)
      return {false, {},
              "APRENDER DMX · Art-Net RX no pudo iniciar; revisa SISTEMA / adaptador RX"};
    if(!capture.signal_present || capture.last_packet_age_ms > 150.0)
      return {false, {},
              "APRENDER DMX · no hay un frame Art-Net fresco de Avolites en el RX seleccionado"};

    aeyla::live_memory_session::register_runtime(
        this, &mArtNetOutput, &mArtNetCapture);
    const auto result = aeyla::live_memory_session::learn_from_avolites(this, index);
    if(result.succeeded) CommitLiveMemoryPersistenceIfDirtyFromUI();
    return {result.succeeded, {}, result.message};
  }

  // Explicit cross-platform relink. This changes only the external library
  // locator; .aeylashow and every .aeylatake byte remain untouched. Pending
  // host bindings are resolved by basename + trim against the exact directory
  // selected by the operator, never by guessing a Windows/macOS path mapping.
  [[nodiscard]] aeyla::product::AuthoringResult RelinkTakeLibraryFromUI(
      const std::filesystem::path& directory)
  {
    if(TakeRecording())
      return {false, {}, "VINCULAR TOMAS · detén GRABAR primero"};
    if(TakePlaying())
      return {false, {}, "VINCULAR TOMAS · detén PLAY primero"};
    if(TakeOutputArmed() || OutputArmed())
      return {false, {}, "VINCULAR TOMAS · desarma la salida física primero"};
    if(directory.empty())
      return {false, {}, "VINCULAR TOMAS · no se seleccionó una carpeta"};

    std::error_code fsError;
    if(!std::filesystem::is_directory(directory, fsError) || fsError)
      return {false, {}, "VINCULAR TOMAS · la carpeta seleccionada no está disponible"};

    {
      const std::scoped_lock lock(mModelMutex);
      aeyla::take_library_session::ensure_scope(
          this, mModel.project_document().project_id);
    }
    aeyla::take_library_session::set_directory(this, directory);
    const auto restored =
        aeyla::take_library_session::restore_persisted_state(this);
    {
      const std::scoped_lock lock(mModelMutex);
      RefreshHostStateCacheLocked();
    }

    const auto scan = aeyla::capture::scan_take_directory(directory, {});
    if(!scan.ok())
      return {false, {}, "VINCULAR TOMAS · " + scan.error};
    if(scan.entries.empty())
      return {false, {}, "VINCULAR TOMAS · la carpeta no contiene archivos .aeylatake válidos"};

    std::string message = "BIBLIOTECA DMX VINCULADA · " +
        std::to_string(scan.entries.size()) + " TOMAS";
    if(restored.restored_bindings > 0U || restored.missing_bindings > 0U)
      message += " · " + std::to_string(restored.restored_bindings) +
          " ASOCIACIONES RESTAURADAS" +
          (restored.missing_bindings == 0U
               ? std::string{}
               : " · " + std::to_string(restored.missing_bindings) +
                     " PENDIENTES");
    return {true, {}, std::move(message)};
  }
''')

# 2) Mac show picker fix already validated in R10.8.1. Custom extension names
# passed to Cocoa must not contain the leading dot.
replace_once(
    "product/AeylaVisualDmx/AeylaRuntimeStatusControl.h",
    'mDialogFileName, mDialogPath, EFileAction::Open, ".aeylashow",',
    'mDialogFileName, mDialogPath, EFileAction::Open, "aeylashow",')
replace_once(
    "product/AeylaVisualDmx/AeylaRuntimeStatusControl.h",
    'mDialogFileName, mDialogPath, EFileAction::Save, ".aeylashow",',
    'mDialogFileName, mDialogPath, EFileAction::Save, "aeylashow",')

# 3) Surface the cross-platform relink as one explicit operator action instead
# of hiding it behind first playback.
replace_once(
    "product/AeylaVisualDmx/AeylaRuntimeStatusControl.h",
'''  [[nodiscard]] IRECT FileMenuPanel() const noexcept
  {
    const auto archive = ArchiveButton();
    return IRECT(archive.R - 310.0F, archive.T - 92.0F,
                 archive.R, archive.T - 8.0F);
  }
''',
'''  [[nodiscard]] IRECT FileMenuPanel() const noexcept
  {
    const auto archive = ArchiveButton();
    return IRECT(archive.R - 310.0F, archive.T - 132.0F,
                 archive.R, archive.T - 8.0F);
  }
''')
replace_once(
    "product/AeylaVisualDmx/AeylaRuntimeStatusControl.h",
'''  void BuildFileMenuButtons()
  {
    const auto panel = FileMenuPanel();
    const float gap = 6.0F;
    const float innerL = panel.L + 8.0F;
    const float innerR = panel.R - 8.0F;
    const float innerT = panel.T + 8.0F;
    const float innerB = panel.B - 8.0F;
    const float w = (innerR - innerL - gap) * 0.5F;
    const float h = (innerB - innerT - gap) * 0.5F;
    mFileButtons[0] = IRECT(innerL, innerT, innerL + w, innerT + h);
    mFileButtons[1] = IRECT(innerL + w + gap, innerT, innerR, innerT + h);
    mFileButtons[2] = IRECT(innerL, innerT + h + gap,
                            innerL + w, innerB);
    mFileButtons[3] = IRECT(innerL + w + gap, innerT + h + gap,
                            innerR, innerB);
  }
''',
'''  void BuildFileMenuButtons()
  {
    const auto panel = FileMenuPanel();
    const float gap = 6.0F;
    const float innerL = panel.L + 8.0F;
    const float innerR = panel.R - 8.0F;
    const float innerT = panel.T + 8.0F;
    const float innerB = panel.B - 8.0F;
    const float w = (innerR - innerL - gap) * 0.5F;
    const float h = (innerB - innerT - gap * 2.0F) / 3.0F;
    mFileButtons[0] = IRECT(innerL, innerT, innerL + w, innerT + h);
    mFileButtons[1] = IRECT(innerL + w + gap, innerT, innerR, innerT + h);
    const float row2 = innerT + h + gap;
    mFileButtons[2] = IRECT(innerL, row2, innerL + w, row2 + h);
    mFileButtons[3] = IRECT(innerL + w + gap, row2, innerR, row2 + h);
    const float row3 = row2 + h + gap;
    mFileButtons[4] = IRECT(innerL, row3, innerR, innerB);
  }
''')
replace_once(
    "product/AeylaVisualDmx/AeylaRuntimeStatusControl.h",
'''    static constexpr std::array<const char*, 4U> labels{
        "NUEVO", "ABRIR", "GUARDAR", "GUARDAR COMO"};
    for(std::size_t index = 0U; index < mFileButtons.size(); ++index)
    {
      const bool blocked = mPlug.TakeRecording() && index < 2U;
      Button(g, mFileButtons[index], labels[index],
''',
'''    static constexpr std::array<const char*, 5U> labels{
        "NUEVO", "ABRIR", "GUARDAR", "GUARDAR COMO", "VINCULAR TOMAS DMX"};
    for(std::size_t index = 0U; index < mFileButtons.size(); ++index)
    {
      const bool blocked = (mPlug.TakeRecording() && index < 2U) ||
          (index == 4U && (mPlug.TakeRecording() || mPlug.TakePlaying() ||
                          mPlug.TakeOutputArmed() || mPlug.OutputArmed()));
      Button(g, mFileButtons[index], labels[index],
''')
replace_once(
    "product/AeylaVisualDmx/AeylaRuntimeStatusControl.h",
'''    if(index == 3U)
      PromptSaveAs();
  }
''',
'''    if(index == 3U)
    {
      PromptSaveAs();
      return;
    }
    if(index == 4U)
      PromptTakeLibraryRelink();
  }
''')
replace_once(
    "product/AeylaVisualDmx/AeylaRuntimeStatusControl.h",
'''  void PromptSaveAs()
  {
''',
'''  void PromptTakeLibraryRelink()
  {
    auto* ui = GetUI();
    if(ui == nullptr) return;
    WDL_String selected;
    ui->PromptForDirectory(selected);
    if(Empty(selected)) return;
    ReportLive(mPlug.RelinkTakeLibraryFromUI(PathFromUtf8(selected.Get())));
    SetDirty(false);
  }

  void PromptSaveAs()
  {
''')
replace_once(
    "product/AeylaVisualDmx/AeylaRuntimeStatusControl.h",
    '  std::array<IRECT, 4U> mFileButtons{};',
    '  std::array<IRECT, 5U> mFileButtons{};')
replace_once(
    "product/AeylaVisualDmx/AeylaRuntimeStatusControl.h",
    '"RGB ESTUDIOS · SHOW / " + projectName + " · R10.8 PRETEST";',
    '"RGB ESTUDIOS · SHOW / " + projectName + " · R10.9 PRETEST";')

# 4) macOS adapter preference: choose wired en* (built-in/USB Ethernet) before
# virtual interfaces such as bridge/utun. Windows keeps the previous fallback.
replace_once(
    "product/AeylaVisualDmx/AeylaShowPlayerIntegration.cpp",
'''    const auto preferred = std::find_if(
        mNetworkInterfaces.begin(), mNetworkInterfaces.end(),
        [](const aeyla::network::NetworkInterface& item) {
          return !item.wireless && !item.ipv4.empty();
        });
    const std::size_t preferredIndex = preferred == mNetworkInterfaces.end()
        ? 0U
        : static_cast<std::size_t>(
              std::distance(mNetworkInterfaces.begin(), preferred));
''',
'''    // On macOS, virtual bridge/utun interfaces can sort before a USB-C
    // Ethernet adapter. Prefer a cabled BSD en* interface first, then retain
    // the generic non-wireless fallback used on Windows/other platforms.
    auto preferred = std::find_if(
        mNetworkInterfaces.begin(), mNetworkInterfaces.end(),
        [](const aeyla::network::NetworkInterface& item) {
          return !item.wireless && !item.ipv4.empty() &&
                 item.name.rfind("en", 0U) == 0U;
        });
    if(preferred == mNetworkInterfaces.end())
      preferred = std::find_if(
          mNetworkInterfaces.begin(), mNetworkInterfaces.end(),
          [](const aeyla::network::NetworkInterface& item) {
            return !item.wireless && !item.ipv4.empty();
          });
    const std::size_t preferredIndex = preferred == mNetworkInterfaces.end()
        ? 0U
        : static_cast<std::size_t>(
              std::distance(mNetworkInterfaces.begin(), preferred));
''')

# 5) During the two-step DMX Learn, pin the source identity observed at OFF and
# reject a second snapshot from a different Art-Net source. This state is
# transient and does not alter live.bin or any Take file format.
replace_once(
    "product/AeylaVisualDmx/AeylaLiveMemorySession.cpp",
'''  bool configured{false};
  bool learn_pending{false};
  DmxUniverse learn_baseline{};

  bool midi_learn_pending{false};
''',
'''  bool configured{false};
  bool learn_pending{false};
  DmxUniverse learn_baseline{};
  std::string learn_source_ipv4;

  bool midi_learn_pending{false};
''')
replace_once(
    "product/AeylaVisualDmx/AeylaLiveMemorySession.cpp",
'''  if(!slot.learn_pending) {
    slot.learn_baseline = rx;
    slot.learn_pending = true;
    return {true,
''',
'''  if(!slot.learn_pending) {
    slot.learn_baseline = rx;
    slot.learn_source_ipv4 = stats.source_ipv4;
    slot.learn_pending = true;
    return {true,
''')
replace_once(
    "product/AeylaVisualDmx/AeylaLiveMemorySession.cpp",
'''  output::LiveMemoryMask mask;
  for(std::size_t channel = 0U; channel < rx.size(); ++channel) {
''',
'''  if(!slot.learn_source_ipv4.empty() &&
     !stats.source_ipv4.empty() &&
     slot.learn_source_ipv4 != stats.source_ipv4) {
    return {false,
            slot.definition.name +
                " · la fuente Art-Net cambió entre OFF y ON. Mantengo el OFF; selecciona una sola fuente y repite CAPTURAR ON"};
  }

  output::LiveMemoryMask mask;
  for(std::size_t channel = 0U; channel < rx.size(); ++channel) {
''')
replace_once(
    "product/AeylaVisualDmx/AeylaLiveMemorySession.cpp",
'''  slot.definition = learned;
  slot.configured = true;
  slot.learn_pending = false;
  session.persistence_dirty = true;
''',
'''  slot.definition = learned;
  slot.configured = true;
  slot.learn_pending = false;
  slot.learn_source_ipv4.clear();
  session.persistence_dirty = true;
''')
replace_once(
    "product/AeylaVisualDmx/AeylaLiveMemorySession.cpp",
'''  auto& slot = session.slots[index];
  slot.learn_pending = false;
  return {true, slot.definition.name + " · aprendizaje DMX cancelado"};
''',
'''  auto& slot = session.slots[index];
  slot.learn_pending = false;
  slot.learn_source_ipv4.clear();
  return {true, slot.definition.name + " · aprendizaje DMX cancelado"};
''')

print("R10.9 field patch applied")
