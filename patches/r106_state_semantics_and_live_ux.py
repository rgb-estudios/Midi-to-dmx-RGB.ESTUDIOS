from pathlib import Path


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"{label}: expected exactly 1 match, found {count}")
    return text.replace(old, new, 1)


# -----------------------------------------------------------------------------
# Runtime: a MIDI Learn completed on the runtime thread must mark the project
# unsaved immediately, just like DMX Learn/fade/mode changes made from the UI.
# -----------------------------------------------------------------------------
cpp_path = Path("product/AeylaVisualDmx/AeylaVisualDmx.cpp")
cpp = cpp_path.read_text(encoding="utf-8")
cpp = replace_once(
    cpp,
    '''    if(event.reserved == 1U)\n      continue;\n    mModel.handle_host_event(event);\n  }\n}\n''',
    '''    if(event.reserved == 1U)\n      continue;\n    mModel.handle_host_event(event);\n  }\n\n  // MIDI Learn is finalized on this non-realtime thread. Persist its authored\n  // binding into the project controller and mark the project unsaved here so\n  // the footer cannot continue to claim GUARDADO after a successful Learn.\n  CommitLiveMemoryPersistenceDirtyLocked();\n}\n''',
    "runtime live-memory dirty bridge",
)
cpp_path.write_text(cpp, encoding="utf-8")


# -----------------------------------------------------------------------------
# Canonical shell / EN VIVO: truthful physical status, larger show controls,
# no stale legacy live controls, and safer authoring interactions.
# -----------------------------------------------------------------------------
runtime_path = Path("product/AeylaVisualDmx/AeylaRuntimeStatusControl.h")
runtime = runtime_path.read_text(encoding="utf-8")

runtime = replace_once(
    runtime,
    '''    const auto tx = mPlug.ArtNetOutputStatus();\n    const float statusRight = HeaderArmButton().L - 8.0F;\n    const float statusLeft = std::max(header.L + 470.0F, statusRight - 156.0F);\n    if(statusRight - statusLeft > 70.0F)\n    {\n      const std::string net = mPlug.BackendReady()\n          ? "ART-NET · " + std::to_string(tx.configured_fps) + " Hz"\n          : "ART-NET · SIN SALIDA";\n      Pill(g, IRECT(statusLeft, header.T + 10.0F, statusRight, header.T + 40.0F),\n           net, mPlug.BackendReady() ? kGood : kWarn);\n    }\n''',
    '''    const auto tx = mPlug.ArtNetOutputStatus();\n    const bool physicalAuthority = tx.enabled || tx.override_enabled;\n    const float statusRight = HeaderArmButton().L - 8.0F;\n    const float statusLeft = std::max(header.L + 470.0F, statusRight - 168.0F);\n    if(statusRight - statusLeft > 70.0F)\n    {\n      std::string net = "ART-NET · SIN SALIDA";\n      IColor netColor = kWarn;\n      if(tx.fail_closed)\n      {\n        net = "ART-NET · FAIL-CLOSED";\n        netColor = kDanger;\n      }\n      else if(physicalAuthority && tx.blackout_latched)\n      {\n        net = "APAGÓN · " + std::to_string(tx.configured_fps) + " Hz";\n        netColor = kDanger;\n      }\n      else if(physicalAuthority)\n      {\n        net = "ART-NET TX · " + std::to_string(tx.configured_fps) + " Hz";\n        netColor = kGood;\n      }\n      else if(mPlug.BackendReady())\n      {\n        net = "ART-NET · LISTA / SIN CARRIER";\n        netColor = kCyan;\n      }\n      Pill(g, IRECT(statusLeft, header.T + 10.0F, statusRight, header.T + 40.0F),\n           net, netColor);\n    }\n''',
    "truthful header Art-Net status",
)

