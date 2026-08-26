#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

namespace aeyla::capture {

struct DmxTakeConsolidateRequest {
  std::filesystem::path source_path;
  std::filesystem::path target_path;
  std::uint64_t start_frame{0U};
  std::uint64_t end_frame_exclusive{0U};
  std::string consolidated_name;
};

struct DmxTakeConsolidateResult {
  bool succeeded{false};
  std::filesystem::path target_path;
  std::uint64_t frame_count{0U};
  std::uint16_t frames_per_second{0U};
  double duration_seconds{0.0};
  std::string error;
};

// Consolida de forma no destructiva un rango de una TOMA ORIGINAL en un nuevo
// .aeylatake. La fuente nunca se modifica. El proceso usa lector y escritor
// file-backed con memoria acotada; no materializa el clip completo en RAM.
//
// El cuadro start_frame pasa a ser 00:00 del archivo consolidado.
[[nodiscard]] DmxTakeConsolidateResult consolidate_take_range(
    const DmxTakeConsolidateRequest& request);

}  // namespace aeyla::capture
