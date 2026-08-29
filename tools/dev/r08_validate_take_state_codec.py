from pathlib import Path


def replace_once(path: Path, old: str, new: str) -> None:
    text = path.read_text(encoding="utf-8")
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"{path}: expected exact block once, found {count}")
    path.write_text(text.replace(old, new, 1), encoding="utf-8")


header = Path("src/runtime/plugin_state.h")
source = Path("src/runtime/plugin_state.cpp")
tests = Path("tests/test_plugin_state.cpp")

replace_once(
    header,
    "inline constexpr std::uint16_t kPluginStateFormatMinor = 2;\n"
    "inline constexpr std::size_t kMaxProjectLocatorBytes = 4096;\n"
    "inline constexpr std::size_t kMaxPluginStateBytes = 64 * 1024;\n"
    "inline constexpr std::size_t kMaxSessionSongBindings = 15;\n"
    "inline constexpr std::size_t kMaxSessionSongIdBytes = 128;",
    "inline constexpr std::uint16_t kPluginStateFormatMinor = 3;\n"
    "inline constexpr std::size_t kMaxProjectLocatorBytes = 4096;\n"
    "inline constexpr std::size_t kMaxTakeLibraryLocatorBytes = 4096;\n"
    "inline constexpr std::size_t kMaxTakeFileNameBytes = 512;\n"
    "inline constexpr std::size_t kMaxPluginStateBytes = 64 * 1024;\n"
    "inline constexpr std::size_t kMaxSessionSongBindings = 15;\n"
    "inline constexpr std::size_t kMaxSessionTakeBindings = 15;\n"
    "inline constexpr std::size_t kMaxSessionSongIdBytes = 128;",
)

replace_once(
    header,
    "struct SessionSongBinding {\n"
    "  std::string song_id;\n"
    "  double host_start_ppq{0.0};\n"
    "  bool operator==(const SessionSongBinding&) const = default;\n"
    "};",
    "struct SessionSongBinding {\n"
    "  std::string song_id;\n"
    "  double host_start_ppq{0.0};\n"
    "  bool operator==(const SessionSongBinding&) const = default;\n"
    "};\n\n"
    "// Portable per-song DMX take selection. Only the basename is persisted so\n"
    "// moving a library between Windows and macOS never bakes a platform path\n"
    "// into every song. 0/0 is the only full-file sentinel.\n"
    "struct SessionTakeBinding {\n"
    "  std::string song_id;\n"
    "  std::string file_name;\n"
    "  std::uint64_t start_frame{0U};\n"
    "  std::uint64_t end_frame_exclusive{0U};\n"
    "  bool operator==(const SessionTakeBinding&) const = default;\n"
    "};",
)

replace_once(
    header,
    "  std::vector<SessionSongBinding> song_bindings{};\n"
    "  ShowMidiMapping show_midi{};",
    "  std::vector<SessionSongBinding> song_bindings{};\n"
    "  ShowMidiMapping show_midi{};\n"
    "  std::string take_library_locator{};\n"
    "  std::vector<SessionTakeBinding> take_bindings{};",
)

replace_once(
    header,
    "  invalid_song_binding,\n  invalid_show_midi_mapping",
    "  invalid_song_binding,\n  invalid_show_midi_mapping,\n  invalid_take_binding",
)

replace_once(
    source,
    "bool is_known_locator_mode(ProjectLocatorMode mode) noexcept {",
    "bool valid_take_file_name(std::string_view value) noexcept {\n"
    "  constexpr std::string_view suffix = \".aeylatake\";\n"
    "  if (value.empty() || value.size() > kMaxTakeFileNameBytes ||\n"
    "      value == \".\" || value == \"..\" ||\n"
    "      value.find('\\0') != std::string_view::npos ||\n"
    "      value.find('/') != std::string_view::npos ||\n"
    "      value.find('\\\\') != std::string_view::npos ||\n"
    "      value.size() < suffix.size() ||\n"
    "      value.substr(value.size() - suffix.size()) != suffix)\n"
    "    return false;\n"
    "  return true;\n"
    "}\n\n"
    "bool is_known_locator_mode(ProjectLocatorMode mode) noexcept {",
)

