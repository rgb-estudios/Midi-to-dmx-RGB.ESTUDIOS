from pathlib import Path


def replace_exact(path: str, old: str, new: str) -> None:
    p = Path(path)
    text = p.read_text(encoding='utf-8')
    count = text.count(old)
    if count != 1:
        raise SystemExit(f'{path}: expected exactly one match, found {count}')
    p.write_text(text.replace(old, new), encoding='utf-8')

# Worker public contract.
replace_exact(
    'src/output/artnet_output_worker.h',
    '''  bool enabled{false};\n  bool override_enabled{false};\n  bool fail_closed{false};\n''',
    '''  bool enabled{false};\n  bool override_enabled{false};\n  bool blackout_latched{false};\n  bool fail_closed{false};\n''')

replace_exact(
    'src/output/artnet_output_worker.h',
    '''// La reproducción tiene prioridad mientras está habilitada. Si ninguna\n// autoridad queda activa se transmite una ráfaga corta de BLACKOUT. Tres\n// errores de envío consecutivos provocan fail-closed: ambas autoridades quedan\n// deshabilitadas, las memorias vuelven a OFF y el rearme debe ser explícito.\n''',
    '''// APAGÓN TOTAL es una máscara física latched de prioridad absoluta: mientras\n// exista autoridad armada transmite DMX 0 continuamente a 44 Hz por encima de\n// Take y memorias, sin retirar ARM. DESARMAR sí retira la autoridad y emite una\n// ráfaga corta final de BLACKOUT. Tres errores consecutivos provocan fail-closed:\n// autoridades y APAGÓN quedan retirados, memorias OFF y rearme explícito.\n''')

replace_exact(
    'src/output/artnet_output_worker.h',
    '''  void publish_override(const DmxUniverse& universe, std::uint64_t generation);\n  void set_override_enabled(bool enabled) noexcept;\n  [[nodiscard]] bool override_enabled() const noexcept;\n\n  // EN VIVO: configuración y control de la capa masked/LTP. Estas funciones\n''',
    '''  void publish_override(const DmxUniverse& universe, std::uint64_t generation);\n  void set_override_enabled(bool enabled) noexcept;\n  [[nodiscard]] bool override_enabled() const noexcept;\n\n  // APAGÓN TOTAL: máscara física de máxima prioridad. No desarma la autoridad;\n  // fuerza DMX 0 continuo mientras alguna autoridad esté armada.\n  void set_blackout_latched(bool enabled) noexcept;\n  [[nodiscard]] bool blackout_latched() const noexcept;\n\n  // EN VIVO: configuración y control de la capa masked/LTP. Estas funciones\n''')

# Worker implementation.
replace_exact(
    'src/output/artnet_output_worker.cpp',
    '''    enabled_.store(false, std::memory_order_release);\n    override_enabled_.store(false, std::memory_order_release);\n    fail_closed_.store(false, std::memory_order_release);\n''',
    '''    enabled_.store(false, std::memory_order_release);\n    override_enabled_.store(false, std::memory_order_release);\n    blackout_latched_.store(false, std::memory_order_release);\n    fail_closed_.store(false, std::memory_order_release);\n''')

replace_exact(
    'src/output/artnet_output_worker.cpp',
    '''  bool override_enabled() const noexcept {\n    return override_enabled_.load(std::memory_order_acquire);\n  }\n\n  bool configure_live_memory(std::size_t index,\n''',
    '''  bool override_enabled() const noexcept {\n    return override_enabled_.load(std::memory_order_acquire);\n  }\n\n  void set_blackout_latched(bool enabled) noexcept {\n    blackout_latched_.store(enabled, std::memory_order_release);\n    if(enabled)\n      live_memories_.reset_levels();\n    wake_.notify_all();\n  }\n\n  bool blackout_latched() const noexcept {\n    return blackout_latched_.load(std::memory_order_acquire);\n  }\n\n  bool configure_live_memory(std::size_t index,\n''')

replace_exact(
    'src/output/artnet_output_worker.cpp',
    '''  bool live_authority_active() const noexcept {\n    return override_enabled_.load(std::memory_order_acquire) ||\n           enabled_.load(std::memory_order_acquire);\n  }\n''',
    '''  bool live_authority_active() const noexcept {\n    if(blackout_latched_.load(std::memory_order_acquire)) return false;\n    return override_enabled_.load(std::memory_order_acquire) ||\n           enabled_.load(std::memory_order_acquire);\n  }\n''')

