#include "output/dmx_live_memory.h"

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

namespace aeyla::output {
namespace {

constexpr float kLevelEpsilon = 0.0001F;
constexpr std::size_t kMaximumMemoryIdBytes = 64U;
constexpr std::size_t kMaximumMemoryNameBytes = 96U;

std::uint8_t mix_slot(std::uint8_t base,
                      std::uint8_t target,
                      float level) noexcept {
  const float mixed = static_cast<float>(base) +
      (static_cast<float>(target) - static_cast<float>(base)) * level;
  const long rounded = std::lround(mixed);
  return static_cast<std::uint8_t>(
      std::clamp(rounded, 0L, 255L));
}

}  // namespace

bool validate_live_memory_definition(
    const LiveMemoryDefinition& definition,
    std::string& error_message) noexcept {
  error_message.clear();
  if(definition.memory_id.empty()) {
    error_message = "La memoria EN VIVO requiere un identificador";
    return false;
  }
  if(definition.memory_id.size() > kMaximumMemoryIdBytes) {
    error_message = "El identificador de la memoria EN VIVO es demasiado largo";
    return false;
  }
  if(definition.name.empty()) {
    error_message = "La memoria EN VIVO requiere un nombre";
    return false;
  }
  if(definition.name.size() > kMaximumMemoryNameBytes) {
    error_message = "El nombre de la memoria EN VIVO es demasiado largo";
    return false;
  }
  if(!definition.mask.any()) {
    error_message = "La memoria EN VIVO no controla ningún canal DMX";
    return false;
  }
  if(definition.fade_ms > kMaximumLiveMemoryFadeMs) {
    error_message = "El fundido de la memoria EN VIVO supera 60 segundos";
    return false;
  }
  return true;
}

float DmxLiveMemoryEngine::clamp_level(float level) noexcept {
  if(!std::isfinite(level)) return 0.0F;
  return std::clamp(level, 0.0F, 1.0F);
}

void DmxLiveMemoryEngine::update_level(RuntimeMemory& memory,
                                       TimePoint now) noexcept {
  if(!memory.configured) return;
  if(memory.transition_duration.count() <= 0) {
    memory.current_level = memory.target_level;
    memory.start_level = memory.target_level;
    return;
  }

  const auto elapsed = now - memory.transition_started;
  if(elapsed <= Clock::duration::zero()) {
    memory.current_level = memory.start_level;
    return;
  }
  if(elapsed >= memory.transition_duration) {
    memory.current_level = memory.target_level;
    memory.start_level = memory.target_level;
    memory.transition_duration = std::chrono::milliseconds{0};
    return;
  }

  const double fraction =
      std::chrono::duration<double>(elapsed).count() /
      std::chrono::duration<double>(memory.transition_duration).count();
  const float amount = static_cast<float>(std::clamp(fraction, 0.0, 1.0));
  memory.current_level = memory.start_level +
      (memory.target_level - memory.start_level) * amount;
}

void DmxLiveMemoryEngine::begin_transition(RuntimeMemory& memory,
                                           float target,
                                           TimePoint now) noexcept {
  update_level(memory, now);
  const float next = clamp_level(target);
  memory.start_level = memory.current_level;
  memory.target_level = next;
  memory.transition_started = now;
  memory.transition_duration =
      std::chrono::milliseconds{memory.definition.fade_ms};
  if(memory.transition_duration.count() == 0)
    memory.current_level = memory.target_level;

  // LTP order changes whenever an operator/MIDI source makes a positive move.
  // A fade-out keeps its previous serial so the memory can reveal lower layers
  // continuously instead of jumping priority on release.
  if(next > kLevelEpsilon)
    memory.activation_serial = ++next_activation_serial_;
}

bool DmxLiveMemoryEngine::configure(
    std::size_t index,
    const LiveMemoryDefinition& definition,
    std::string& error_message) {
  if(index >= memories_.size()) {
    error_message = "Índice de memoria EN VIVO fuera de rango";
    return false;
  }
  if(!validate_live_memory_definition(definition, error_message))
    return false;

  const std::scoped_lock lock(mutex_);
  auto& memory = memories_[index];
  memory = RuntimeMemory{};
  memory.configured = true;
  memory.definition = definition;
  error_message.clear();
  return true;
}

void DmxLiveMemoryEngine::clear(std::size_t index) noexcept {
  if(index >= memories_.size()) return;
  const std::scoped_lock lock(mutex_);
  memories_[index] = RuntimeMemory{};
}

void DmxLiveMemoryEngine::clear_all() noexcept {
  const std::scoped_lock lock(mutex_);
  for(auto& memory : memories_)
    memory = RuntimeMemory{};
  next_activation_serial_ = 0U;
}

bool DmxLiveMemoryEngine::toggle(std::size_t index, TimePoint now) noexcept {
  if(index >= memories_.size()) return false;
  const std::scoped_lock lock(mutex_);
  auto& memory = memories_[index];
  if(!memory.configured) return false;
  update_level(memory, now);
  const float next = memory.target_level > kLevelEpsilon ? 0.0F : 1.0F;
  begin_transition(memory, next, now);
  return true;
}

bool DmxLiveMemoryEngine::set_target_level(std::size_t index,
                                           float level,
                                           TimePoint now) noexcept {
  if(index >= memories_.size()) return false;
  const std::scoped_lock lock(mutex_);
  auto& memory = memories_[index];
  if(!memory.configured) return false;
  begin_transition(memory, level, now);
  return true;
}

bool DmxLiveMemoryEngine::set_direct_level(std::size_t index,
                                           float level,
                                           TimePoint now) noexcept {
  if(index >= memories_.size()) return false;
  const std::scoped_lock lock(mutex_);
  auto& memory = memories_[index];
  if(!memory.configured) return false;
  update_level(memory, now);
  const float next = clamp_level(level);
  memory.start_level = next;
  memory.current_level = next;
  memory.target_level = next;
  memory.transition_started = now;
  memory.transition_duration = std::chrono::milliseconds{0};
  if(next > kLevelEpsilon)
    memory.activation_serial = ++next_activation_serial_;
  return true;
}

void DmxLiveMemoryEngine::reset_levels() noexcept {
  const std::scoped_lock lock(mutex_);
  for(auto& memory : memories_) {
    if(!memory.configured) continue;
    memory.start_level = 0.0F;
    memory.current_level = 0.0F;
    memory.target_level = 0.0F;
    memory.transition_duration = std::chrono::milliseconds{0};
    memory.activation_serial = 0U;
  }
  next_activation_serial_ = 0U;
}

DmxUniverse DmxLiveMemoryEngine::compose(const DmxUniverse& base,
                                         TimePoint now) noexcept {
  const std::scoped_lock lock(mutex_);
  DmxUniverse output = base;
  std::vector<std::pair<std::uint64_t, std::size_t>> active;
  active.reserve(memories_.size());

  for(std::size_t index = 0U; index < memories_.size(); ++index) {
    auto& memory = memories_[index];
    if(!memory.configured) continue;
    update_level(memory, now);
    if(memory.current_level <= kLevelEpsilon) continue;
    active.emplace_back(memory.activation_serial, index);
  }

  std::sort(active.begin(), active.end(),
            [](const auto& left, const auto& right) {
              if(left.first != right.first) return left.first < right.first;
              return left.second < right.second;
            });

  for(const auto& [serial, index] : active) {
    (void)serial;
    const auto& memory = memories_[index];
    const float level = clamp_level(memory.current_level);
    for(std::size_t channel = 0U; channel < output.size(); ++channel) {
      if(!memory.definition.mask.test(channel)) continue;
      output[channel] = mix_slot(output[channel],
                                 memory.definition.target[channel],
                                 level);
    }
  }
  return output;
}

LiveMemorySnapshot DmxLiveMemoryEngine::snapshot(std::size_t index,
                                                 TimePoint now) noexcept {
  LiveMemorySnapshot result;
  if(index >= memories_.size()) return result;
  const std::scoped_lock lock(mutex_);
  auto& memory = memories_[index];
  if(!memory.configured) return result;
  update_level(memory, now);
  result.configured = true;
  result.memory_id = memory.definition.memory_id;
  result.name = memory.definition.name;
  result.mode = memory.definition.mode;
  result.fade_ms = memory.definition.fade_ms;
  result.channel_count = memory.definition.mask.count();
  result.level = clamp_level(memory.current_level);
  result.target_level = clamp_level(memory.target_level);
  result.transitioning = memory.transition_duration.count() > 0;
  result.activation_serial = memory.activation_serial;
  return result;
}

}  // namespace aeyla::output
