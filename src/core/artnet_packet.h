#pragma once

#include "core/dmx_compiler.h"

#include <cstdint>
#include <vector>

namespace aeyla {

std::vector<std::uint8_t> make_artdmx_packet(const DmxUniverse& universe,
                                             std::uint16_t port_address,
                                             std::uint8_t sequence = 0,
                                             std::uint16_t channel_count = 512);

}  // namespace aeyla
