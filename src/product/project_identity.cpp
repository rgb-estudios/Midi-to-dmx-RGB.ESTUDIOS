#include "product/project_identity.h"

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <iomanip>
#include <random>
#include <sstream>

namespace aeyla::product {
namespace {

std::uint64_t fallback_seed() noexcept {
  static std::atomic<std::uint64_t> counter{0U};
  const auto ticks = static_cast<std::uint64_t>(
      std::chrono::high_resolution_clock::now().time_since_epoch().count());
  return ticks ^ (++counter * 0x9E3779B97F4A7C15ULL);
}

}  // namespace

std::string generate_project_uuid() {
  std::array<std::uint8_t, 16> bytes{};
  try {
    std::random_device random;
    for (auto& value : bytes)
      value = static_cast<std::uint8_t>(random());
  } catch (...) {
    std::mt19937_64 generator(fallback_seed());
    for (auto& value : bytes)
      value = static_cast<std::uint8_t>(generator());
  }

  bool all_zero = true;
  for (const auto value : bytes) all_zero = all_zero && value == 0U;
  if (all_zero) {
    std::mt19937_64 generator(fallback_seed());
    for (auto& value : bytes)
      value = static_cast<std::uint8_t>(generator());
  }

  bytes[6] = static_cast<std::uint8_t>((bytes[6] & 0x0FU) | 0x40U);
  bytes[8] = static_cast<std::uint8_t>((bytes[8] & 0x3FU) | 0x80U);

  std::ostringstream output;
  output << std::hex << std::setfill('0');
  for (std::size_t index = 0U; index < bytes.size(); ++index) {
    if (index == 4U || index == 6U || index == 8U || index == 10U)
      output << '-';
    output << std::setw(2) << static_cast<unsigned int>(bytes[index]);
  }
  return output.str();
}

std::string current_utc_timestamp() {
  const std::time_t time = std::chrono::system_clock::to_time_t(
      std::chrono::system_clock::now());
  std::tm utc{};
#ifdef _WIN32
  if (gmtime_s(&utc, &time) != 0) return "1970-01-01T00:00:00Z";
#else
  if (gmtime_r(&time, &utc) == nullptr) return "1970-01-01T00:00:00Z";
#endif

  char buffer[32]{};
  if (std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &utc) == 0U)
    return "1970-01-01T00:00:00Z";
  return buffer;
}

}  // namespace aeyla::product
