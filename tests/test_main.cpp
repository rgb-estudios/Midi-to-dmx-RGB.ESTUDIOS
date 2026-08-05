#include "core/artnet_packet.h"
#include "core/dmx_compiler.h"
#include "core/fixture_profile.h"
#include "core/rig.h"

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

aeyla::AttributeFrame red_look() {
  aeyla::AttributeFrame frame{};
  aeyla::set(frame, aeyla::Attribute::Dimmer, 0.5F);
  aeyla::set(frame, aeyla::Attribute::Red, 1.0F);
  aeyla::set(frame, aeyla::Attribute::Green, 0.25F);
  return frame;
}
}

int main() {
  using namespace aeyla;

  FixtureProfile original{
      "original", "Original", 4,
      {{1, Attribute::Dimmer}, {2, Attribute::Red}, {3, Attribute::Green}, {4, Attribute::Blue}}};
  FixtureProfile reordered{
      "reordered", "Reordered", 4,
      {{4, Attribute::Dimmer}, {1, Attribute::Red}, {2, Attribute::Blue}, {3, Attribute::Green}}};

  PatchedFixture a{"L1", 1, true, original, red_look()};
  PatchedFixture b{"L1", 1, true, reordered, red_look()};
  const auto da = compile_dmx({a});
  const auto db = compile_dmx({b});

  check(da.universe[0] == 128, "original dimmer should be 128");
  check(da.universe[1] == 255, "original red should be 255");
  check(da.universe[2] == 64, "original green should be 64");
  check(db.universe[3] == da.universe[0], "reordered dimmer must preserve semantic value");
  check(db.universe[0] == da.universe[1], "reordered red must preserve semantic value");
  check(db.universe[2] == da.universe[2], "reordered green must preserve semantic value");

  FixtureProfile no_dimmer{"rgb", "RGB no dimmer", 3,
                            {{1, Attribute::Red}, {2, Attribute::Green}, {3, Attribute::Blue}}};
  PatchedFixture c{"L1", 1, true, no_dimmer, red_look()};
  const auto dc = compile_dmx({c});
  check(dc.universe[0] == 128, "colour must be multiplied by logical dimmer when fixture has no dimmer");

  const auto rig10 = make_floor_rig_10();
  int active = 0;
  for (const auto& fixture : rig10) active += fixture.active ? 1 : 0;
  check(rig10.size() == 14, "rig10 must preserve 14 logical positions");
  check(active == 10, "rig10 must activate exactly 10 physical fixtures");

  DmxUniverse universe{};
  universe[0] = 123;
  const auto packet = make_artdmx_packet(universe, 0, 7, 512);
  check(packet.size() == 530, "ArtDMX packet must contain 18-byte header plus 512 data bytes");
  check(packet[0] == 'A' && packet[7] == 0, "Art-Net ID must be valid");
  check(packet[8] == 0x00 && packet[9] == 0x50, "ArtDMX opcode must be valid");
  check(packet[12] == 7, "ArtDMX sequence must be preserved");
  check(packet[18] == 123, "DMX payload must begin at byte 18");

  if (failures == 0) {
    std::cout << "All AEYLA core tests passed\n";
    return EXIT_SUCCESS;
  }
  return EXIT_FAILURE;
}
