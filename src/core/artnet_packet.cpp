#include "core/artnet_packet.h"

#include <algorithm>
#include <stdexcept>

namespace aeyla {

std::vector<std::uint8_t> make_artdmx_packet(const DmxUniverse& universe,
                                             std::uint16_t port_address,
                                             std::uint8_t sequence,
                                             std::uint16_t channel_count) {
  if (channel_count < 2 || channel_count > 512 || channel_count % 2 != 0) {
    throw std::invalid_argument("ArtDMX channel_count must be even and between 2 and 512");
  }
  if (port_address > 0x7FFF) throw std::invalid_argument("Art-Net port address exceeds 15 bits");

  std::vector<std::uint8_t> packet(18 + channel_count, 0);
  const char id[8] = {'A', 'r', 't', '-', 'N', 'e', 't', '\0'};
  std::copy(id, id + 8, packet.begin());
  packet[8] = 0x00;   // OpCode ArtDMX, little-endian 0x5000
  packet[9] = 0x50;
  packet[10] = 0x00;  // Protocol version 14, big-endian
  packet[11] = 0x0E;
  packet[12] = sequence;
  packet[13] = 0x00;  // Physical
  packet[14] = static_cast<std::uint8_t>(port_address & 0xFF);
  packet[15] = static_cast<std::uint8_t>((port_address >> 8) & 0x7F);
  packet[16] = static_cast<std::uint8_t>((channel_count >> 8) & 0xFF);
  packet[17] = static_cast<std::uint8_t>(channel_count & 0xFF);
  std::copy_n(universe.begin(), channel_count, packet.begin() + 18);
  return packet;
}

}  // namespace aeyla
