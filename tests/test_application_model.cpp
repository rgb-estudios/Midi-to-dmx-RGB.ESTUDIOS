#include "product/application_model.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>
#include <utility>

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
  check(model.snapshot().project_name == "Untitled AEYLA Show",
        "snapshot must expose the project-owned name");
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

  auto authored = aeyla::project::make_default_project_document(
      "aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee", "2026-08-07T01:30:00Z");
  authored.name = "AEYLA Document-Driven Test";
  authored.fixtures[0].address = 201U;
  std::swap(authored.fixture_profiles[0].channels[3].slot,
            authored.fixture_profiles[0].channels[5].slot);
  authored.output.armed = true;

  model.set_backend_ready(true);
  const auto loaded = model.load_project_document(authored);
  check(loaded.ok(), "valid authored project must load into the shared runtime");
  check(model.snapshot().project_id == authored.project_id,
        "runtime snapshot must expose loaded project UUID");
  check(model.snapshot().project_name == authored.name,
        "runtime snapshot must expose loaded project name");
  check(!model.snapshot().output_armed,
        "project reload must never restore persisted output arm");
  check(model.snapshot().blackout,
        "successful project reload must remain in blackout");
  check(!model.project_document().output.armed,
        "authoritative loaded document must clear persisted arm state");

  model.set_blackout(false);
  model.handle_host_event(note_on);
  check(model.snapshot().dmx[200] == 255,
        "loaded physical patch must move fixture dimmer to authored address");
  check(model.snapshot().dmx[201] == 255,
        "loaded physical patch must preserve shutter at authored address");
  check(model.snapshot().dmx[203] < model.snapshot().dmx[205],
        "semantic blue must remain below red after profile-channel reordering");
  check(model.snapshot().dmx[205] > 0,
        "semantic red must follow the reordered fixture-profile channel");

  auto invalid = authored;
  invalid.fixtures[0].universe = 1U;
  const auto rejected = model.load_project_document(invalid);
  check(!rejected.ok(),
        "runtime must reject a project that violates one-universe Alpha 0.3 scope");
  check(!model.snapshot().project_valid,
        "rejected reload must invalidate authoritative runtime project state");
  check(!model.snapshot().output_armed,
        "rejected reload must remain disarmed");
  check(model.snapshot().blackout,
        "rejected reload must force blackout");
  check(all_zero(model.snapshot().dmx),
        "rejected reload must publish a safe zero DMX frame");

  if (failures == 0) {
    std::cout << "All AEYLA application model tests passed.\n";
    return EXIT_SUCCESS;
  }

  std::cerr << failures << " test(s) failed.\n";
  return EXIT_FAILURE;
}