runtime = replace_once(
    runtime,
    '''    IColor rail = kBrand;\n    if(mPlug.TakeRecording() || mPlug.TakeOutputLive()) rail = kDanger;\n    else if(!mPlug.RuntimeHealthy() || mPlug.RenderingOffline()) rail = kDanger;\n    else if(mPlug.TakePlaying()) rail = kGood;\n    else if(mPlug.TakeOutputArmed()) rail = kWarn;\n    g.FillRect(rail, IRECT(footer.L, footer.T, footer.R, footer.T + 2.0F));\n''',
    '''    const auto tx = mPlug.ArtNetOutputStatus();\n    const bool physicalAuthority = tx.enabled || tx.override_enabled;\n    IColor rail = kBrand;\n    if(mPlug.TakeRecording() || mPlug.TakeOutputLive() ||\n       (physicalAuthority && tx.blackout_latched))\n      rail = kDanger;\n    else if(!mPlug.RuntimeHealthy() || mPlug.RenderingOffline() || tx.fail_closed)\n      rail = kDanger;\n    else if(mPlug.TakePlaying()) rail = kGood;\n    else if(physicalAuthority) rail = kWarn;\n    g.FillRect(rail, IRECT(footer.L, footer.T, footer.R, footer.T + 2.0F));\n''',
    "footer authority rail",
)

runtime = replace_once(
    runtime,
    '''    std::string operation;\n    IColor operationColor = kMuted;\n    if(mPlug.TakeRecording()) {\n      operation = "REC · CAPTURANDO";\n      operationColor = kDanger;\n    }\n    else if(mPlug.TakeOutputLive()) {\n      operation = "AL AIRE";\n      operationColor = kDanger;\n    }\n    else if(mPlug.TakePlaying()) {\n      operation = "PLAY · REPRODUCIENDO";\n      operationColor = kGood;\n    }\n    else if(mPlug.TakeOutputArmed()) {\n      operation = "ART-NET ARMADA · CARRIER ACTIVO";\n      operationColor = kWarn;\n    }\n    else if(mPlug.BackendReady()) {\n      operation = "ART-NET LISTA · DESARMADA";\n      operationColor = kGood;\n    }\n    else {\n      operation = "SALIDA NO PREPARADA";\n      operationColor = kWarn;\n    }\n''',
    '''    std::string operation;\n    IColor operationColor = kMuted;\n    if(tx.fail_closed) {\n      operation = "FAIL-CLOSED · REARME MANUAL";\n      operationColor = kDanger;\n    }\n    else if(physicalAuthority && tx.blackout_latched) {\n      operation = "APAGÓN · DMX 0 · CARRIER " +\n          std::to_string(tx.configured_fps) + " Hz";\n      operationColor = kDanger;\n    }\n    else if(mPlug.TakeRecording()) {\n      operation = "REC · CAPTURANDO";\n      operationColor = kDanger;\n    }\n    else if(mPlug.TakeOutputLive()) {\n      operation = "AL AIRE · CARRIER ACTIVO";\n      operationColor = kDanger;\n    }\n    else if(mPlug.TakePlaying()) {\n      operation = "PLAY · REPRODUCIENDO";\n      operationColor = kGood;\n    }\n    else if(physicalAuthority) {\n      operation = "ART-NET ARMADA · CARRIER " +\n          std::to_string(tx.configured_fps) + " Hz";\n      operationColor = kWarn;\n    }\n    else if(mPlug.BackendReady()) {\n      operation = "ART-NET LISTA · SIN CARRIER";\n      operationColor = kCyan;\n    }\n    else {\n      operation = "SALIDA NO PREPARADA";\n      operationColor = kWarn;\n    }\n''',
    "footer operation truth",
)

runtime = replace_once(
    runtime,
    '''    mLiveCloseButton = {};\n    mLivePanicButton = {};\n    mLiveArmButton = {};\n\n    const float transportTop = top;\n    const float transportGap = 7.0F;\n    const float transportW = 92.0F;\n    for(std::size_t index = 0U; index < mLiveTransport.size(); ++index)\n    {\n      const float x = left + static_cast<float>(index) * (transportW + transportGap);\n      mLiveTransport[index] = IRECT(x, transportTop, x + transportW,\n                                   transportTop + 34.0F);\n    }\n\n    const float contentTop = transportTop + 44.0F;\n''',
    '''    const float transportTop = top;\n    const float transportGap = 8.0F;\n    const float transportW = std::clamp(\n        ((right - left) - transportGap * 3.0F) / 4.0F, 118.0F, 176.0F);\n    const float transportTotal = transportW * 4.0F + transportGap * 3.0F;\n    const float transportLeft = left + std::max(0.0F,\n        ((right - left) - transportTotal) * 0.5F);\n    for(std::size_t index = 0U; index < mLiveTransport.size(); ++index)\n    {\n      const float x = transportLeft +\n          static_cast<float>(index) * (transportW + transportGap);\n      mLiveTransport[index] = IRECT(x, transportTop, x + transportW,\n                                   transportTop + 40.0F);\n    }\n\n    const float contentTop = transportTop + 52.0F;\n''',
    "larger centered live transport",
)

