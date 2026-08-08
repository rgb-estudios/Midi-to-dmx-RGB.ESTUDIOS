#include "programmer/programmer.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {
int failures = 0;

void check(bool condition, const std::string& message) {
  if (!condition) {
    ++failures;
    std::cerr << "FAIL: " << message << '\n';
  }
}

aeyla::programmer::CompleteLookState make_base() {
  using namespace aeyla;
  using namespace aeyla::programmer;

  CompleteLookState base;
  for (const char* id : {"par-01", "par-02", "par-03"}) {
    CompleteFixtureState fixture;
    fixture.fixture_id = id;
    set(fixture.attributes, Attribute::Dimmer, 0.50F);
    set(fixture.attributes, Attribute::Red, 0.10F);
    set(fixture.attributes, Attribute::Blue, 0.20F);
    base.fixtures.push_back(fixture);
  }
  return base;
}
}  // namespace

int main() {
  using namespace aeyla;
  using namespace aeyla::programmer;

  Programmer programmer;
  check(!programmer.configured(), "programmer must start unconfigured");
  check(!programmer.configure({}), "empty fixture configuration must be rejected");
  check(!programmer.configure({"par-01", "par-01"}),
        "duplicate stable fixture IDs must be rejected");

  std::vector<std::string> too_many;
  for (std::size_t index = 0U; index < 15U; ++index)
    too_many.push_back("par-" + std::to_string(index + 1U));
  check(!programmer.configure(too_many),
        "AEYLA v1 programmer must reject a 15th fixture");

  check(programmer.configure({"par-01", "par-02", "par-03"}),
        "valid programmer fixture set must configure");
  check(programmer.fixture_count() == 3U, "fixture count must match configuration");
  check(programmer.selected_count() == 0U, "configuration must not auto-select fixtures");

  check(programmer.select_fixture("par-01"), "known fixture must be selectable");
  check(programmer.select_fixture("par-02"), "second fixture must be selectable");
  check(!programmer.select_fixture("missing"), "unknown fixture selection must fail");
  check(programmer.selected_count() == 2U, "two fixtures should be selected");

  check(programmer.set_selected(Attribute::Dimmer, 0.80F),
        "selected fixtures must accept programmer attributes");
  check(programmer.has_programmed_values(), "programmer must report touched values");

  const auto base = make_base();
  const auto par1_preview = programmer.preview_fixture("par-01", base.fixtures[0].attributes);
  const auto par3_preview = programmer.preview_fixture("par-03", base.fixtures[2].attributes);
  check(par1_preview.has_value() && get(*par1_preview, Attribute::Dimmer) == 0.80F,
        "selected fixture preview must overlay programmed dimmer");
  check(par3_preview.has_value() && get(*par3_preview, Attribute::Dimmer) == 0.50F,
        "untouched fixture preview must preserve base dimmer");

  // An explicit zero is a real programmed value, not the same as untouched.
  programmer.clear_selection();
  check(programmer.select_fixture("par-01"), "fixture must remain addressable after clear selection");
  check(programmer.set_selected(Attribute::Red, 0.0F),
        "explicit zero must be accepted as a programmed value");
  const auto explicit_zero = programmer.preview_fixture("par-01", base.fixtures[0].attributes);
  check(explicit_zero.has_value() && get(*explicit_zero, Attribute::Red) == 0.0F,
        "explicit zero must override a non-zero base value");

  check(programmer.clear_selected_attribute(Attribute::Red),
        "clearing a touched attribute must succeed");
  const auto inherited_red = programmer.preview_fixture("par-01", base.fixtures[0].attributes);
  check(inherited_red.has_value() && get(*inherited_red, Attribute::Red) == 0.10F,
        "cleared attribute must inherit from the base Look again");

  const auto captured = programmer.capture_complete_look(base);
  check(captured.has_value(), "STORE LOOK capture must produce a complete state");
  if (captured.has_value()) {
    check(captured->fixtures.size() == 3U, "captured Look must contain every configured fixture");
    check(get(captured->fixtures[0].attributes, Attribute::Dimmer) == 0.80F,
          "captured selected fixture must contain the programmed dimmer");
    check(get(captured->fixtures[1].attributes, Attribute::Dimmer) == 0.80F,
          "previously programmed fixture keeps its programmer value even when deselected");
    check(get(captured->fixtures[2].attributes, Attribute::Dimmer) == 0.50F,
          "captured untouched fixture must contain complete base state");
  }

  // Capture is stable by fixture ID, not vector order.
  auto reordered_base = base;
  std::swap(reordered_base.fixtures[0], reordered_base.fixtures[2]);
  const auto reordered_capture = programmer.capture_complete_look(reordered_base);
  check(reordered_capture.has_value() &&
            reordered_capture->fixtures.front().fixture_id == "par-01" &&
            get(reordered_capture->fixtures.front().attributes, Attribute::Blue) == 0.20F,
        "capture must resolve base state by stable fixture ID rather than vector position");

  auto incomplete_base = base;
  incomplete_base.fixtures.pop_back();
  check(!programmer.capture_complete_look(incomplete_base).has_value(),
        "STORE LOOK must fail rather than silently capture an incomplete rig");

  programmer.clear_programmed_values();
  check(!programmer.has_programmed_values(), "CLEAR must remove all programmed overrides");
  check(programmer.selected_count() == 1U,
        "CLEAR programmed values must not destroy fixture selection state");
  const auto cleared = programmer.preview_fixture("par-01", base.fixtures[0].attributes);
  check(cleared.has_value() && get(*cleared, Attribute::Dimmer) == 0.50F,
        "CLEAR must restore preview to the complete base Look");

  programmer.select_all();
  check(programmer.selected_count() == 3U, "select all must select every configured fixture");
  programmer.clear_selection();
  check(programmer.selected_count() == 0U, "clear selection must deselect every fixture");
  check(!programmer.set_selected(Attribute::Blue, 1.0F),
        "writing with no selected fixtures must report a no-op");

  if (failures == 0) {
    std::cout << "All AEYLA programmer tests passed.\n";
    return EXIT_SUCCESS;
  }

  std::cerr << failures << " test(s) failed.\n";
  return EXIT_FAILURE;
}
