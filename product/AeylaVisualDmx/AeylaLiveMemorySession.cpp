#include "AeylaLiveMemorySession.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <mutex>
#include <string>

namespace aeyla::live_memory_session {
namespace {

struct SlotState {
  output::LiveMemoryDefinition definition{};
  bool configured{false};
  bool learn_pending{false};
  DmxUniverse learn_baseline{};
};

struct SessionState {
  output::ArtNetOutputWorker* output_worker{nullptr};
  capture::ArtNetCaptureWorker* capture_worker{nullptr};
  std::array<SlotState, kOperatorMemoryCount> slots{};
};

std::mutex gMutex;
std::map<const void*, SessionState> gSessions;

constexpr std::array<std::uint32_t, 3U> kFadePresetsMs{
    100U, 1000U, 1500U};
constexpr std::array<const char*, kOperatorMemoryCount> kMemoryIds{
    "front", "haze", "white-base", "fixture-test"};
constexpr std::array<const char*, kOperatorMemoryCount> kMemoryNames{
    "FRONTAL", "HUMO / HAZE", "BASE BLANCA", "TEST LUMINARIAS"};

void initialize_slot(std::size_t index, SlotState& slot) {
  slot.definition.memory_id = kMemoryIds[index];
  slot.definition.name = kMemoryNames[index];
  slot.definition.fade_ms = 1000U;
  slot.definition.mode = index == 1U
      ? output::LiveMemoryControlMode::fader
      : output::LiveMemoryControlMode::toggle;
}

SessionState& ensure_session_locked(const void* owner) {
  auto [iterator, inserted] = gSessions.try_emplace(owner);
  if(inserted) {
    for(std::size_t index = 0U; index < iterator->second.slots.size(); ++index)
      initialize_slot(index, iterator->second.slots[index]);
  }
  return iterator->second;
}

ActionResult invalid_index() {
  return {false, "Memoria EN VIVO fuera de rango"};
}

std::string fade_label(std::uint32_t fade_ms) {
  if(fade_ms == 100U) return "0.1 s";
  if(fade_ms == 1000U) return "1.0 s";
  if(fade_ms == 1500U) return "1.5 s";
  return std::to_string(fade_ms) + " ms";
}

bool reconfigure_locked(SessionState& session,
                        std::size_t index,
                        std::string& error) {
  auto& slot = session.slots[index];
  if(!slot.configured || session.output_worker == nullptr)
    return false;
  return session.output_worker->configure_live_memory(
      index, slot.definition, error);
}

}  // namespace

void register_runtime(const void* owner,
                      output::ArtNetOutputWorker* output_worker,
                      capture::ArtNetCaptureWorker* capture_worker) {
  if(owner == nullptr) return;
  const std::scoped_lock lock(gMutex);
  auto& session = ensure_session_locked(owner);
  session.output_worker = output_worker;
  session.capture_worker = capture_worker;

  // Re-registering after a socket/runtime restart preserves learned definitions
  // for this plugin lifetime but always restores them at level OFF.
  if(session.output_worker != nullptr) {
    session.output_worker->reset_live_memories();
    for(std::size_t index = 0U; index < session.slots.size(); ++index) {
      if(!session.slots[index].configured) continue;
      std::string ignored;
      (void)session.output_worker->configure_live_memory(
          index, session.slots[index].definition, ignored);
    }
  }
}

void clear(const void* owner) noexcept {
  if(owner == nullptr) return;
  const std::scoped_lock lock(gMutex);
  const auto iterator = gSessions.find(owner);
  if(iterator == gSessions.end()) return;
  if(iterator->second.output_worker != nullptr)
    iterator->second.output_worker->reset_live_memories();
  gSessions.erase(iterator);
}

MemoryView view(const void* owner, std::size_t index) {
  MemoryView result;
  if(owner == nullptr || index >= kOperatorMemoryCount)
    return result;

  const std::scoped_lock lock(gMutex);
  auto& session = ensure_session_locked(owner);
  const auto& slot = session.slots[index];
  result.configured = slot.configured;
  result.learning = slot.learn_pending;
  result.name = slot.definition.name;
  result.mode = slot.definition.mode;
  result.fade_ms = slot.definition.fade_ms;
  result.channel_count = slot.definition.mask.count();

  if(session.output_worker != nullptr && slot.configured) {
    const auto runtime = session.output_worker->live_memory_snapshot(index);
    result.level = runtime.level;
    result.target_level = runtime.target_level;
    result.transitioning = runtime.transitioning;
  }
  return result;
}

ActionResult learn_from_avolites(const void* owner, std::size_t index) {
  if(owner == nullptr || index >= kOperatorMemoryCount)
    return invalid_index();

  const std::scoped_lock lock(gMutex);
  auto& session = ensure_session_locked(owner);
  if(session.capture_worker == nullptr || session.output_worker == nullptr)
    return {false, "El runtime de red EN VIVO no está disponible"};

  const auto stats = session.capture_worker->stats();
  if(!stats.running)
    return {false, "Art-Net RX no está activo · revisa RED / SALIDA"};
  if(!stats.signal_present)
    return {false, "No hay señal Art-Net de Avolites para aprender la memoria"};

  DmxUniverse rx{};
  if(!session.capture_worker->latest_frame(rx))
    return {false, "Avolites aún no entregó un frame DMX completo"};

  auto& slot = session.slots[index];
  if(!slot.learn_pending) {
    slot.learn_baseline = rx;
    slot.learn_pending = true;
    return {true,
            slot.definition.name +
                " · PASO 1/2 listo: deja esta memoria OFF como referencia, "
                "actívala en Avolites y presiona APRENDER nuevamente"};
  }

  output::LiveMemoryMask mask;
  for(std::size_t channel = 0U; channel < rx.size(); ++channel) {
    if(rx[channel] != slot.learn_baseline[channel])
      mask.set(channel);
  }
  if(!mask.any()) {
    return {false,
            slot.definition.name +
                " · no se detectaron cambios DMX. Mantengo el PASO 1; "
                "activa la memoria en Avolites y vuelve a presionar APRENDER"};
  }

  auto learned = slot.definition;
  learned.target = rx;
  learned.mask = mask;
  std::string error;
  if(!session.output_worker->configure_live_memory(index, learned, error))
    return {false, "No se pudo validar la memoria aprendida · " + error};

  slot.definition = learned;
  slot.configured = true;
  slot.learn_pending = false;
  const auto changed = mask.count();
  return {true,
          slot.definition.name + " APRENDIDA · " +
              std::to_string(changed) + " canales · fade " +
              fade_label(slot.definition.fade_ms) +
              " · estado inicial OFF"};
}

ActionResult cancel_learn(const void* owner, std::size_t index) {
  if(owner == nullptr || index >= kOperatorMemoryCount)
    return invalid_index();
  const std::scoped_lock lock(gMutex);
  auto& session = ensure_session_locked(owner);
  auto& slot = session.slots[index];
  slot.learn_pending = false;
  return {true, slot.definition.name + " · aprendizaje cancelado"};
}

ActionResult toggle(const void* owner, std::size_t index) {
  if(owner == nullptr || index >= kOperatorMemoryCount)
    return invalid_index();
  const std::scoped_lock lock(gMutex);
  auto& session = ensure_session_locked(owner);
  auto& slot = session.slots[index];
  if(!slot.configured)
    return {false, slot.definition.name + " · primero APRENDER desde Avolites"};
  if(session.output_worker == nullptr)
    return {false, "La salida Art-Net EN VIVO no está disponible"};
  if(!session.output_worker->toggle_live_memory(index))
    return {false,
            slot.definition.name +
                " · salida física no armada o bloqueada por seguridad"};
  const auto runtime = session.output_worker->live_memory_snapshot(index);
  return {true,
          slot.definition.name +
              (runtime.target_level > 0.5F ? " · ENTRANDO" : " · SALIENDO") +
              " · " + fade_label(slot.definition.fade_ms)};
}

ActionResult set_fader_level(const void* owner,
                             std::size_t index,
                             float level) {
  if(owner == nullptr || index >= kOperatorMemoryCount)
    return invalid_index();
  const std::scoped_lock lock(gMutex);
  auto& session = ensure_session_locked(owner);
  auto& slot = session.slots[index];
  if(!slot.configured)
    return {false, slot.definition.name + " · primero APRENDER desde Avolites"};
  if(slot.definition.mode != output::LiveMemoryControlMode::fader)
    return {false, slot.definition.name + " está configurada como BOTÓN / TOGGLE"};
  if(session.output_worker == nullptr)
    return {false, "La salida Art-Net EN VIVO no está disponible"};

  const float normalized = std::isfinite(level)
      ? std::clamp(level, 0.0F, 1.0F)
      : 0.0F;
  if(!session.output_worker->set_live_memory_level(index, normalized, true))
    return {false,
            slot.definition.name +
                " · salida física no armada o bloqueada por seguridad"};
  return {true,
          slot.definition.name + " · " +
              std::to_string(static_cast<int>(std::lround(normalized * 100.0F))) +
              "%"};
}

ActionResult cycle_fade(const void* owner,
                        std::size_t index,
                        int direction) {
  if(owner == nullptr || index >= kOperatorMemoryCount)
    return invalid_index();
  if(direction == 0)
    return {false, "Dirección de cambio de fade inválida"};

  const std::scoped_lock lock(gMutex);
  auto& session = ensure_session_locked(owner);
  auto& slot = session.slots[index];
  const auto current = std::find(kFadePresetsMs.begin(),
                                 kFadePresetsMs.end(),
                                 slot.definition.fade_ms);
  std::size_t position = current == kFadePresetsMs.end()
      ? 1U
      : static_cast<std::size_t>(
            std::distance(kFadePresetsMs.begin(), current));
  if(direction > 0)
    position = (position + 1U) % kFadePresetsMs.size();
  else
    position = position == 0U ? kFadePresetsMs.size() - 1U : position - 1U;
  slot.definition.fade_ms = kFadePresetsMs[position];

  std::string error;
  if(slot.configured && !reconfigure_locked(session, index, error))
    return {false, "No se pudo aplicar el nuevo fade · " + error};
  return {true,
          slot.definition.name + " · FADE " +
              fade_label(slot.definition.fade_ms) + " · nivel reiniciado OFF"};
}

ActionResult toggle_mode(const void* owner, std::size_t index) {
  if(owner == nullptr || index >= kOperatorMemoryCount)
    return invalid_index();
  const std::scoped_lock lock(gMutex);
  auto& session = ensure_session_locked(owner);
  auto& slot = session.slots[index];
  slot.definition.mode =
      slot.definition.mode == output::LiveMemoryControlMode::toggle
          ? output::LiveMemoryControlMode::fader
          : output::LiveMemoryControlMode::toggle;

  std::string error;
  if(slot.configured && !reconfigure_locked(session, index, error))
    return {false, "No se pudo aplicar el modo de memoria · " + error};
  return {true,
          slot.definition.name + " · " +
              (slot.definition.mode == output::LiveMemoryControlMode::toggle
                   ? "BOTÓN / TOGGLE"
                   : "FADER") +
              " · nivel reiniciado OFF"};
}

void reset_levels(const void* owner) noexcept {
  if(owner == nullptr) return;
  const std::scoped_lock lock(gMutex);
  const auto iterator = gSessions.find(owner);
  if(iterator == gSessions.end() || iterator->second.output_worker == nullptr)
    return;
  iterator->second.output_worker->reset_live_memories();
}

}  // namespace aeyla::live_memory_session
