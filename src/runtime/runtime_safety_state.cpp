#include "runtime/runtime_safety_state.h"

namespace aeyla::runtime {

void RuntimeSafetyState::request_safe_actions(bool publish_safe_frame) noexcept {
  pending_actions_.release_transients = true;
  pending_actions_.force_haze_zero = true;
  pending_actions_.publish_safe_frame =
      pending_actions_.publish_safe_frame || publish_safe_frame;
}

void RuntimeSafetyState::set_project_valid(bool valid) noexcept {
  project_valid_ = valid;
  if (!valid) {
    output_armed_ = false;
    blackout_ = true;
    reason_ = RuntimeSafetyReason::project_invalid;
    request_safe_actions(true);
  }
}

void RuntimeSafetyState::set_backend_ready(bool ready) noexcept {
  backend_ready_ = ready;
  if (!ready) {
    output_armed_ = false;
    blackout_ = true;
    reason_ = RuntimeSafetyReason::backend_unavailable;
    request_safe_actions(true);
  }
}

bool RuntimeSafetyState::request_arm() noexcept {
  if (!project_valid_) {
    reason_ = RuntimeSafetyReason::project_invalid;
    return false;
  }
  if (!backend_ready_) {
    reason_ = RuntimeSafetyReason::backend_unavailable;
    return false;
  }
  if (reload_in_progress_) {
    reason_ = RuntimeSafetyReason::project_reload;
    return false;
  }

  output_armed_ = true;
  reason_ = RuntimeSafetyReason::none;
  return true;
}

void RuntimeSafetyState::disarm(RuntimeSafetyReason reason) noexcept {
  output_armed_ = false;
  reason_ = reason;
  request_safe_actions(true);
}

void RuntimeSafetyState::set_blackout(bool enabled) noexcept {
  blackout_ = enabled;
  if (enabled) request_safe_actions(true);
}

void RuntimeSafetyState::begin_project_reload() noexcept {
  reload_in_progress_ = true;
  project_valid_ = false;
  output_armed_ = false;
  blackout_ = true;
  reason_ = RuntimeSafetyReason::project_reload;
  request_safe_actions(true);
}

void RuntimeSafetyState::complete_project_reload(bool valid) noexcept {
  reload_in_progress_ = false;
  project_valid_ = valid;
  output_armed_ = false;
  blackout_ = true;
  reason_ = valid ? RuntimeSafetyReason::operator_disarm
                  : RuntimeSafetyReason::project_invalid;
  request_safe_actions(true);
}

void RuntimeSafetyState::on_event_overflow() noexcept {
  reason_ = RuntimeSafetyReason::event_overflow;
  request_safe_actions(false);
}

void RuntimeSafetyState::on_host_deactivation() noexcept {
  output_armed_ = false;
  blackout_ = true;
  reason_ = RuntimeSafetyReason::host_deactivation;
  request_safe_actions(true);
}

void RuntimeSafetyState::on_shutdown() noexcept {
  output_armed_ = false;
  blackout_ = true;
  reason_ = RuntimeSafetyReason::shutdown;
  request_safe_actions(true);
}

RuntimeSafetyActions RuntimeSafetyState::consume_pending_actions() noexcept {
  const auto result = pending_actions_;
  pending_actions_ = {};
  return result;
}

}  // namespace aeyla::runtime
