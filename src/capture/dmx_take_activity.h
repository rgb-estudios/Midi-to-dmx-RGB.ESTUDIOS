#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace aeyla::capture {

inline constexpr std::size_t kMaximumTakeActivityBuckets = 256U;

struct DmxTakeActivityBucket {
  // Nivel máximo observado en cualquier canal dentro del bucket.
  std::uint8_t level{0U};
  // Mayor cambio de un canal entre cuadros consecutivos dentro del bucket.
  std::uint8_t motion{0U};
};

struct DmxTakeActivityEnvelope {
  std::uint64_t frame_count{0U};
  std::uint16_t frames_per_second{0U};
  std::vector<DmxTakeActivityBucket> buckets;
  std::string error;

  [[nodiscard]] bool ok() const noexcept { return error.empty(); }
};

// Construye una representación visual acotada de la actividad real de los 512
// canales. Lee el archivo secuencialmente mediante la caché fija del lector y
// nunca retiene la toma completa en RAM.
[[nodiscard]] DmxTakeActivityEnvelope build_take_activity_envelope(
    const std::filesystem::path& path,
    std::size_t requested_buckets = kMaximumTakeActivityBuckets);

}  // namespace aeyla::capture