runtime = replace_once(
    runtime,
    '''    g.DrawText(IText(8.3F, kMuted, "AeylaUI",\n                     EAlign::Far, EVAlign::Middle),\n               "OPERAR PRIMERO · CONFIGURAR SÓLO CUANDO SEA NECESARIO",\n''',
    '''    g.DrawText(IText(8.3F, kMuted, "AeylaUI",\n                     EAlign::Far, EVAlign::Middle),\n               "4 ACCESOS · DMX / MIDI DENTRO DE EDITAR",\n''',
    "memory header simplification",
)

runtime = replace_once(
    runtime,
    '''  void HandleLiveMouseDown(float x, float y)\n  {\n    BuildLiveLayout();\n    if(Contains(mLiveCloseButton, x, y))\n    {\n      mPlug.SetUiWorkspaceFromUI(0);\n      mLiveOpen = false;\n      mLiveConfigIndex = -1;\n      mDraggingMemory = -1;\n      SetDirty(false);\n      return;\n    }\n    if(Contains(mLivePanicButton, x, y))\n    {\n      mPlug.SetBlackoutFromUI(!mPlug.GlobalBlackout());\n      SetLiveMessage(true, mPlug.GlobalBlackout()\n          ? "APAGÓN ACTIVO · salida desarmada y memorias OFF."\n          : "APAGÓN DESACTIVADO · ARM sigue siendo manual.");\n      SetDirty(false);\n      return;\n    }\n    if(Contains(mLiveArmButton, x, y))\n    {\n      ReportLive(mPlug.ToggleTakeOutputArmFromUI());\n      SetDirty(false);\n      return;\n    }\n\n''',
    '''  void HandleLiveMouseDown(float x, float y)\n  {\n    BuildLiveLayout();\n\n''',
    "remove stale live-local safety controls",
)

runtime = replace_once(
    runtime,
    '''    for(std::size_t index = 0U; index < mLiveMemoryCards.size(); ++index)\n    {\n      if(Contains(mLiveConfigButtons[index], x, y))\n      {\n        mLiveConfigIndex = mLiveConfigIndex == static_cast<int>(index)\n            ? -1 : static_cast<int>(index);\n        mDraggingMemory = -1;\n        SetLiveMessage(true, mLiveConfigIndex == static_cast<int>(index)\n            ? "CONFIGURAR · sólo esta memoria expone DMX/MIDI/modo/fade."\n            : "OPERACIÓN · controles de autoría ocultos.");\n        SetDirty(false);\n        return;\n      }\n\n      const auto view = mPlug.LiveMemoryViewFromUI(index);\n''',
    '''    for(std::size_t index = 0U; index < mLiveMemoryCards.size(); ++index)\n    {\n      const auto view = mPlug.LiveMemoryViewFromUI(index);\n      if(Contains(mLiveConfigButtons[index], x, y))\n      {\n        const bool activeOrFading = view.transitioning ||\n            view.level > 0.005F || view.target_level > 0.005F;\n        if(activeOrFading && mLiveConfigIndex != static_cast<int>(index))\n        {\n          SetLiveMessage(false, view.name +\n              " · llévala a OFF / 0% antes de editar DMX, MIDI, modo o fade.");\n          SetDirty(false);\n          return;\n        }\n        mLiveConfigIndex = mLiveConfigIndex == static_cast<int>(index)\n            ? -1 : static_cast<int>(index);\n        mDraggingMemory = -1;\n        SetLiveMessage(true, mLiveConfigIndex == static_cast<int>(index)\n            ? "EDITAR · DMX / MIDI / modo / fade de esta memoria."\n            : "OPERACIÓN · controles de autoría ocultos.");\n        SetDirty(false);\n        return;\n      }\n\n''',
    "block authoring while live memory active",
)

