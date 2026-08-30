#pragma once

#include "capture/artnet_capture_worker.h"
#include "output/artnet_output_worker.h"

#include <cstddef>
#include <cstdint>
#include <string>

namespace aeyla::live_memory_session {

inline constexpr std::size_t kOperatorMemoryCount = 4U;

struct ActionResult {
  bool succeeded{false};
  std::string message;
};

struct MemoryView {
  bool configured{false};
  bool learning{false};
  std::string name;
  output::LiveMemoryControlMode mode{output::LiveMemoryControlMode::toggle};
  std::uint32_t fade_ms{1000U};
  std::size_t channel_count{0U};
  float level{0.0F};
  float target_level{0.0F};
  bool transitioning{false};
};

// Session data is scoped by the concrete plugin instance. Nothing is shared
// between two DAW instances even if they point to the same Art-Net network.
void register_runtime(const void* owner,
                      output::ArtNetOutputWorker* output_worker,
                      capture::ArtNetCaptureWorker* capture_worker);
void clear(const void* owner) noexcept;

[[nodiscard]] MemoryView view(const void* owner, std::size_t index);

// Two-step learn from Avolites:
// 1) first press while the Avolites memory is OFF captures the RX baseline;
// 2) second press while the Avolites memory is ON captures only the slots that
//    actually changed. This avoids treating unrelated zero-valued slots as part
//    of FRONTAL/HUMO/BASE/TEST.
[[nodiscard]] ActionResult learn_from_avolites(const void* owner,
                                               std::size_t index);
[[nodiscard]] ActionResult cancel_learn(const void* owner,
                                        std::size_t index);

[[nodiscard]] ActionResult toggle(const void* owner, std::size_t index);
[[nodiscard]] ActionResult set_fader_level(const void* owner,
                                           std::size_t index,
                                           float level);
[[nodiscard]] ActionResult cycle_fade(const void* owner,
                                      std::size_t index,
                                      int direction);
[[nodiscard]] ActionResult toggle_mode(const void* owner,
                                       std::size_t index);
void reset_levels(const void* owner) noexcept;

}  // namespace aeyla::live_memory_session
