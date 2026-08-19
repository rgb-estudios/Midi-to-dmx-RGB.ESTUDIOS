#pragma once

#include <cstdint>

namespace aeyla::runtime {

enum class RuntimeSafetyReason : std::uint8_t {
  none,
  operator_disarm,
  project_invalid,
  show_not_ready,
  backend_unavailable,
  project_reload,
  event_overflow,
  host_deactivation,
  offline_render,
  runtime_fault,
  shutdown
};

struct RuntimeSafetyActions {
  bool release_transients{false};
  bool force_haze_zero{false};
  bool publish_safe_frame{false};
};

// Shared product-owned safety state. Both standalone and VST3 adapters call
// this model; no framework/UI/backend handles live here.
class RuntimeSafetyState {
 public:
  [[nodiscard]] bool output_armed() const noexcept { return output_armed_; }
  [[nodiscard]] bool blackout() const noexcept { return blackout_; }
  [[nodiscard]] bool project_valid() const noexcept { return project_valid_; }
  [[nodiscard]] bool backend_ready() const noexcept { return backend_ready_; }
  [[nodiscard]] bool reload_in_progress() const noexcept { return reload_in_progress_; }
  [[nodiscard]] RuntimeSafetyReason reason() const noexcept { return reason_; }

  void set_project_valid(bool valid) noexcept;
  void set_backend_ready(bool ready) noexcept;
  [[nodiscard]] bool request_arm() noexcept;
  void disarm(RuntimeSafetyReason reason = RuntimeSafetyReason::operator_disarm) noexcept;
  void set_blackout(bool enabled) noexcept;

  void begin_project_reload() noexcept;
  void complete_project_reload(bool valid) noexcept;
  void on_event_overflow() noexcept;
  void on_host_deactivation() noexcept;
  void on_shutdown() noexcept;

  [[nodiscard]] RuntimeSafetyActions consume_pending_actions() noexcept;

 private:
  void request_safe_actions(bool publish_safe_frame) noexcept;

  bool output_armed_{false};
  bool blackout_{true};
  bool project_valid_{false};
  bool backend_ready_{false};
  bool reload_in_progress_{false};
  RuntimeSafetyReason reason_{RuntimeSafetyReason::none};
  RuntimeSafetyActions pending_actions_{};
};

}  // namespace aeyla::runtime