replace_exact(
    'src/output/artnet_output_worker.cpp',
    '''    result.enabled = enabled_.load(std::memory_order_acquire);\n    result.override_enabled = override_enabled_.load(std::memory_order_acquire);\n    result.fail_closed = fail_closed_.load(std::memory_order_acquire);\n''',
    '''    result.enabled = enabled_.load(std::memory_order_acquire);\n    result.override_enabled = override_enabled_.load(std::memory_order_acquire);\n    result.blackout_latched = blackout_latched_.load(std::memory_order_acquire);\n    result.fail_closed = fail_closed_.load(std::memory_order_acquire);\n''')

replace_exact(
    'src/output/artnet_output_worker.cpp',
    '''  void enter_fail_closed() noexcept {\n    const bool already = fail_closed_.exchange(true, std::memory_order_acq_rel);\n    enabled_.store(false, std::memory_order_release);\n    override_enabled_.store(false, std::memory_order_release);\n    live_memories_.reset_levels();\n''',
    '''  void enter_fail_closed() noexcept {\n    const bool already = fail_closed_.exchange(true, std::memory_order_acq_rel);\n    enabled_.store(false, std::memory_order_release);\n    override_enabled_.store(false, std::memory_order_release);\n    blackout_latched_.store(false, std::memory_order_release);\n    live_memories_.reset_levels();\n''')

replace_exact(
    'src/output/artnet_output_worker.cpp',
    '''      const auto remaining =\n          blackout_burst_remaining_.load(std::memory_order_acquire);\n      if(remaining > 0U) {\n        (void)transmit(blackout, 0U, true);\n        consume_one_blackout_frame();\n      } else if(override_enabled_.load(std::memory_order_acquire)) {\n''',
    '''      const bool base_enabled = enabled_.load(std::memory_order_acquire);\n      const bool override_enabled =\n          override_enabled_.load(std::memory_order_acquire);\n      const bool blackout_latched =\n          blackout_latched_.load(std::memory_order_acquire);\n      const auto remaining =\n          blackout_burst_remaining_.load(std::memory_order_acquire);\n      if(blackout_latched && (base_enabled || override_enabled)) {\n        // Absolute physical priority: no Take or live-memory frame can leak\n        // through while APAGÓN TOTAL is latched. ARM remains untouched.\n        (void)transmit(blackout, 0U, true);\n      } else if(remaining > 0U) {\n        (void)transmit(blackout, 0U, true);\n        consume_one_blackout_frame();\n      } else if(override_enabled) {\n''')

replace_exact(
    'src/output/artnet_output_worker.cpp',
    '''  std::atomic<bool> enabled_{false};\n  std::atomic<bool> override_enabled_{false};\n  std::atomic<bool> fail_closed_{false};\n''',
    '''  std::atomic<bool> enabled_{false};\n  std::atomic<bool> override_enabled_{false};\n  std::atomic<bool> blackout_latched_{false};\n  std::atomic<bool> fail_closed_{false};\n''')

replace_exact(
    'src/output/artnet_output_worker.cpp',
    '''bool ArtNetOutputWorker::override_enabled() const noexcept {\n  return impl_->override_enabled();\n}\n\nbool ArtNetOutputWorker::configure_live_memory(\n''',
    '''bool ArtNetOutputWorker::override_enabled() const noexcept {\n  return impl_->override_enabled();\n}\n\nvoid ArtNetOutputWorker::set_blackout_latched(bool enabled) noexcept {\n  impl_->set_blackout_latched(enabled);\n}\n\nbool ArtNetOutputWorker::blackout_latched() const noexcept {\n  return impl_->blackout_latched();\n}\n\nbool ArtNetOutputWorker::configure_live_memory(\n''')

