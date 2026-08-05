#include "core/artnet_packet.h"
#include "core/color_transform.h"
#include "core/dmx_compiler.h"
#include "core/fixture_profile.h"

#include <iomanip>
#include <iostream>

int main() {
  using namespace aeyla;

  FixtureProfile profile{
      "generic-rgbw-6ch", "Generic RGBW 6ch", 6,
      {{1, Attribute::Dimmer}, {2, Attribute::Strobe}, {3, Attribute::Red},
       {4, Attribute::Green}, {5, Attribute::Blue}, {6, Attribute::White}}};

  PatchedFixture fixture{};
  fixture.logical_id = "L1";
  fixture.address = 1;
  fixture.profile = profile;
  fixture.values = transform_rgb({1.0F, 0.12F, 0.02F}, {.intensity = 0.75F});

  const auto compiled = compile_dmx({fixture});
  const auto packet = make_artdmx_packet(compiled.universe, 0, 1, 512);

  std::cout << "AEYLA core demo\n";
  std::cout << "DMX 1-6:";
  for (int i = 0; i < 6; ++i) std::cout << ' ' << static_cast<int>(compiled.universe.at(i));
  std::cout << "\nArtDMX bytes: " << packet.size() << "\n";
  return compiled.warnings.empty() ? 0 : 1;
}