replace_once(
    source,
    "  if (validate_show_midi_mapping(state.show_midi) !=\n"
    "      ShowMidiMappingError::none)\n"
    "    return PluginStateError::invalid_show_midi_mapping;\n"
    "  return PluginStateError::none;",
    "  if (validate_show_midi_mapping(state.show_midi) !=\n"
    "      ShowMidiMappingError::none)\n"
    "    return PluginStateError::invalid_show_midi_mapping;\n"
    "  if (state.take_library_locator.size() > kMaxTakeLibraryLocatorBytes ||\n"
    "      state.take_library_locator.find('\\0') != std::string::npos)\n"
    "    return PluginStateError::invalid_take_binding;\n"
    "  if (state.take_bindings.size() > kMaxSessionTakeBindings)\n"
    "    return PluginStateError::invalid_take_binding;\n"
    "  std::set<std::string> take_song_ids;\n"
    "  for (const auto& binding : state.take_bindings) {\n"
    "    const bool full_file = binding.start_frame == 0U &&\n"
    "                           binding.end_frame_exclusive == 0U;\n"
    "    const bool valid_trim = binding.end_frame_exclusive > binding.start_frame &&\n"
    "                            binding.end_frame_exclusive - binding.start_frame >= 2U;\n"
    "    if (!valid_binding_id(binding.song_id) ||\n"
    "        !valid_take_file_name(binding.file_name) ||\n"
    "        (!full_file && !valid_trim) ||\n"
    "        !take_song_ids.insert(binding.song_id).second)\n"
    "      return PluginStateError::invalid_take_binding;\n"
    "  }\n"
    "  return PluginStateError::none;",
)

replace_once(
    source,
    "  const auto payload_size = static_cast<std::uint32_t>(\n"
    "      kFixedPayloadSize + locator_size + binding_bytes + 8U);",
    "  std::size_t take_binding_bytes =\n"
    "      4U + state.take_library_locator.size() + 2U;\n"
    "  for (const auto& binding : state.take_bindings)\n"
    "    take_binding_bytes += 2U + binding.song_id.size() + 2U +\n"
    "                          binding.file_name.size() + 8U + 8U;\n"
    "  const auto payload_size = static_cast<std::uint32_t>(\n"
    "      kFixedPayloadSize + locator_size + binding_bytes + 8U +\n"
    "      take_binding_bytes);",
)

replace_once(
    source,
    "    append_u64(result.bytes, pack_show_midi_mapping(state.show_midi));",
    "    append_u64(result.bytes, pack_show_midi_mapping(state.show_midi));\n"
    "    append_u32(result.bytes, static_cast<std::uint32_t>(\n"
    "        state.take_library_locator.size()));\n"
    "    result.bytes.insert(result.bytes.end(), state.take_library_locator.begin(),\n"
    "                        state.take_library_locator.end());\n"
    "    append_u16(result.bytes,\n"
    "               static_cast<std::uint16_t>(state.take_bindings.size()));\n"
    "    for (const auto& binding : state.take_bindings) {\n"
    "      append_u16(result.bytes,\n"
    "                 static_cast<std::uint16_t>(binding.song_id.size()));\n"
    "      result.bytes.insert(result.bytes.end(), binding.song_id.begin(),\n"
    "                          binding.song_id.end());\n"
    "      append_u16(result.bytes,\n"
    "                 static_cast<std::uint16_t>(binding.file_name.size()));\n"
    "      result.bytes.insert(result.bytes.end(), binding.file_name.begin(),\n"
    "                          binding.file_name.end());\n"
    "      append_u64(result.bytes, binding.start_frame);\n"
    "      append_u64(result.bytes, binding.end_frame_exclusive);\n"
    "    }",
)

replace_once(
    source,
    "  // Same-major future minor versions may append fields inside payload_size.\n"
    "  if (offset > payload_end) {",
    "  if (format_minor >= 3U) {\n"
    "    std::uint32_t take_locator_size = 0U;\n"
    "    if (!read_u32(bytes.first(payload_end), offset, take_locator_size) ||\n"
    "        take_locator_size > kMaxTakeLibraryLocatorBytes ||\n"
    "        offset + take_locator_size > payload_end) {\n"
    "      result.error = PluginStateError::invalid_take_binding;\n"
    "      return result;\n"
    "    }\n"
    "    try {\n"
    "      result.state.take_library_locator.assign(\n"
    "          reinterpret_cast<const char*>(bytes.data() + offset),\n"
    "          take_locator_size);\n"
    "    } catch (...) {\n"
    "      result.error = PluginStateError::allocation_failure;\n"
    "      return result;\n"
    "    }\n"
    "    offset += take_locator_size;\n\n"
    "    std::uint16_t take_count = 0U;\n"
    "    if (!read_u16(bytes.first(payload_end), offset, take_count) ||\n"
    "        take_count > kMaxSessionTakeBindings) {\n"
    "      result.error = PluginStateError::invalid_take_binding;\n"
    "      return result;\n"
    "    }\n"
    "    try {\n"
    "      result.state.take_bindings.reserve(take_count);\n"
    "      for (std::uint16_t index = 0U; index < take_count; ++index) {\n"
    "        std::uint16_t id_size = 0U;\n"
    "        std::uint16_t file_size = 0U;\n"
    "        if (!read_u16(bytes.first(payload_end), offset, id_size) ||\n"
    "            id_size == 0U || id_size > kMaxSessionSongIdBytes ||\n"
    "            offset + id_size > payload_end) {\n"
    "          result.error = PluginStateError::invalid_take_binding;\n"
    "          return result;\n"
    "        }\n"
    "        SessionTakeBinding binding;\n"
    "        binding.song_id.assign(\n"
    "            reinterpret_cast<const char*>(bytes.data() + offset), id_size);\n"
    "        offset += id_size;\n"
    "        if (!read_u16(bytes.first(payload_end), offset, file_size) ||\n"
    "            file_size == 0U || file_size > kMaxTakeFileNameBytes ||\n"
    "            offset + file_size + 16U > payload_end) {\n"
    "          result.error = PluginStateError::invalid_take_binding;\n"
    "          return result;\n"
    "        }\n"
    "        binding.file_name.assign(\n"
    "            reinterpret_cast<const char*>(bytes.data() + offset), file_size);\n"
    "        offset += file_size;\n"
    "        if (!read_u64(bytes.first(payload_end), offset, binding.start_frame) ||\n"
    "            !read_u64(bytes.first(payload_end), offset,\n"
    "                      binding.end_frame_exclusive)) {\n"
    "          result.error = PluginStateError::invalid_take_binding;\n"
    "          return result;\n"
    "        }\n"
    "        result.state.take_bindings.push_back(std::move(binding));\n"
    "      }\n"
    "    } catch (...) {\n"
    "      result.error = PluginStateError::allocation_failure;\n"
    "      return result;\n"
    "    }\n"
    "  }\n\n"
    "  // Same-major future minor versions may append fields inside payload_size.\n"
    "  if (offset > payload_end) {",
)

