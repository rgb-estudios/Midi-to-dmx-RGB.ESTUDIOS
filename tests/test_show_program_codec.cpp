#include "show/show_program_codec.h"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <set>
#include <string>
#include <vector>

namespace {
int failures = 0;

void check(bool condition, const std::string& message) {
  if (!condition) {
    ++failures;
    std::cerr << "FAIL: " << message << '\n';
  }
}

void legacy_u16(std::vector<std::uint8_t>& bytes, std::uint16_t value) {
  bytes.push_back(static_cast<std::uint8_t>(value & 0xFFU));
  bytes.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
}

void legacy_u32(std::vector<std::uint8_t>& bytes, std::uint32_t value) {
  for (std::size_t index = 0U; index < 4U; ++index)
    bytes.push_back(static_cast<std::uint8_t>((value >> (index * 8U)) & 0xFFU));
}

void legacy_u64(std::vector<std::uint8_t>& bytes, std::uint64_t value) {
  for (std::size_t index = 0U; index < 8U; ++index)
    bytes.push_back(static_cast<std::uint8_t>((value >> (index * 8U)) & 0xFFU));
}

void legacy_string(std::vector<std::uint8_t>& bytes, const std::string& value) {
  legacy_u16(bytes, static_cast<std::uint16_t>(value.size()));
  bytes.insert(bytes.end(), value.begin(), value.end());
}

std::vector<std::uint8_t> make_legacy_show(bool ambiguous) {
  std::vector<std::uint8_t> bytes{'A', 'E', 'Y', 'L', 'A', 'S', 'H', 'W'};
  legacy_u16(bytes, 1U);
  legacy_u16(bytes, 0U);
  legacy_u32(bytes, 1U);
  legacy_string(bytes, "legacy-song");
  legacy_string(bytes, "Legacy Song");
  legacy_u64(bytes, std::bit_cast<std::uint64_t>(120.0));
  bytes.push_back(4U);
  bytes.push_back(4U);
  legacy_u32(bytes, 960U);
  legacy_u64(bytes, 3840U);
  legacy_u32(bytes, 1U);
  legacy_u32(bytes, ambiguous ? 2U : 1U);
  legacy_string(bytes, "legacy-cue");
  legacy_string(bytes, "Legacy Cue");
  legacy_string(bytes, "look-solid");
  legacy_u32(bytes, 250U);
  legacy_u32(bytes, 250U);
  bytes.push_back(0U);
  bytes.push_back(0U);
  const auto append_clip = [&](const std::string& id, std::uint64_t tick,
                               std::uint8_t note) {
    legacy_string(bytes, id);
    legacy_string(bytes, "legacy-cue");
    legacy_u64(bytes, tick);
    legacy_u64(bytes, 960U);
    bytes.push_back(note);
    bytes.push_back(127U);
    bytes.push_back(1U);
  };
  append_clip("legacy-placement-1", 0U, 36U);
  if (ambiguous) append_clip("legacy-placement-2", 1920U, 37U);
  return bytes;
}

aeyla::show::SongProgram make_song(std::size_t number) {
  using namespace aeyla::show;
  SongProgram song;
  song.song_id = "song-" + std::to_string(number);
  song.name = "AEYLA Song " + std::to_string(number);
  song.tempo_bpm = 124.5;
  song.time_signature_numerator = 4U;
  song.time_signature_denominator = 4U;
  song.ppq = 960U;
  song.length_ticks = 8U * song.ppq;
  song.scenes = {
      {"scene-intro", "Intro", "look-gradient", 900U, 150U, false,
       CueBehavior::latch},
      {"scene-red", "Red", "look-solid", 200U, 100U, false,
       CueBehavior::latch},
      {"scene-hit", "White Hit", "look-solid", 0U, 0U, false,
       CueBehavior::momentary},
      {"scene-black", "Blackout", "", 0U, 0U, true,
       CueBehavior::latch},
  };
  song.clips = {
      {"clip-intro", "scene-intro", 0U, song.ppq, 36U, 100U, 1U},
      {"clip-red", "scene-red", 3U * song.ppq, 4U * song.ppq, 37U, 127U, 1U},
      {"clip-hit", "scene-hit", 4U * song.ppq, song.ppq / 4U, 38U, 120U, 2U},
      {"clip-black", "scene-black", 7U * song.ppq, song.ppq, 39U, 127U, 1U},
  };
  song.scenes[0].midi_binding = MidiBinding{36U, 1U};
  song.scenes[1].midi_binding = MidiBinding{37U, 1U};
  song.scenes[2].midi_binding = MidiBinding{38U, 2U};
  song.scenes[3].midi_binding = MidiBinding{39U, 1U};
  return song;
}

aeyla::show::ShowProgram make_show(std::size_t count) {
  aeyla::show::ShowProgram program;
  for (std::size_t index = 0U; index < count; ++index)
    program.songs.push_back(make_song(index + 1U));
  return program;
}

}  // namespace

