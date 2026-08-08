#pragma once

#include "core/attributes.h"

#include <algorithm>
#include <bitset>
#include <cstddef>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace aeyla::programmer {

constexpr std::size_t kMaximumProgrammerFixtures = 14U;

struct CompleteFixtureState {
  std::string fixture_id;
  AttributeFrame attributes{};
  bool operator==(const CompleteFixtureState&) const = default;
};

// A complete reproducible lighting state. Unlike the live Programmer, this has
// no notion of "untouched" attributes: every fixture owns a complete semantic
// AttributeFrame. This is the shape that a persisted Look must ultimately hold.
struct CompleteLookState {
  std::vector<CompleteFixtureState> fixtures;
  bool operator==(const CompleteLookState&) const = default;
};

struct ProgrammerFixtureState {
  std::string fixture_id;
  AttributeFrame values{};
  std::bitset<attribute_count> touched{};
  bool selected{false};
};

// Console-style lighting Programmer.
//
// The Programmer is deliberately independent of DAW audio, set-list playback,
// Art-Net ownership and timeline storage. It only answers the authoring question:
// "which fixtures are selected, and which lighting attributes have I changed?"
//
// Untouched values pass through from the current/base Look. An explicitly
// programmed zero is therefore different from an untouched attribute, matching
// conventional lighting-console programmer semantics.
class Programmer final {
 public:
  Programmer() = default;

  [[nodiscard]] bool configure(std::vector<std::string> fixture_ids) {
    if (fixture_ids.empty() || fixture_ids.size() > kMaximumProgrammerFixtures)
      return false;

    std::set<std::string> unique;
    for (const auto& id : fixture_ids) {
      if (id.empty() || !unique.insert(id).second) return false;
    }

    fixtures_.clear();
    fixtures_.reserve(fixture_ids.size());
    for (auto& id : fixture_ids) {
      ProgrammerFixtureState fixture;
      fixture.fixture_id = std::move(id);
      fixtures_.push_back(std::move(fixture));
    }
    return true;
  }

  [[nodiscard]] bool configured() const noexcept { return !fixtures_.empty(); }
  [[nodiscard]] std::size_t fixture_count() const noexcept {
    return fixtures_.size();
  }

  [[nodiscard]] std::size_t selected_count() const noexcept {
    return static_cast<std::size_t>(std::count_if(
        fixtures_.begin(), fixtures_.end(),
        [](const ProgrammerFixtureState& fixture) { return fixture.selected; }));
  }

  [[nodiscard]] bool select_fixture(std::string_view fixture_id,
                                    bool selected = true) noexcept {
    auto* fixture = find_fixture(fixture_id);
    if (fixture == nullptr) return false;
    fixture->selected = selected;
    return true;
  }

  void select_all(bool selected = true) noexcept {
    for (auto& fixture : fixtures_) fixture.selected = selected;
  }

  void clear_selection() noexcept { select_all(false); }

  // Writes one semantic lighting attribute to every selected fixture. Returns
  // false when no fixture is selected so a MIDI/UI gesture can detect a no-op.
  [[nodiscard]] bool set_selected(Attribute attribute, float value) noexcept {
    bool changed = false;
    const auto index = static_cast<std::size_t>(attribute);
    const float normalized = clamp_normalized(value);
    for (auto& fixture : fixtures_) {
      if (!fixture.selected) continue;
      fixture.values[index] = normalized;
      fixture.touched.set(index);
      changed = true;
    }
    return changed;
  }

  // Releases one attribute from the Programmer for selected fixtures. The next
  // preview/capture therefore inherits that attribute from the current/base Look.
  [[nodiscard]] bool clear_selected_attribute(Attribute attribute) noexcept {
    bool changed = false;
    const auto index = static_cast<std::size_t>(attribute);
    for (auto& fixture : fixtures_) {
      if (!fixture.selected || !fixture.touched.test(index)) continue;
      fixture.touched.reset(index);
      fixture.values[index] = 0.0F;
      changed = true;
    }
    return changed;
  }

  // CLEAR in a lighting-console sense: remove all programmed overrides while
  // preserving fixture selection. Selection is a UI/workflow state, not output.
  void clear_programmed_values() noexcept {
    for (auto& fixture : fixtures_) {
      fixture.values.fill(0.0F);
      fixture.touched.reset();
    }
  }

  [[nodiscard]] bool has_programmed_values() const noexcept {
    return std::any_of(fixtures_.begin(), fixtures_.end(),
                       [](const ProgrammerFixtureState& fixture) {
                         return fixture.touched.any();
                       });
  }

  [[nodiscard]] const ProgrammerFixtureState* fixture(
      std::string_view fixture_id) const noexcept {
    return find_fixture(fixture_id);
  }

  // Applies the Programmer overlay to one complete base frame. This is the live
  // preview primitive used before STORE CUE.
  [[nodiscard]] std::optional<AttributeFrame> preview_fixture(
      std::string_view fixture_id, const AttributeFrame& base) const noexcept {
    const auto* programmed = find_fixture(fixture_id);
    if (programmed == nullptr) return std::nullopt;

    AttributeFrame result = base;
    for (std::size_t index = 0U; index < attribute_count; ++index) {
      if (programmed->touched.test(index)) result[index] = programmed->values[index];
    }
    return result;
  }

  // Freezes the current Programmer over a complete base Look. The result is a
  // complete reproducible lighting state suitable for STORE LOOK / STORE CUE.
  // Base matching is by stable fixture ID rather than vector order.
  [[nodiscard]] std::optional<CompleteLookState> capture_complete_look(
      const CompleteLookState& base) const {
    if (!configured() || base.fixtures.size() != fixtures_.size())
      return std::nullopt;

    std::set<std::string> seen;
    for (const auto& fixture : base.fixtures) {
      if (fixture.fixture_id.empty() || !seen.insert(fixture.fixture_id).second)
        return std::nullopt;
    }

    CompleteLookState result;
    result.fixtures.reserve(fixtures_.size());
    for (const auto& programmed : fixtures_) {
      const auto base_it = std::find_if(
          base.fixtures.begin(), base.fixtures.end(),
          [&](const CompleteFixtureState& fixture) {
            return fixture.fixture_id == programmed.fixture_id;
          });
      if (base_it == base.fixtures.end()) return std::nullopt;

      CompleteFixtureState captured = *base_it;
      for (std::size_t index = 0U; index < attribute_count; ++index) {
        if (programmed.touched.test(index))
          captured.attributes[index] = programmed.values[index];
      }
      result.fixtures.push_back(std::move(captured));
    }
    return result;
  }

 private:
  [[nodiscard]] ProgrammerFixtureState* find_fixture(
      std::string_view fixture_id) noexcept {
    const auto found = std::find_if(
        fixtures_.begin(), fixtures_.end(),
        [&](const ProgrammerFixtureState& fixture) {
          return fixture.fixture_id == fixture_id;
        });
    return found == fixtures_.end() ? nullptr : &*found;
  }

  [[nodiscard]] const ProgrammerFixtureState* find_fixture(
      std::string_view fixture_id) const noexcept {
    const auto found = std::find_if(
        fixtures_.begin(), fixtures_.end(),
        [&](const ProgrammerFixtureState& fixture) {
          return fixture.fixture_id == fixture_id;
        });
    return found == fixtures_.end() ? nullptr : &*found;
  }

  std::vector<ProgrammerFixtureState> fixtures_;
};

}  // namespace aeyla::programmer
