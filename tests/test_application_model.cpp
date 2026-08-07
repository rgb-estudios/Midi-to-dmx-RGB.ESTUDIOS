#include "product/application_model.h"

#include <algorithm>
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

bool all_zero(const aeyla::DmxUniverse& universe) {
  return std::all_of(universe.begin(), universe.end(),
                     [](std::uint8_t value) { return value == 0; });
}

int active_fixture_count(const aeyla::product::ApplicationSnapshot& snapshot) {
  return static_cast<int>(std::count_if(
      snapshot.fixtures.begin(), snapshot.fixtures.end(),
      [](const auto& fixture) { return fixture.active; }));
}
}  // namespace

int main() {
  using aeyla::runtime::HostEvent;
  using aeyla::runtime::HostEventType;
  using aeyla::product::ApplicationModel;

  ApplicationModel model;
  check(model.snapshot().project_valid, "canonical development project should be valid");
  check(!model.snapshot().backend_ready, "null backend must not report ready");
  check(!model.snapshot().output_armed, "output must start disarmed");
  check(model.snapshot().blackout, "blackout must start enabled");
  check(all_zero(model.snapshot().dmx), "startup blackout must compile a zero DMX frame");
  check(active_fixture_count(model.snapshot()) == 10,
        "default rig must preserve 14 positions with exactly 10 active fixtures");
  check(!model.request_arm(), "arming must fail while no backend is ready");

  model.set_backend_ready(true);
  model.set_blackout(false);
  check(model.request_arm(), "arming should succeed after project and backend validation");
  check(model.snapshot().output_armed, "snapshot must expose authoritative armed state");

  HostEvent note_on{};
  note_on.type = HostEventType::note_on;
  note_on.note = 36;
  note_on.value = 1.0F;
  model.handle_host_event(note_on);

  check(model.snapshot().active_executor == 0,
        "MIDI note 36 must activate executor 1 in the shared model");
  check(!all_zero(model.snapshot().dmx),
        "active executor with blackout off must compile a non-zero DMX frame");
  check(model.snapshot().dmx[0] == 255,
        "first reference fixture dimmer must be full at velocity 127");
  check(model.snapshot().dmx[1] == 255,
        "first reference fixture shutter must be open");
  check(model.snapshot().dmx[3] > 0,
        "solid executor must produce red emitter output");

  const auto golden = model.snapshot().dmx;
  ApplicationModel second_model;
  second_model.set_backend_ready(true);
  second_model.set_blackout(false);
  second_model.handle_host_event(note_on);
  check(second_model.snapshot().dmx == golden,
        "identical standalone/VST3 commands must produce byte-identical DMX");

  HostEvent note_off = note_on;
  note_off.type = HostEventType::note_off;
  note_off.value = 0.0F;
  model.handle_host_event(note_off);
  check(model.snapshot().active_executor == -1,
        "matching Note Off must release the active executor");

  model.set_rig14(true);
  check(active_fixture_count(model.snapshot()) == 14,
        "Rig 14 must activate all fourteen physical outputs");

  model.set_blackout(true);
  check(all_zero(model.snapshot().dmx),
        "blackout must override every artistic and executor value");

  model.set_backend_ready(false);
  check(!model.snapshot().output_armed,
        "backend loss must disarm output through shared safety state");
  check(model.snapshot().blackout,
        "backend loss must leave blackout enabled");

  if (failures == 0) {
    std::cout << "All AEYLA application model tests passed.\n";
    return EXIT_SUCCESS;
  }

  std::cerr << failures << " test(s) failed.\n";
  return EXIT_FAILURE;
}