int main() {
  using namespace aeyla::show;
  const std::set<std::string> looks{"look-gradient", "look-solid"};

  // Authoring and performance readiness are intentionally different. A new
  // project with no songs must survive Save/Open, but cannot enter Show Mode.
  const ShowProgram empty;
  const auto empty_encoded = encode_show_program(empty, looks);
  check(empty_encoded.ok(),
        "empty authoring show must encode so a new project can be saved");
  const auto empty_decoded = decode_show_program(empty_encoded.bytes, looks);
  check(empty_decoded.ok() && empty_decoded.program.has_value() &&
            empty_decoded.program->songs.empty(),
        "empty authoring show must round-trip through show.bin");
  check(!validate_show_program_for_performance(*empty_decoded.program, looks).ok(),
        "empty decoded show must still fail performance preflight");

  const ShowProgram source = make_show(2U);
  const auto encoded = encode_show_program(source, looks);
  check(encoded.ok(), "valid show must encode successfully");
  check(encoded.bytes.size() > 32U &&
            encoded.bytes.size() <= kMaximumEncodedShowBytes,
        "encoded show must remain inside package bounds");

  const auto encoded_again = encode_show_program(source, looks);
  check(encoded_again.ok() && encoded_again.bytes == encoded.bytes,
        "show.bin encoding must be byte-for-byte deterministic");

  const auto decoded = decode_show_program(encoded.bytes, looks);
  check(decoded.ok(), "encoded show must decode successfully");
  check(decoded.program.has_value() && *decoded.program == source,
        "show.bin round-trip must preserve every authored field");

  const auto migrated_legacy = decode_show_program(make_legacy_show(false), looks);
  check(migrated_legacy.ok() &&
            migrated_legacy.program->songs.front().scenes.front()
                .midi_binding == MidiBinding{36U, 1U},
        "legacy 1.0 show must migrate an unambiguous placement mapping to its Cue");
  check(!decode_show_program(make_legacy_show(true), looks).ok(),
        "legacy Cue with ambiguous placement mappings must fail closed");

  // Every strict prefix of a valid file is truncated and must fail closed.
  for (std::size_t size = 0U; size < encoded.bytes.size(); ++size) {
    std::vector<std::uint8_t> prefix(encoded.bytes.begin(),
                                     encoded.bytes.begin() + size);
    const auto truncated = decode_show_program(prefix, looks);
    if (truncated.ok()) {
      check(false, "every truncated show.bin prefix must be rejected");
      break;
    }
  }

  auto wrong_magic = encoded.bytes;
  wrong_magic[0] ^= 0xFFU;
  check(!decode_show_program(wrong_magic, looks).ok(),
        "corrupt show magic must be rejected");

  auto wrong_major = encoded.bytes;
  wrong_major[8] = 2U;
  wrong_major[9] = 0U;
  check(!decode_show_program(wrong_major, looks).ok(),
        "unsupported show major version must be rejected");

  auto future_minor = encoded.bytes;
  future_minor[10] = 2U;
  future_minor[11] = 0U;
  check(!decode_show_program(future_minor, looks).ok(),
        "newer show minor version must not be silently interpreted");

  // A zero-song header is valid only when no stale song payload follows it.
  // Mutating a two-song file this way must therefore fail on trailing bytes.
  auto zero_songs_with_stale_payload = encoded.bytes;
  zero_songs_with_stale_payload[12] = 0U;
  zero_songs_with_stale_payload[13] = 0U;
  zero_songs_with_stale_payload[14] = 0U;
  zero_songs_with_stale_payload[15] = 0U;
  check(!decode_show_program(zero_songs_with_stale_payload, looks).ok(),
        "zero-song header with stale trailing song payload must be rejected");

  auto too_many_songs = encoded.bytes;
  too_many_songs[12] = 16U;
  too_many_songs[13] = 0U;
  too_many_songs[14] = 0U;
  too_many_songs[15] = 0U;
  check(!decode_show_program(too_many_songs, looks).ok(),
        "show.bin claiming a 16th song must be rejected before allocation");

  auto huge_song_id = encoded.bytes;
  huge_song_id[16] = 0xFFU;
  huge_song_id[17] = 0xFFU;
  check(!decode_show_program(huge_song_id, looks).ok(),
        "oversized encoded strings must be rejected before allocation");

  auto trailing = encoded.bytes;
  trailing.push_back(0x42U);
  check(!decode_show_program(trailing, looks).ok(),
        "unexpected trailing bytes must be rejected");

  const std::set<std::string> missing_look{"look-solid"};
  check(!decode_show_program(encoded.bytes, missing_look).ok(),
        "decoded show must be revalidated against project look IDs");

  const auto fifteen = encode_show_program(make_show(15U), looks);
  check(fifteen.ok(), "codec must accept the canonical 15-song maximum");
  check(decode_show_program(fifteen.bytes, looks).ok(),
        "15-song show must survive encode/decode validation");

  const auto sixteen = encode_show_program(make_show(16U), looks);
  check(!sixteen.ok() && sixteen.bytes.empty(),
        "codec must fail closed instead of serializing a 16-song show");

  std::vector<std::uint8_t> oversized(kMaximumEncodedShowBytes + 1U, 0U);
  check(!decode_show_program(oversized, looks).ok(),
        "decoder must reject payloads larger than 4 MiB before parsing");

  if (failures == 0) {
    std::cout << "All AEYLA show-program codec tests passed.\n";
    return EXIT_SUCCESS;
  }
  std::cerr << failures << " test(s) failed.\n";
  return EXIT_FAILURE;
}
