#include "show/show_program_codec.h"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <utility>

namespace aeyla::show {
namespace {

constexpr std::array<std::uint8_t, 8> kMagic = {
    'A', 'E', 'Y', 'L', 'A', 'S', 'H', 'W'};
constexpr std::size_t kMaximumIdBytes = 128U;
constexpr std::size_t kMaximumNameBytes = 256U;

void add_validation_diagnostics(
    const ShowValidation& validation,
    std::vector<ShowCodecDiagnostic>& diagnostics,
    std::size_t offset) {
  for (const auto& diagnostic : validation.diagnostics) {
    if (diagnostic.severity != ShowDiagnosticSeverity::error) continue;
    diagnostics.push_back(
        {offset, diagnostic.path + ": " + diagnostic.message});
  }
}

class Writer final {
 public:
  void u8(std::uint8_t value) { append(&value, 1U); }

  void u16(std::uint16_t value) {
    const std::array<std::uint8_t, 2> bytes = {
        static_cast<std::uint8_t>(value & 0xFFU),
        static_cast<std::uint8_t>((value >> 8U) & 0xFFU)};
    append(bytes.data(), bytes.size());
  }

  void u32(std::uint32_t value) {
    std::array<std::uint8_t, 4> bytes{};
    for (std::size_t index = 0; index < bytes.size(); ++index)
      bytes[index] = static_cast<std::uint8_t>((value >> (index * 8U)) & 0xFFU);
    append(bytes.data(), bytes.size());
  }

  void u64(std::uint64_t value) {
    std::array<std::uint8_t, 8> bytes{};
    for (std::size_t index = 0; index < bytes.size(); ++index)
      bytes[index] = static_cast<std::uint8_t>((value >> (index * 8U)) & 0xFFU);
    append(bytes.data(), bytes.size());
  }

  void f64(double value) { u64(std::bit_cast<std::uint64_t>(value)); }

  void string(std::string_view value) {
    if (!ok_) return;
    if (value.size() > 0xFFFFU) {
      fail("string exceeds codec length field");
      return;
    }
    u16(static_cast<std::uint16_t>(value.size()));
    append(reinterpret_cast<const std::uint8_t*>(value.data()), value.size());
  }

  void magic() { append(kMagic.data(), kMagic.size()); }

  [[nodiscard]] bool ok() const noexcept { return ok_; }
  [[nodiscard]] const std::string& error() const noexcept { return error_; }
  [[nodiscard]] std::vector<std::uint8_t> take() { return std::move(bytes_); }

 private:
  void fail(std::string message) {
    if (!ok_) return;
    ok_ = false;
    error_ = std::move(message);
  }

  void append(const std::uint8_t* data, std::size_t size) {
    if (!ok_ || size == 0U) return;
    if (size > kMaximumEncodedShowBytes ||
        bytes_.size() > kMaximumEncodedShowBytes - size) {
      fail("encoded show exceeds 4 MiB package limit");
      return;
    }
    bytes_.insert(bytes_.end(), data, data + size);
  }

  bool ok_{true};
  std::string error_;
  std::vector<std::uint8_t> bytes_;
};

class Reader final {
 public:
  explicit Reader(std::span<const std::uint8_t> bytes) : bytes_(bytes) {}

  bool magic() {
    if (!require(kMagic.size(), "truncated show magic")) return false;
    for (std::size_t index = 0; index < kMagic.size(); ++index) {
      if (bytes_[offset_ + index] != kMagic[index]) {
        fail("invalid show magic");
        return false;
      }
    }
    offset_ += kMagic.size();
    return true;
  }

  bool u8(std::uint8_t& value) {
    if (!require(1U, "truncated 8-bit field")) return false;
    value = bytes_[offset_++];
    return true;
  }

  bool u16(std::uint16_t& value) {
    if (!require(2U, "truncated 16-bit field")) return false;
    value = static_cast<std::uint16_t>(bytes_[offset_]) |
            static_cast<std::uint16_t>(bytes_[offset_ + 1U] << 8U);
    offset_ += 2U;
    return true;
  }

  bool u32(std::uint32_t& value) {
    if (!require(4U, "truncated 32-bit field")) return false;
    value = 0U;
    for (std::size_t index = 0; index < 4U; ++index)
      value |= static_cast<std::uint32_t>(bytes_[offset_ + index]) << (index * 8U);
    offset_ += 4U;
    return true;
  }

  bool u64(std::uint64_t& value) {
    if (!require(8U, "truncated 64-bit field")) return false;
    value = 0U;
    for (std::size_t index = 0; index < 8U; ++index)
      value |= static_cast<std::uint64_t>(bytes_[offset_ + index]) << (index * 8U);
    offset_ += 8U;
    return true;
  }