# Product-facing blackout must no longer disarm the carrier/scheduler.
replace_exact(
    'product/AeylaVisualDmx/AeylaShowPlayerIntegration.cpp',
    '''void AeylaVisualDmx::SetBlackoutFromUI(bool enabled)\n{\n  if(enabled)\n  {\n    mTakeScheduler.disarm();\n    mActiveTakeSongIndex.store(-1, std::memory_order_release);\n  }\n\n  GetParam(kParamBlackout)->Set(enabled ? 1.0 : 0.0);\n  mParamBlackout.store(enabled, std::memory_order_release);\n\n  const std::scoped_lock lock(mModelMutex);\n  mModel.release_transients();\n  if(enabled)\n    mModel.disarm(aeyla::runtime::RuntimeSafetyReason::operator_disarm);\n  mModel.set_blackout(enabled);\n  SyncSnapshotToAtomicsLocked();\n  PublishOutputFrameLocked(\n      mRenderingOffline.load(std::memory_order_acquire));\n}\n''',
    '''void AeylaVisualDmx::SetBlackoutFromUI(bool enabled)\n{\n  GetParam(kParamBlackout)->Set(enabled ? 1.0 : 0.0);\n  mParamBlackout.store(enabled, std::memory_order_release);\n\n  const std::scoped_lock lock(mModelMutex);\n  if(enabled)\n    mModel.release_transients();\n  mModel.set_blackout(enabled);\n  SyncSnapshotToAtomicsLocked();\n\n  // R10.5: APAGÓN TOTAL is a physical mask, not DISARM. The single Art-Net\n  // worker keeps its lease/carrier and transmits zero at 44 Hz above Take and\n  // EN VIVO. Releasing APAGÓN reveals the underlying current state without a\n  // second ARM action.\n  mArtNetOutput.set_blackout_latched(enabled);\n  PublishOutputFrameLocked(\n      mRenderingOffline.load(std::memory_order_acquire));\n}\n''')

replace_exact(
    'product/AeylaVisualDmx/AeylaVisualDmx.h',
    '''  // Operator-facing BLACKOUT path. Enabling blackout performs one deterministic\n  // disarm boundary for both semantic and Take authority, then latches blackout.\n  // Disabling blackout never auto-arms output.\n  void SetBlackoutFromUI(bool enabled);\n''',
    '''  // Operator-facing APAGÓN TOTAL. This is a physical zero-DMX mask with\n  // absolute priority; it preserves ARM/carrier. DESARMAR is a separate action.\n  // Releasing APAGÓN reveals the underlying HOLD/Take state without auto-arming.\n  void SetBlackoutFromUI(bool enabled);\n''')

# Keep the worker latch synchronized with semantic blackout each runtime tick.
replace_exact(
    'product/AeylaVisualDmx/AeylaVisualDmx.cpp',
    '''  mArtNetOutput.publish_latest(snapshot.dmx, snapshot.generation);\n  mArtNetOutput.set_enabled(snapshot.output_armed && !renderingOffline);\n\n  const auto stats = mArtNetOutput.stats();\n''',
    '''  mArtNetOutput.publish_latest(snapshot.dmx, snapshot.generation);\n  mArtNetOutput.set_blackout_latched(snapshot.global_blackout && !renderingOffline);\n  mArtNetOutput.set_enabled(snapshot.output_armed && !renderingOffline);\n\n  const auto stats = mArtNetOutput.stats();\n''')

# Physical regression: continuous blackout, no loss of ARM, clean release.
replace_exact(
    'tests/test_artnet_output_worker.cpp',
    '''  worker.set_enabled(false);\n  check(receive_until_blackout(receiver.socket, 6),\n        "disabling output must emit a zero-DMX ArtDMX safety packet");\n''',
    '''  // R10.5 APAGÓN TOTAL: force continuous zero DMX without dropping base\n  // authority. Clear must reveal the latest desired frame without re-ARM.\n  worker.set_blackout_latched(true);\n  check(worker.stats().enabled && worker.stats().blackout_latched,\n        "latched blackout must preserve armed base authority");\n  int blackout_frames = 0;\n  for(int attempt = 0; attempt < 8; ++attempt) {\n    const auto packet = receive_packet(receiver.socket);\n    if(is_artdmx(packet) && payload_is_zero(packet))\n      ++blackout_frames;\n    if(blackout_frames >= 4) break;\n  }\n  check(blackout_frames >= 4,\n        "latched blackout must transmit continuous zero DMX, not one burst");\n\n  worker.set_blackout_latched(false);\n  bool resumed_after_blackout = false;\n  for(int attempt = 0; attempt < 6; ++attempt) {\n    const auto packet = receive_packet(receiver.socket);\n    if(is_artdmx(packet) && packet.size() > 19U &&\n       packet[18] == 101U && packet[19] == 77U) {\n      resumed_after_blackout = true;\n      break;\n    }\n  }\n  check(resumed_after_blackout && worker.stats().enabled &&\n            !worker.stats().blackout_latched,\n        "releasing blackout must reveal latest frame without a second ARM");\n\n  worker.set_enabled(false);\n  check(receive_until_blackout(receiver.socket, 6),\n        "disabling output must emit a zero-DMX ArtDMX safety packet");\n''')

print('R10.5 blackout contract patch applied')
