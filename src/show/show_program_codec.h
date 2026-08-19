#pragma once

#include "show/show_program.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace aeyla::show {

constexpr std::uint16_t kShowCodecMajor = 1U;
constexpr std::uint16_t kShowCodecMinor = 1U;
constexpr std::size_t kMaximumEncodedShowBytes = 4U * 1024U * 1024U;

struct ShowCodecDiagnostic {
  std::size_t offset{0U};
  std::string message;
  bool operator==(const ShowCodecDiagnostic&) const = default;
};

struct ShowEncodeResult {
  std::vector<std::uint8_t> bytes;
  std::vector<ShowCodecDiagnostic> diagnostics;

  [[nodiscard]] bool ok() const noexcept {
    return !bytes.empty() && diagnostics.empty();
  }
};

struct ShowDecodeResult {
  std::optional<ShowProgram> program;
  std::vector<ShowCodecDiagnostic> diagnostics;

  [[nodiscard]] bool ok() const noexcept {
    return program.has_value() && diagnostics.empty();
  }
};

// Deterministic, bounded binary codec for the authored show program stored
// inside `.aeylashow` as `show.bin`. Project/look validation is performed at
// both encode and decode boundaries, so a package cannot load a scene that
// references a look absent from the accompanying project document.
ShowEncodeResult encode_show_program(
    const ShowProgram& program,
    const std::set<std::string>& available_look_ids);

ShowDecodeResult decode_show_program(
    const std::vector<std::uint8_t>& bytes,
    const std::set<std::string>& available_look_ids);

}  // namespace aeyla::show