  bool f64(double& value) {
    std::uint64_t bits = 0U;
    if (!u64(bits)) return false;
    value = std::bit_cast<double>(bits);
    return true;
  }

  bool string(std::string& value, std::size_t maximum,
              const char* field_name) {
    std::uint16_t length = 0U;
    if (!u16(length)) return false;
    if (static_cast<std::size_t>(length) > maximum) {
      fail(std::string(field_name) + " exceeds codec size limit");
      return false;
    }
    if (!require(length, "truncated string payload")) return false;
    value.assign(reinterpret_cast<const char*>(bytes_.data() + offset_), length);
    offset_ += length;
    return true;
  }

  [[nodiscard]] bool ok() const noexcept { return error_.empty(); }
  [[nodiscard]] std::size_t offset() const noexcept { return offset_; }
  [[nodiscard]] std::size_t remaining() const noexcept {
    return offset_ <= bytes_.size() ? bytes_.size() - offset_ : 0U;
  }
  [[nodiscard]] const std::string& error() const noexcept { return error_; }

 private:
  bool require(std::size_t count, const char* message) {
    if (!ok()) return false;
    if (count > remaining()) {
      fail(message);
      return false;
    }
    return true;
  }

  void fail(std::string message) {
    if (!error_.empty()) return;
    error_ = std::move(message);
  }

