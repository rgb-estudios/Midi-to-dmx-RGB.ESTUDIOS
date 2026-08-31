#include "runtime/runtime_safety_state.h"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {
int failures = 0;
void check(bool condition, const std::string& message) {
  if (!condition) {
    ++failures;
    std::cerr << "FAIL: " << message << '\n';
  }
}
}  // namespace

int main() {
  using aeyla::runtime::RuntimeSafetyReason;
  using aeyla::runtime::RuntimeSafetyState;

  RuntimeSafetyState state;
  check(!state.output_armed(), "startup must be disarmed");
  check(state.blackout(), "startup must be blacked out");
  check(!state.request_arm(), "cannot arm without valid project/backend");
  check(state.reason() == RuntimeSafetyReason::project_invalid,
        "missing project should explain arm rejection");

  state.set_project_valid(true);
  check(!state.request_arm(), "cannot arm without backend");
  state.set_backend_ready(true);
  check(state.request_arm(), "valid project and backend may arm");
  check(state.output_armed(), "arm request should set armed");

  state.set_blackout(false);
  check(!state.blackout(), "operator can clear blackout after explicit arm readiness");

  state.on_event_overflow();
  check(state.output_armed(), "event overflow releases transients but need not drop base output");
  auto actions = state.consume_pending_actions();
  check(actions.release_transients, "overflow must release transients");
  check(actions.force_haze_zero, "overflow must force haze zero");
  check(!actions.publish_safe_frame, "overflow may retain deterministic base frame");
  actions = state.consume_pending_actions();
  check(!actions.release_transients && !actions.force_haze_zero && !actions.publish_safe_frame,
        "actions should clear after consumption");

  state.begin_project_reload();
  check(!state.output_armed() && state.blackout(), "reload must disarm and blackout");
  check(state.reload_in_progress(), "reload flag should be set");
  check(!state.request_arm(), "cannot arm during reload");
  actions = state.consume_pending_actions();
  check(actions.publish_safe_frame, "reload must request safe frame");

  state.complete_project_reload(true);
  check(state.project_valid(), "successful reload validates project");
  check(!state.output_armed(), "successful reload still requires explicit re-arm");
  check(state.blackout(), "successful reload remains blacked out");

  state.set_backend_ready(true);
  check(state.request_arm(), "may re-arm after successful reload");
  state.set_project_valid(false);
  check(!state.output_armed() && state.blackout(), "invalid project must force safe state");
  check(state.reason() == RuntimeSafetyReason::project_invalid,
        "invalid project reason should be retained");

  state.set_project_valid(true);
  state.set_backend_ready(true);
  check(state.request_arm(), "may arm before host deactivation test");
  state.set_blackout(false);
  (void)state.consume_pending_actions();
  state.on_host_deactivation();
  check(state.output_armed() && !state.blackout(),
        "host/editor deactivation must preserve explicit operator authority");
  actions = state.consume_pending_actions();
  check(!actions.release_transients && !actions.force_haze_zero &&
            !actions.publish_safe_frame,
        "host/editor deactivation must not request a physical withdrawal frame");

  state.set_project_valid(true);
  state.set_backend_ready(true);
  check(state.request_arm(), "may arm before offline-render inhibit test");
  state.set_blackout(false);
  state.disarm(RuntimeSafetyReason::offline_render);
  state.set_blackout(true);
  check(!state.output_armed() && state.blackout(),
        "offline render must leave output disarmed and blacked out");
  check(state.reason() == RuntimeSafetyReason::offline_render,
        "offline-render inhibit reason should be retained");

  state.set_project_valid(true);
  state.set_backend_ready(true);
  check(state.request_arm(), "may arm before runtime-fault inhibit test");
  state.disarm(RuntimeSafetyReason::runtime_fault);
  state.set_blackout(true);
  check(!state.output_armed() && state.blackout() &&
            state.reason() == RuntimeSafetyReason::runtime_fault,
        "runtime fault must latch an explicit safe reason");

  state.set_project_valid(true);
  state.set_backend_ready(true);
  check(state.request_arm(), "may arm before shutdown test");
  state.on_shutdown();
  check(!state.output_armed() && state.blackout(), "shutdown must be safe");
  check(state.reason() == RuntimeSafetyReason::shutdown, "shutdown reason should be recorded");

  if (failures == 0) {
    std::cout << "All runtime safety tests passed.\n";
    return EXIT_SUCCESS;
  }
  std::cerr << failures << " test(s) failed.\n";
  return EXIT_FAILURE;
}
