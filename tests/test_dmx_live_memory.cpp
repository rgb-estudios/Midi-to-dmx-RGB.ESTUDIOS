#include "output/dmx_live_memory.h"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {

int failures = 0;

void check(bool condition, const std::string& message) {
  if(condition) return;
  ++failures;
  std::cerr << "FAIL: " << message << '\n';
}

}  // namespace

int main() {
  using namespace aeyla;
  using namespace aeyla::output;
  using namespace std::chrono_literals;

  DmxLiveMemoryEngine engine;
  std::string error;

  LiveMemoryDefinition invalid;
  invalid.memory_id = "invalid";
  invalid.name = "Invalid";
  check(!engine.configure(0U, invalid, error),
        "memory without DMX mask must fail closed");

  LiveMemoryDefinition front;
  front.memory_id = "front";
  front.name = "FRONTAL";
  front.fade_ms = 1000U;
  front.target[0] = 110U;
  front.mask.set(0U);
  check(engine.configure(0U, front, error),
        "front memory must configure: " + error);

  DmxUniverse base{};
  base[0] = 10U;
  base[1] = 20U;
  base[2] = 30U;

  const auto t0 = DmxLiveMemoryEngine::TimePoint{};
  check(engine.set_target_level(0U, 1.0F, t0),
        "front memory must accept fade-in target");
  const auto halfFront = engine.compose(base, t0 + 500ms);
  check(halfFront[0] == 60U,
        "1 s front fade must be exactly half at 500 ms");
  check(halfFront[1] == base[1] && halfFront[2] == base[2],
        "masked front memory must never touch unrelated channels");
  const auto frontSnapshot = engine.snapshot(0U, t0 + 500ms);
  check(frontSnapshot.configured && frontSnapshot.transitioning &&
            frontSnapshot.channel_count == 1U &&
            frontSnapshot.level > 0.49F && frontSnapshot.level < 0.51F,
        "front snapshot must expose deterministic fade state");

  const auto fullFront = engine.compose(base, t0 + 1000ms);
  check(fullFront[0] == 110U,
        "front memory must reach its exact target after fade-in");

  LiveMemoryDefinition white;
  white.memory_id = "white-base";
  white.name = "BASE BLANCA";
  white.fade_ms = 1000U;
  white.target[0] = 210U;
  white.mask.set(0U);
  check(engine.configure(1U, white, error),
        "white memory must configure: " + error);

  check(engine.set_direct_level(0U, 1.0F, t0 + 1100ms),
        "front memory direct level must succeed");
  check(engine.set_direct_level(1U, 1.0F, t0 + 1200ms),
        "newer overlapping memory must activate");
  const auto whiteWins = engine.compose(base, t0 + 1200ms);
  check(whiteWins[0] == 210U,
        "newest active memory must win overlapping channel LTP order");

  check(engine.set_target_level(1U, 0.0F, t0 + 1300ms),
        "white memory must accept fade-out target");
  const auto revealHalf = engine.compose(base, t0 + 1800ms);
  check(revealHalf[0] == 160U,
        "fading newer memory must continuously reveal older front memory");
  const auto revealDone = engine.compose(base, t0 + 2300ms);
  check(revealDone[0] == 110U,
        "released newer memory must reveal underlying front target exactly");

  LiveMemoryDefinition haze;
  haze.memory_id = "haze";
  haze.name = "HUMO / HAZE";
  haze.mode = LiveMemoryControlMode::fader;
  haze.fade_ms = 1500U;
  haze.target[1] = 220U;
  haze.mask.set(1U);
  check(engine.configure(2U, haze, error),
        "haze fader memory must configure: " + error);
  check(engine.set_direct_level(2U, 0.25F, t0 + 2400ms),
        "continuous fader level must apply without transport coupling");
  const auto hazeQuarter = engine.compose(base, t0 + 2400ms);
  check(hazeQuarter[1] == 70U,
        "25 percent fader must interpolate base 20 toward target 220");

  engine.reset_levels();
  const auto safe = engine.compose(base, t0 + 2500ms);
  check(safe == base,
        "DISARM/PANIC reset must return every memory to safe OFF");
  const auto configuredAfterReset = engine.snapshot(0U, t0 + 2500ms);
  check(configuredAfterReset.configured && configuredAfterReset.level == 0.0F &&
            configuredAfterReset.target_level == 0.0F,
        "safe reset must preserve memory definition but never active level");

  check(!engine.toggle(kMaximumLiveMemories, t0),
        "out-of-range memory command must fail without side effects");

  if(failures == 0) {
    std::cout << "AEYLA live-memory PASS: masked fades, overlap reveal and safe reset\n";
    return EXIT_SUCCESS;
  }
  std::cerr << failures << " test(s) failed.\n";
  return EXIT_FAILURE;
}