runtime = replace_once(
    runtime,
    '''      if(view.mode == aeyla::output::LiveMemoryControlMode::toggle &&\n         Contains(mLiveMainControls[index], x, y))\n      {\n        ReportLive(mPlug.ToggleLiveMemoryFromUI(index));\n        SetDirty(false);\n        return;\n      }\n      if(view.mode == aeyla::output::LiveMemoryControlMode::fader &&\n         Contains(mLiveMainControls[index], x, y))\n      {\n        if(!view.configured)\n        {\n          ReportLive(mPlug.SetLiveMemoryLevelFromUI(index, 0.0F));\n          SetDirty(false);\n          return;\n        }\n        mDraggingMemory = static_cast<int>(index);\n        ApplyFaderFromX(index, x);\n        SetDirty(false);\n        return;\n      }\n''',
    '''      if(view.mode == aeyla::output::LiveMemoryControlMode::toggle &&\n         Contains(mLiveMainControls[index], x, y))\n      {\n        if(!view.configured)\n        {\n          mLiveConfigIndex = static_cast<int>(index);\n          mDraggingMemory = -1;\n          SetLiveMessage(false, view.name +\n              " · primero aprende DMX: captura OFF y luego ON.");\n        }\n        else\n          ReportLive(mPlug.ToggleLiveMemoryFromUI(index));\n        SetDirty(false);\n        return;\n      }\n      if(view.mode == aeyla::output::LiveMemoryControlMode::fader &&\n         Contains(mLiveMainControls[index], x, y))\n      {\n        if(!view.configured)\n        {\n          mLiveConfigIndex = static_cast<int>(index);\n          mDraggingMemory = -1;\n          SetLiveMessage(false, view.name +\n              " · primero aprende DMX: captura OFF y luego ON.");\n          SetDirty(false);\n          return;\n        }\n        mDraggingMemory = static_cast<int>(index);\n        ApplyFaderFromX(index, x);\n        SetDirty(false);\n        return;\n      }\n''',
    "unconfigured live control enters edit",
)

runtime = replace_once(
    runtime,
    '''  IRECT mLiveCloseButton{};\n  IRECT mLivePanicButton{};\n  IRECT mLiveArmButton{};\n  std::array<IRECT, 4U> mLiveTransport{};\n''',
    '''  std::array<IRECT, 4U> mLiveTransport{};\n''',
    "remove stale live-local safety rectangles",
)

runtime_path.write_text(runtime, encoding="utf-8")


# -----------------------------------------------------------------------------
# TOMA / MIDI / SISTEMA: consistent semantic colors and physical TX truth.
# -----------------------------------------------------------------------------
main_path = Path("product/AeylaVisualDmx/AeylaMainControl.h")
main = main_path.read_text(encoding="utf-8")

main = replace_once(
    main,
    '''      if(onAir)\n        g.FillRoundRect(kGood,\n''',
    '''      if(onAir)\n        g.FillRoundRect(kDanger,\n''',
    "setlist on-air semantic color",
)

main = replace_once(
    main,
    '''      if(outX > inX)\n        g.DrawRect(kGood, IRECT(inX, mTimeline.T + 1.0F, outX,\n''',
    '''      if(outX > inX)\n        g.DrawRect(kCyan, IRECT(inX, mTimeline.T + 1.0F, outX,\n''',
    "timeline selected range color",
)

main = replace_once(
    main,
    '''    StatusRow(g, mMidiPreparedStatus, "PREPARADA", preparedStatus, kWarn);\n    StatusRow(g, mMidiActiveStatus, "ACTIVA",\n              activeName, active >= 0 ? kGood : kFaint);\n''',
    '''    StatusRow(g, mMidiPreparedStatus, "PREPARADA", preparedStatus, kCyan);\n    StatusRow(g, mMidiActiveStatus, "AL AIRE",\n              activeName, active >= 0 ? kDanger : kFaint);\n''',
    "midi prepared/on-air colors",
)

main = replace_once(
    main,
    '''                  capture.storage_failed ? kAccent :\n                      (capture.signal_present ? kGood : kWarn),\n''',
    '''                  capture.storage_failed ? kDanger :\n                      (capture.signal_present ? kGood : kWarn),\n''',
    "RX failure color",
)

main = replace_once(
    main,
    '''                  output.fail_closed ? kAccent :\n                      (mPlug.BackendReady() ? kGood : kWarn),\n''',
    '''                  output.fail_closed ? kDanger :\n                      (mPlug.BackendReady() ? kGood : kWarn),\n''',
    "TX fail-closed color",
)