replace_once(
    source,
    "    case PluginStateError::invalid_show_midi_mapping: return \"invalid_show_midi_mapping\";",
    "    case PluginStateError::invalid_show_midi_mapping: return \"invalid_show_midi_mapping\";\n"
    "    case PluginStateError::invalid_take_binding: return \"invalid_take_binding\";",
)

replace_once(
    tests,
    "using aeyla::runtime::SessionSongBinding;",
    "using aeyla::runtime::SessionSongBinding;\n"
    "using aeyla::runtime::SessionTakeBinding;",
)

replace_once(
    tests,
    "  state.show_midi.play_note = 72U;",
    "  state.show_midi.play_note = 72U;\n"
    "  state.take_library_locator = \"C:/AEYLA/Takes\";\n"
    "  state.take_bindings = {\n"
    "      SessionTakeBinding{\"song-intro\", \"Intro_Toma_2.aeylatake\", 44U, 440U},\n"
    "      SessionTakeBinding{\"song-final\", \"Final_Clip.aeylatake\", 0U, 0U},\n"
    "  };",
)

replace_once(
    tests,
    "  check(decoded.state.show_midi == state.show_midi,\n"
    "        \"MIDI Show mapping must survive host-state round trip\");",
    "  check(decoded.state.show_midi == state.show_midi,\n"
    "        \"MIDI Show mapping must survive host-state round trip\");\n"
    "  check(decoded.state.take_library_locator == state.take_library_locator &&\n"
    "            decoded.state.take_bindings == state.take_bindings,\n"
    "        \"take library selection and trims must survive host-state round trip\");",
)

replace_once(
    tests,
    "  // Legacy format 1.0 contains no binding-count tail. It remains readable and",
    "  // Take bindings are basenames only and trims have one unambiguous full-file\n"
    "  // sentinel (0/0). Never admit traversal, arbitrary extensions or one-frame\n"
    "  // / overflow-prone ranges into host state.\n"
    "  {\n"
    "    auto invalid = state;\n"
    "    invalid.take_bindings.front().file_name = \"../escape.aeylatake\";\n"
    "    const auto result = aeyla::runtime::encode_plugin_component_state(invalid);\n"
    "    check(result.error == PluginStateError::invalid_take_binding,\n"
    "          \"take filename traversal must be rejected\");\n"
    "  }\n"
    "  {\n"
    "    auto invalid = state;\n"
    "    invalid.take_bindings.front().file_name = \"Intro_Toma_2.wav\";\n"
    "    const auto result = aeyla::runtime::encode_plugin_component_state(invalid);\n"
    "    check(result.error == PluginStateError::invalid_take_binding,\n"
    "          \"take binding must reference an .aeylatake basename\");\n"
    "  }\n"
    "  {\n"
    "    auto invalid = state;\n"
    "    invalid.take_bindings.front().start_frame = 44U;\n"
    "    invalid.take_bindings.front().end_frame_exclusive = 0U;\n"
    "    const auto result = aeyla::runtime::encode_plugin_component_state(invalid);\n"
    "    check(result.error == PluginStateError::invalid_take_binding,\n"
    "          \"only 0/0 may represent full-file playback\");\n"
    "  }\n"
    "  {\n"
    "    auto invalid = state;\n"
    "    invalid.take_bindings.front().end_frame_exclusive =\n"
    "        invalid.take_bindings.front().start_frame + 1U;\n"
    "    const auto result = aeyla::runtime::encode_plugin_component_state(invalid);\n"
    "    check(result.error == PluginStateError::invalid_take_binding,\n"
    "          \"take trim must leave at least two DMX frames\");\n"
    "  }\n"
    "  {\n"
    "    auto invalid = state;\n"
    "    invalid.take_bindings.front().start_frame =\n"
    "        std::numeric_limits<std::uint64_t>::max();\n"
    "    invalid.take_bindings.front().end_frame_exclusive = 0U;\n"
    "    const auto result = aeyla::runtime::encode_plugin_component_state(invalid);\n"
    "    check(result.error == PluginStateError::invalid_take_binding,\n"
    "          \"take trim validation must not overflow at UINT64_MAX\");\n"
    "  }\n\n"
    "  // Legacy format 1.0 contains no binding-count tail. It remains readable and",
)

