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

aeyla::show::ShowProgram make_ready_show(
    const aeyla::project::ProjectDocument& document,
    std::string song_name = "AEYLA Runtime Song") {
  using namespace aeyla::show;
  ShowProgram program;
  SongProgram song;
  song.song_id = "runtime-song";
  song.name = std::move(song_name);
  song.length_ticks = 4U * song.ppq;
  song.scenes.push_back({"scene-main", "Main", document.looks.front().look_id,
                         250U, 250U, false, CueBehavior::latch});
  song.clips.push_back({"clip-main", "scene-main", 0U, song.length_ticks,
                        36U, 127U, 1U});
  song.scenes.front().midi_binding = MidiBinding{36U, 1U};
  program.songs.push_back(std::move(song));
  return program;
}
}  // namespace

int main() {
  using aeyla::runtime::HostEvent;
  using aeyla::runtime::HostEventType;
  using aeyla::product::ApplicationModel;
  using aeyla::product::VisualSource;

  ApplicationModel model;
  check(model.snapshot().project_valid, "canonical development project should be valid");
  check(model.snapshot().project_dirty,
        "new untitled development project should begin dirty");
  check(model.snapshot().project_name == "Untitled AEYLA Show",
        "snapshot must expose the project-owned name");
  check(model.snapshot().song_count == 0U && !model.snapshot().performance_ready,
        "new project must be saveable authoring state but not performance-ready");
  check(!model.snapshot().backend_ready, "null backend must not report ready");
  check(!model.snapshot().output_armed, "output must start disarmed");
  check(model.snapshot().blackout, "blackout must start enabled");
  check(model.snapshot().global_blackout,
        "global operator blackout must start enabled");
  check(all_zero(model.snapshot().dmx), "startup blackout must compile a zero DMX frame");
  check(active_fixture_count(model.snapshot()) == 10,
        "default rig must preserve 14 positions with exactly 10 active fixtures");
  check(!model.request_arm(), "arming must fail while no backend is ready");

  model.set_backend_ready(true);
  const auto configured_output = model.configure_artnet_output("127.0.0.1", 23U);
  check(configured_output.succeeded &&
            model.project_document().output.backend == "artnet" &&
            model.project_document().output.target == "127.0.0.1" &&
            model.project_document().output.universe == 23U,
        "Art-Net setup must persist backend, target and 15-bit port address");
  check(std::all_of(model.project_document().fixtures.begin(),
                    model.project_document().fixtures.end(),
                    [](const auto& fixture) { return fixture.universe == 23U; }),
        "one-universe Art-Net setup must repatch fixture universe metadata atomically");
  check(model.snapshot().blackout && !model.snapshot().output_armed,
        "changing physical output configuration must force a safe boundary");
  check(!model.snapshot().backend_ready,
        "changing Art-Net endpoint must revoke readiness until socket preflight");
  const auto disabled_output = model.disable_output_backend();
  check(disabled_output.succeeded &&
            model.project_document().output.backend == "none" &&
            model.project_document().output.target.empty() &&
            !model.snapshot().backend_ready,
        "disabling physical output must persist NONE and revoke backend readiness");

  model.set_backend_ready(true);
  model.set_blackout(false);
  check(!model.request_arm(),
        "valid rig with no programmed song must fail the performance ARM gate");
  check(!model.snapshot().output_armed && model.snapshot().blackout,
        "failed show-readiness ARM must leave output disarmed and blacked out");

  const auto development_show = make_ready_show(model.project_document());
  const auto show_loaded = model.replace_show_program(development_show);
  check(show_loaded.ok(), "valid authored show must load into the application model");
  check(model.snapshot().performance_ready && model.snapshot().song_count == 1U,
        "one valid song must satisfy the show-program performance preflight");
  check(model.snapshot().blackout && !model.snapshot().output_armed,
        "replacing show semantics must force a safe output state");

  model.set_blackout(false);
  check(!model.snapshot().global_blackout,
        "clearing blackout must release the global operator latch");
  check(model.request_arm(),
        "arming should succeed only after project, show and backend validation");
  check(model.snapshot().output_armed, "snapshot must expose authoritative armed state");

  HostEvent note_on{};
  note_on.type = HostEventType::note_on;
  note_on.channel = 1U;
  note_on.note = 36;
  note_on.value = 1.0F;
  model.handle_host_event(note_on);

  check(model.snapshot().active_executor == -1,
        "authored Show MIDI must not leak into the diagnostic executor path");
  check(model.snapshot().active_scene_id == "scene-main" &&
            model.snapshot().active_scene_name == "Main",
        "MIDI note 36 must resolve the authored Cue when a Show is loaded");
  check(!all_zero(model.snapshot().dmx),
        "authored Cue with blackout off must compile a non-zero DMX frame");
  check(model.snapshot().dmx[0] == 255,
        "first reference fixture dimmer must be full at velocity 127");
  check(model.snapshot().dmx[1] == 255,
        "first reference fixture shutter must be open");
  check(model.snapshot().dmx[3] > 0,
        "solid executor must produce red emitter output");

  // Regression for the R07 REAPER report: a Song with no resolved Cue (or a
  // host position outside its bounds) produces an artistic black frame, but
  // must not silently re-latch the global APAGÓN control used by Take output.
  model.seek_active_song_tick(development_show.songs.front().length_ticks);
  check(model.snapshot().blackout,
        "an out-of-range Song position must keep the Show renderer black");
  check(!model.snapshot().global_blackout,
        "artistic Show black must not re-latch global APAGÓN or block a Take");
  model.seek_active_song_tick(0U);

  const auto golden = model.snapshot().dmx;
  ApplicationModel second_model;
  second_model.set_backend_ready(true);
  second_model.set_blackout(false);
  second_model.handle_host_event(note_on);
  check(second_model.snapshot().active_executor == 0 &&
            second_model.snapshot().active_scene_id.empty(),
        "without an authored Show, note 36 must remain a diagnostic executor");
  check(!all_zero(second_model.snapshot().dmx),
        "diagnostic executor must remain usable before a Show is authored");

  HostEvent note_off = note_on;
  note_off.type = HostEventType::note_off;
  note_off.value = 0.0F;
  model.handle_host_event(note_off);
  check(model.snapshot().active_scene_id == "scene-main",
        "Note Off must not release an authored LATCH Cue");
  check(model.snapshot().dmx == golden,
        "LATCH Cue output must remain deterministic after Note Off");

  second_model.handle_host_event(note_off);
  check(second_model.snapshot().active_executor == -1,
        "matching Note Off must release the diagnostic executor");

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
  const auto authored_show = make_ready_show(authored, "Authored Runtime Song");

  model.set_backend_ready(true);
  const auto loaded = model.load_project_bundle(authored, authored_show);
  check(loaded.ok(), "valid authored project+show bundle must load into shared runtime");
  check(!model.snapshot().project_dirty,
        "successfully loaded project+show bundle must begin clean");
  check(model.snapshot().project_id == authored.project_id,
        "runtime snapshot must expose loaded project UUID");
  check(model.snapshot().project_name == authored.name,
        "runtime snapshot must expose loaded project name");
  check(model.snapshot().song_count == 1U && model.snapshot().performance_ready,
        "runtime snapshot must expose loaded show readiness");
  check(model.show_program() == authored_show,
        "application model must own the loaded show program beside the project");
  check(!model.snapshot().output_armed,
        "project+show reload must never restore persisted output arm");
  check(model.snapshot().blackout,
        "successful project+show reload must remain in blackout");
  check(!model.project_document().output.armed,
        "authoritative loaded document must clear persisted arm state");

  model.set_visual_source(VisualSource::wave);
  model.set_visual_speed(0.72F);
  model.set_white_extraction(0.44F);
  model.set_amber_extraction(0.31F);
  model.set_uv_manual(0.28F);
  model.set_rig14(true);
  check(model.snapshot().project_dirty,
        "authored control changes must mark the project+show bundle dirty");

  const auto save_document =
      model.project_document_for_save("2026-08-07T01:45:00Z");
  const auto save_show = model.show_program_for_save();
  check(save_document.modified_at == "2026-08-07T01:45:00Z",
        "save snapshot must receive the requested modified timestamp");
  check(save_document.visual.active_look_id == "look-wave",
        "selected visual source must be reflected in the saved document");
  check(save_document.visual.speed == 0.72F,
        "visual speed must be reflected in the saved document");
  check(save_document.visual.white_extraction == 0.44F &&
            save_document.visual.amber_extraction == 0.31F &&
            save_document.visual.uv_manual == 0.28F,
        "authored color extraction must be reflected in the saved document");
  check(std::all_of(save_document.fixtures.begin(), save_document.fixtures.end(),
                    [](const auto& fixture) { return fixture.enabled; }),
        "Rig 14 selection must persist all fourteen enabled fixtures");
  check(!save_document.output.armed,
        "save snapshot must never persist output arm");
  check(save_show == authored_show,
        "save snapshot must preserve the authored musical show program");

  model.mark_project_saved("2026-08-07T01:45:00Z");
  check(!model.snapshot().project_dirty,
        "successful save acknowledgement must clear dirty state");
  check(model.project_document().modified_at == "2026-08-07T01:45:00Z",
        "successful save acknowledgement must update authoritative timestamp");

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

  // Invalid candidate loads are transactional: they must not invalidate or
  // partially replace the currently valid project+show runtime.
  auto invalid = authored;
  invalid.fixtures[0].universe = 1U;
  const auto before_rejected_load = model.snapshot();
  const auto before_show = model.show_program();
  const auto rejected = model.load_project_bundle(invalid, authored_show);
  check(!rejected.ok(),
        "runtime must reject a bundle that violates one-universe Alpha 0.3 scope");
  check(model.snapshot().project_valid &&
            model.snapshot().project_id == before_rejected_load.project_id &&
            model.snapshot().project_name == before_rejected_load.project_name,
        "rejected bundle must preserve the previously loaded valid project");
  check(model.show_program() == before_show,
        "rejected bundle must preserve the previously loaded valid show program");
  check(model.snapshot().blackout == before_rejected_load.blackout &&
            model.snapshot().output_armed == before_rejected_load.output_armed &&
            model.snapshot().dmx == before_rejected_load.dmx,
        "rejected bundle must not mutate live safety or DMX state");

  // Human authoring slice: no MIDI numbers are required to create a complete
  // Look, Song and Cue placement. Every semantic edit remains persisted and
  // every structural Store operation forces a safe output boundary.
  ApplicationModel authored_model;
  authored_model.set_visual_source(VisualSource::wave);
  authored_model.set_visual_speed(0.61F);
  authored_model.set_white_extraction(0.33F);
  check(authored_model.set_active_look_color(false, {0.10F, 0.80F, 0.25F}) &&
            authored_model.set_active_look_color(true, {0.15F, 0.20F, 0.95F}),
        "primary and secondary palette edits must update the active Look");
  check(authored_model.set_active_look_intensity(0.74F),
        "Look intensity must be editable independently from Grand Master");
  check(authored_model.active_look_fixture_enabled(13U) &&
            authored_model.toggle_active_look_fixture(13U) &&
            !authored_model.active_look_fixture_enabled(13U),
        "fixture participation must be an editable Look-owned mask");
  const auto stored_look = authored_model.store_current_look();
  check(stored_look.succeeded && !stored_look.object_id.empty(),
        "STORE LOOK must persist a named complete artistic state");
  authored_model.set_visual_source(VisualSource::solid);
  check(authored_model.select_look(
            authored_model.project_document().looks.size() - 1U) &&
            authored_model.active_look_color(false) ==
                std::array<float, 3>{0.10F, 0.80F, 0.25F},
        "stored Looks must be selectable again after changing source");
  const auto created_song = authored_model.create_song();
  check(created_song.succeeded && authored_model.snapshot().song_count == 1U,
        "NEW SONG must create one editable Song without requiring MIDI data");
  check(!authored_model.snapshot().performance_ready,
        "empty editable Song must remain below performance preflight");
  authored_model.set_blackout(false);
  check(authored_model.snapshot().blackout &&
            !authored_model.snapshot().global_blackout,
        "an empty Song may render black but must not re-latch global APAGÓN");
  const std::uint64_t authored_tick = 20U * 960U;
  const auto stored_cue = authored_model.store_cue_at_tick(authored_tick);
  check(stored_cue.succeeded && authored_model.snapshot().performance_ready,
        "STORE CUE at playhead must make the Song performance-ready");
  check(authored_model.show_program().songs.front().length_ticks > authored_tick &&
            authored_model.show_program().songs.front().clips.front().start_tick ==
                authored_tick,
        "STORE CUE must extend an authoring Song when the playhead is past its end");
  check(authored_model.show_program().songs.front().scenes.front()
            .midi_binding.has_value(),
        "stored Cue must own its hidden MIDI Learn binding");
  check(authored_model.snapshot().blackout &&
            !authored_model.snapshot().output_armed,
        "STORE LOOK/CUE structural changes must force blackout and disarm");
  const auto authored_document = authored_model.project_document_for_save(
      "2026-08-08T12:00:00Z");
  const auto stored_look_document = std::find_if(
      authored_document.looks.begin(), authored_document.looks.end(),
      [&](const auto& look) { return look.look_id == stored_look.object_id; });
  check(stored_look_document != authored_document.looks.end() &&
            stored_look_document->speed == 0.61F &&
            stored_look_document->white_extraction == 0.33F &&
            stored_look_document->intensity == 0.74F &&
            !stored_look_document->fixture_mask[13U] &&
            stored_look_document->primary_color ==
                std::array<float, 3>{0.10F, 0.80F, 0.25F} &&
            stored_look_document->secondary_color ==
                std::array<float, 3>{0.15F, 0.20F, 0.95F},
        "complete stored Look must own colors, speed, extraction and fixture mask");

  // Replacing the show with an empty but authoring-valid program is allowed,
  // but must immediately revoke performance readiness and force safe output.
  const aeyla::show::ShowProgram empty_show;
  check(model.replace_show_program(empty_show).ok(),
        "empty show must remain a valid editable authoring state");
  check(!model.snapshot().performance_ready && model.snapshot().song_count == 0U,
        "empty authored show must revoke performance readiness");
  check(model.snapshot().blackout && !model.snapshot().output_armed,
        "show semantic replacement must force blackout and disarm");
  model.set_backend_ready(true);
  model.set_blackout(false);
  check(!model.request_arm() && model.snapshot().blackout,
        "empty show must fail ARM even when backend and project are otherwise valid");

  if (failures == 0) {
    std::cout << "All AEYLA application model tests passed.\n";
    return EXIT_SUCCESS;
  }

  std::cerr << failures << " test(s) failed.\n";
  return EXIT_FAILURE;
}