main = replace_once(
    main,
    '''    char transmission[220];\n    if(output.running)\n      std::snprintf(transmission, sizeof(transmission),\n                    "%u Hz · %llu paquetes · %llu errores · %llu retrasos",\n                    static_cast<unsigned>(output.configured_fps),\n                    static_cast<unsigned long long>(output.sent_packets),\n                    static_cast<unsigned long long>(output.send_errors),\n                    static_cast<unsigned long long>(output.timing_misses));\n    else\n      std::snprintf(transmission, sizeof(transmission),\n                    "MOTOR DETENIDO · %llu errores acumulados",\n                    static_cast<unsigned long long>(output.send_errors));\n\n    const std::string authority = output.fail_closed\n        ? "FALLO ENCLAVADO · REARME MANUAL"\n        : (mPlug.TakeOutputLive() ? "TOMA AL AIRE" :\n            (mPlug.TakeOutputArmed() ? "ARMADA · ESPERA REPRODUCIR" :\n                                      "DESARMADA"));\n''',
    '''    const bool physicalAuthority = output.enabled || output.override_enabled;\n    char transmission[220];\n    if(physicalAuthority && output.blackout_latched)\n      std::snprintf(transmission, sizeof(transmission),\n                    "APAGÓN · %u Hz · %llu paquetes negros · %llu errores",\n                    static_cast<unsigned>(output.configured_fps),\n                    static_cast<unsigned long long>(output.blackout_packets),\n                    static_cast<unsigned long long>(output.send_errors));\n    else if(physicalAuthority)\n      std::snprintf(transmission, sizeof(transmission),\n                    "CARRIER %u Hz · %llu paquetes · %llu errores · %llu retrasos",\n                    static_cast<unsigned>(output.configured_fps),\n                    static_cast<unsigned long long>(output.sent_packets),\n                    static_cast<unsigned long long>(output.send_errors),\n                    static_cast<unsigned long long>(output.timing_misses));\n    else if(output.running)\n      std::snprintf(transmission, sizeof(transmission),\n                    "MOTOR LISTO · SIN CARRIER · %llu errores acumulados",\n                    static_cast<unsigned long long>(output.send_errors));\n    else\n      std::snprintf(transmission, sizeof(transmission),\n                    "MOTOR DETENIDO · %llu errores acumulados",\n                    static_cast<unsigned long long>(output.send_errors));\n\n    const std::string authority = output.fail_closed\n        ? "FAIL-CLOSED · REARME MANUAL"\n        : (physicalAuthority && output.blackout_latched\n            ? "APAGÓN TOTAL · ARM CONSERVADO"\n            : (mPlug.TakeOutputLive() ? "TOMA AL AIRE" :\n                (physicalAuthority ? "ARMADA · CARRIER ACTIVO" : "DESARMADA")));\n''',
    "system TX/authority truth",
)

main = replace_once(
    main,
    '''    StatusRow(g, transmissionRow, "TRANSMISIÓN", transmission,\n              output.fail_closed || output.consecutive_send_errors > 0U ? kWarn :\n                  (output.running ? kGood : kFaint));\n    StatusRow(g, authorityRow, "AUTORIDAD", authority,\n              output.fail_closed ? kAccent :\n                  (mPlug.TakeOutputLive() ? kAccent :\n                      (mPlug.TakeOutputArmed() ? kWarn : kGood)));\n''',
    '''    StatusRow(g, transmissionRow, "TRANSMISIÓN", transmission,\n              output.fail_closed || output.consecutive_send_errors > 0U\n                  ? kDanger\n                  : (physicalAuthority\n                      ? (output.blackout_latched ? kDanger : kGood)\n                      : (output.running ? kCyan : kFaint)));\n    StatusRow(g, authorityRow, "AUTORIDAD", authority,\n              output.fail_closed || (physicalAuthority && output.blackout_latched)\n                  ? kDanger\n                  : (mPlug.TakeOutputLive() ? kDanger :\n                      (physicalAuthority ? kWarn : kCyan)));\n''',
    "system status row colors",
)

main_path.write_text(main, encoding="utf-8")

print("R10.6 state semantics + live UX hardening applied")