replace_once(
    tests,
    "    PluginComponentState legacy_state = state;\n"
    "    legacy_state.song_bindings.clear();\n"
    "    auto bytes = aeyla::runtime::encode_plugin_component_state(legacy_state).bytes;\n"
    "    check(bytes.size() >= 2U, \"current empty-binding state must contain count tail\");\n"
    "    bytes.resize(bytes.size() - 10U);",
    "    PluginComponentState legacy_state = state;\n"
    "    legacy_state.song_bindings.clear();\n"
    "    legacy_state.take_library_locator.clear();\n"
    "    legacy_state.take_bindings.clear();\n"
    "    auto bytes = aeyla::runtime::encode_plugin_component_state(legacy_state).bytes;\n"
    "    check(bytes.size() >= 16U, \"current state must contain 1.1/1.2/1.3 tails\");\n"
    "    bytes.resize(bytes.size() - 16U);",
)
replace_once(tests, "    payload_size -= 10U;", "    payload_size -= 16U;")

replace_once(
    tests,
    "    auto bytes = encoded.bytes;\n"
    "    bytes.resize(bytes.size() - 8U);\n"
    "    bytes[10] = 1U;",
    "    auto legacy11 = state;\n"
    "    legacy11.take_library_locator.clear();\n"
    "    legacy11.take_bindings.clear();\n"
    "    auto bytes = aeyla::runtime::encode_plugin_component_state(legacy11).bytes;\n"
    "    bytes.resize(bytes.size() - 14U);\n"
    "    bytes[10] = 1U;",
)
replace_once(tests, "    payload_size -= 8U;", "    payload_size -= 14U;")
replace_once(
    tests,
    "    auto expected = state;\n    expected.show_midi = {};",
    "    auto expected = legacy11;\n    expected.show_midi = {};",
)

replace_once(
    tests,
    "  // Same-major future minor payloads may append fields and remain readable.",
    "  // R08 format 1.2 contains MIDI Show state but no take-library tail.\n"
    "  {\n"
    "    auto legacy12 = state;\n"
    "    legacy12.take_library_locator.clear();\n"
    "    legacy12.take_bindings.clear();\n"
    "    auto bytes = aeyla::runtime::encode_plugin_component_state(legacy12).bytes;\n"
    "    bytes.resize(bytes.size() - 6U);\n"
    "    bytes[10] = 2U;\n"
    "    bytes[11] = 0U;\n"
    "    std::uint32_t payload_size = static_cast<std::uint32_t>(bytes[12]) |\n"
    "                                 (static_cast<std::uint32_t>(bytes[13]) << 8U) |\n"
    "                                 (static_cast<std::uint32_t>(bytes[14]) << 16U) |\n"
    "                                 (static_cast<std::uint32_t>(bytes[15]) << 24U);\n"
    "    payload_size -= 6U;\n"
    "    bytes[12] = static_cast<std::uint8_t>(payload_size & 0xFFU);\n"
    "    bytes[13] = static_cast<std::uint8_t>((payload_size >> 8U) & 0xFFU);\n"
    "    bytes[14] = static_cast<std::uint8_t>((payload_size >> 16U) & 0xFFU);\n"
    "    bytes[15] = static_cast<std::uint8_t>((payload_size >> 24U) & 0xFFU);\n"
    "    const auto result = aeyla::runtime::decode_plugin_component_state(bytes);\n"
    "    check(result.ok() && result.state == legacy12,\n"
    "          \"legacy 1.2 state must migrate with empty take bindings\");\n"
    "  }\n\n"
    "  // Same-major future minor payloads may append fields and remain readable.",
)

print("R08 hardened take-state codec 1.3 workspace patch applied")