  std::span<const std::uint8_t> bytes_;
  std::size_t offset_{0U};
  std::string error_;
};

bool decode_scene(Reader& reader, SceneDefinition& scene) {
  if (!reader.string(scene.scene_id, kMaximumIdBytes, "scene ID") ||
      !reader.string(scene.name, kMaximumNameBytes, "scene name") ||
      !reader.string(scene.look_id, kMaximumIdBytes, "look ID") ||
      !reader.u32(scene.transition_in_ms) ||
      !reader.u32(scene.transition_out_ms))
    return false;

  std::uint8_t blackout = 0U;
  std::uint8_t behavior = 0U;
  if (!reader.u8(blackout) || !reader.u8(behavior)) return false;
  if (blackout > 1U) return false;
  if (behavior > static_cast<std::uint8_t>(CueBehavior::momentary)) return false;
  scene.blackout = blackout != 0U;
  scene.behavior = static_cast<CueBehavior>(behavior);
  return true;
}

bool decode_clip(Reader& reader, MidiSceneClip& clip) {
  return reader.string(clip.clip_id, kMaximumIdBytes, "clip ID") &&
         reader.string(clip.scene_id, kMaximumIdBytes, "scene ID") &&
         reader.u64(clip.start_tick) &&
         reader.u64(clip.duration_ticks) &&
         reader.u8(clip.note) &&
         reader.u8(clip.velocity) &&
         reader.u8(clip.channel);
}

}  // namespace

ShowEncodeResult encode_show_program(
    const ShowProgram& program,
    const std::set<std::string>& available_look_ids) {
  ShowEncodeResult result;
  const ShowValidation validation =
      validate_show_program(program, available_look_ids);
  if (!validation.ok()) {
    add_validation_diagnostics(validation, result.diagnostics, 0U);
    return result;
  }

  Writer writer;
  writer.magic();
  writer.u16(kShowCodecMajor);
  writer.u16(kShowCodecMinor);
  writer.u32(static_cast<std::uint32_t>(program.songs.size()));

  for (const auto& song : program.songs) {
    if (!writer.ok()) break;
    writer.string(song.song_id);
    writer.string(song.name);
    writer.f64(song.tempo_bpm);
    writer.u8(song.time_signature_numerator);
    writer.u8(song.time_signature_denominator);
    writer.u32(song.ppq);
    writer.u64(song.length_ticks);
    writer.u32(static_cast<std::uint32_t>(song.scenes.size()));
    writer.u32(static_cast<std::uint32_t>(song.clips.size()));

    for (const auto& scene : song.scenes) {
      if (!writer.ok()) break;
      writer.string(scene.scene_id);
      writer.string(scene.name);
      writer.string(scene.look_id);
      writer.u32(scene.transition_in_ms);
      writer.u32(scene.transition_out_ms);
      writer.u8(scene.blackout ? 1U : 0U);
      writer.u8(static_cast<std::uint8_t>(scene.behavior));
    }

    for (const auto& clip : song.clips) {
      if (!writer.ok()) break;
      writer.string(clip.clip_id);
      writer.string(clip.scene_id);
      writer.u64(clip.start_tick);
      writer.u64(clip.duration_ticks);
      writer.u8(clip.note);
      writer.u8(clip.velocity);
      writer.u8(clip.channel);
    }
  }

  if (!writer.ok()) {
    result.diagnostics.push_back({0U, writer.error()});
    return result;
  }
  result.bytes = writer.take();
  return result;
}

ShowDecodeResult decode_show_program(
    const std::vector<std::uint8_t>& bytes,
    const std::set<std::string>& available_look_ids) {
  ShowDecodeResult result;
  if (bytes.empty() || bytes.size() > kMaximumEncodedShowBytes) {
    result.diagnostics.push_back(
        {0U, "show.bin size must be between 1 byte and 4 MiB"});
    return result;
  }

  Reader reader(std::span<const std::uint8_t>(bytes.data(), bytes.size()));
  if (!reader.magic()) {
    result.diagnostics.push_back({reader.offset(), reader.error()});
    return result;
  }

  std::uint16_t major = 0U;
  std::uint16_t minor = 0U;
  std::uint32_t song_count = 0U;
  if (!reader.u16(major) || !reader.u16(minor) || !reader.u32(song_count)) {
    result.diagnostics.push_back({reader.offset(), reader.error()});
    return result;
  }
  if (major != kShowCodecMajor) {
    result.diagnostics.push_back(
        {reader.offset(), "unsupported show.bin major version"});
    return result;
  }
  if (minor > kShowCodecMinor) {
    result.diagnostics.push_back(
        {reader.offset(), "show.bin minor version is newer than this runtime"});
    return result;
  }
  if (song_count == 0U || song_count > kMaximumSongs) {
    result.diagnostics.push_back(
        {reader.offset(), "show.bin song count is outside the 1..15 limit"});
    return result;
  }

  ShowProgram program;
  program.songs.reserve(song_count);
  for (std::uint32_t song_index = 0U; song_index < song_count; ++song_index) {
    SongProgram song;
    if (!reader.string(song.song_id, kMaximumIdBytes, "song ID") ||
        !reader.string(song.name, kMaximumNameBytes, "song name") ||
        !reader.f64(song.tempo_bpm) ||
        !reader.u8(song.time_signature_numerator) ||
        !reader.u8(song.time_signature_denominator) ||
        !reader.u32(song.ppq) ||
        !reader.u64(song.length_ticks)) {
      result.diagnostics.push_back({reader.offset(), reader.error()});
      return result;
    }

    std::uint32_t scene_count = 0U;
    std::uint32_t clip_count = 0U;
    if (!reader.u32(scene_count) || !reader.u32(clip_count)) {
      result.diagnostics.push_back({reader.offset(), reader.error()});
      return result;
    }
    if (scene_count == 0U || scene_count > kMaximumScenesPerSong) {
      result.diagnostics.push_back(
          {reader.offset(), "show.bin scene count exceeds runtime bounds"});
      return result;
    }
    if (clip_count > kMaximumClipsPerSong) {
      result.diagnostics.push_back(
          {reader.offset(), "show.bin clip count exceeds runtime bounds"});
      return result;
    }

    song.scenes.reserve(scene_count);
    for (std::uint32_t scene_index = 0U; scene_index < scene_count; ++scene_index) {
      SceneDefinition scene;
      const std::size_t before = reader.offset();
      if (!decode_scene(reader, scene)) {
        result.diagnostics.push_back(
            {reader.offset(), reader.ok() ? "invalid scene enum/boolean value"
                                          : reader.error()});
        return result;
      }
      if (reader.offset() <= before) {
        result.diagnostics.push_back({before, "scene decoder made no progress"});
        return result;
      }
      song.scenes.push_back(std::move(scene));
    }

    song.clips.reserve(clip_count);
    for (std::uint32_t clip_index = 0U; clip_index < clip_count; ++clip_index) {
      MidiSceneClip clip;
      if (!decode_clip(reader, clip)) {
        result.diagnostics.push_back({reader.offset(), reader.error()});
        return result;
      }
      song.clips.push_back(std::move(clip));
    }
    program.songs.push_back(std::move(song));
  }

  if (reader.remaining() != 0U) {
    result.diagnostics.push_back(
        {reader.offset(), "show.bin contains unexpected trailing bytes"});
    return result;
  }

  const ShowValidation validation =
      validate_show_program(program, available_look_ids);
  if (!validation.ok()) {
    add_validation_diagnostics(validation, result.diagnostics, reader.offset());
    return result;
  }

  result.program = std::move(program);
  return result;
}

}  // namespace aeyla::show
