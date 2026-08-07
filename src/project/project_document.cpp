#include "project/project_document.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <iomanip>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <variant>

namespace aeyla::project {
namespace {

constexpr std::size_t kMaximumJsonBytes = 4U * 1024U * 1024U;
constexpr std::size_t kMaximumJsonDepth = 64U;
constexpr std::size_t kMaximumJsonNodes = 100000U;
constexpr std::size_t kMaximumStringBytes = 65536U;
constexpr std::size_t kExpectedFixtureCount = 14U;
constexpr std::uint16_t kMaximumUniverse = 32767U;

using JsonArray = std::vector<struct JsonValue>;
using JsonObject = std::map<std::string, struct JsonValue>;

struct JsonValue {
  using Storage = std::variant<std::nullptr_t, bool, double, std::string,
                               JsonArray, JsonObject>;
  Storage storage{nullptr};

  [[nodiscard]] const JsonObject* object() const {
    return std::get_if<JsonObject>(&storage);
  }
  [[nodiscard]] const JsonArray* array() const {
    return std::get_if<JsonArray>(&storage);
  }
  [[nodiscard]] const std::string* string() const {
    return std::get_if<std::string>(&storage);
  }
  [[nodiscard]] const double* number() const {
    return std::get_if<double>(&storage);
  }
  [[nodiscard]] const bool* boolean() const {
    return std::get_if<bool>(&storage);
  }
};

class JsonParser final {
 public:
  explicit JsonParser(std::string_view input) : input_(input) {}

  std::optional<JsonValue> parse(std::string& error) {
    if (input_.size() > kMaximumJsonBytes) {
      error = "JSON exceeds the 4 MiB project-state limit";
      return std::nullopt;
    }

    skip_whitespace();
    auto value = parse_value(0U);
    if (!value.has_value()) {
      error = error_;
      return std::nullopt;
    }

    skip_whitespace();
    if (position_ != input_.size()) {
      fail("unexpected trailing data");
      error = error_;
      return std::nullopt;
    }

    return value;
  }

 private:
  std::optional<JsonValue> parse_value(std::size_t depth) {
    if (depth > kMaximumJsonDepth) return fail("maximum JSON depth exceeded");
    if (++nodes_ > kMaximumJsonNodes) return fail("maximum JSON node count exceeded");
    if (position_ >= input_.size()) return fail("unexpected end of JSON");

    const char ch = input_[position_];
    if (ch == '{') return parse_object(depth + 1U);
    if (ch == '[') return parse_array(depth + 1U);
    if (ch == '"') {
      auto value = parse_string();
      if (!value.has_value()) return std::nullopt;
      return JsonValue{std::move(*value)};
    }
    if (ch == 't') return parse_literal("true", JsonValue{true});
    if (ch == 'f') return parse_literal("false", JsonValue{false});
    if (ch == 'n') return parse_literal("null", JsonValue{nullptr});
    if (ch == '-' || (ch >= '0' && ch <= '9')) return parse_number();
    return fail("unexpected token");
  }

  std::optional<JsonValue> parse_object(std::size_t depth) {
    ++position_;
    skip_whitespace();
    JsonObject object;
    if (consume('}')) return JsonValue{std::move(object)};

    while (true) {
      if (position_ >= input_.size() || input_[position_] != '"')
        return fail("object key must be a string");
      auto key = parse_string();
      if (!key.has_value()) return std::nullopt;
      skip_whitespace();
      if (!consume(':')) return fail("expected ':' after object key");
      skip_whitespace();
      auto value = parse_value(depth);
      if (!value.has_value()) return std::nullopt;
      if (!object.emplace(*key, std::move(*value)).second)
        return fail("duplicate object key");
      skip_whitespace();
      if (consume('}')) break;
      if (!consume(',')) return fail("expected ',' or '}' in object");
      skip_whitespace();
    }
    return JsonValue{std::move(object)};
  }

  std::optional<JsonValue> parse_array(std::size_t depth) {
    ++position_;
    skip_whitespace();
    JsonArray array;
    if (consume(']')) return JsonValue{std::move(array)};

    while (true) {
      auto value = parse_value(depth);
      if (!value.has_value()) return std::nullopt;
      array.push_back(std::move(*value));
      skip_whitespace();
      if (consume(']')) break;
      if (!consume(',')) return fail("expected ',' or ']' in array");
      skip_whitespace();
    }
    return JsonValue{std::move(array)};
  }

  static int hex_value(char ch) {
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'a' && ch <= 'f') return 10 + ch - 'a';
    if (ch >= 'A' && ch <= 'F') return 10 + ch - 'A';
    return -1;
  }

  std::optional<std::uint32_t> parse_hex_quad() {
    if (position_ + 4U > input_.size()) {
      fail("truncated unicode escape");
      return std::nullopt;
    }
    std::uint32_t value = 0;
    for (int index = 0; index < 4; ++index) {
      const int digit = hex_value(input_[position_++]);
      if (digit < 0) {
        fail("invalid unicode escape");
        return std::nullopt;
      }
      value = (value << 4U) | static_cast<std::uint32_t>(digit);
    }
    return value;
  }

  bool append_utf8(std::string& output, std::uint32_t codepoint) {
    if (codepoint > 0x10FFFFU ||
        (codepoint >= 0xD800U && codepoint <= 0xDFFFU)) {
      fail("invalid unicode codepoint");
      return false;
    }
    if (codepoint <= 0x7FU) {
      output.push_back(static_cast<char>(codepoint));
    } else if (codepoint <= 0x7FFU) {
      output.push_back(static_cast<char>(0xC0U | (codepoint >> 6U)));
      output.push_back(static_cast<char>(0x80U | (codepoint & 0x3FU)));
    } else if (codepoint <= 0xFFFFU) {
      output.push_back(static_cast<char>(0xE0U | (codepoint >> 12U)));
      output.push_back(static_cast<char>(0x80U | ((codepoint >> 6U) & 0x3FU)));
      output.push_back(static_cast<char>(0x80U | (codepoint & 0x3FU)));
    } else {
      output.push_back(static_cast<char>(0xF0U | (codepoint >> 18U)));
      output.push_back(static_cast<char>(0x80U | ((codepoint >> 12U) & 0x3FU)));
      output.push_back(static_cast<char>(0x80U | ((codepoint >> 6U) & 0x3FU)));
      output.push_back(static_cast<char>(0x80U | (codepoint & 0x3FU)));
    }
    return output.size() <= kMaximumStringBytes;
  }

  std::optional<std::string> parse_string() {
    if (!consume('"')) return fail_string("expected string");
    std::string output;
    while (position_ < input_.size()) {
      const unsigned char ch = static_cast<unsigned char>(input_[position_++]);
      if (ch == '"') return output;
      if (ch < 0x20U) return fail_string("unescaped control character in string");
      if (ch != '\\') {
        output.push_back(static_cast<char>(ch));
      } else {
        if (position_ >= input_.size()) return fail_string("truncated string escape");
        const char escape = input_[position_++];
        switch (escape) {
          case '"': output.push_back('"'); break;
          case '\\': output.push_back('\\'); break;
          case '/': output.push_back('/'); break;
          case 'b': output.push_back('\b'); break;
          case 'f': output.push_back('\f'); break;
          case 'n': output.push_back('\n'); break;
          case 'r': output.push_back('\r'); break;
          case 't': output.push_back('\t'); break;
          case 'u': {
            auto first = parse_hex_quad();
            if (!first.has_value()) return std::nullopt;
            std::uint32_t codepoint = *first;
            if (codepoint >= 0xD800U && codepoint <= 0xDBFFU) {
              if (position_ + 2U > input_.size() || input_[position_] != '\\' ||
                  input_[position_ + 1U] != 'u')
                return fail_string("high surrogate without low surrogate");
              position_ += 2U;
              auto second = parse_hex_quad();
              if (!second.has_value() || *second < 0xDC00U || *second > 0xDFFFU)
                return fail_string("invalid low surrogate");
              codepoint = 0x10000U + ((codepoint - 0xD800U) << 10U) +
                          (*second - 0xDC00U);
            } else if (codepoint >= 0xDC00U && codepoint <= 0xDFFFU) {
              return fail_string("unexpected low surrogate");
            }
            if (!append_utf8(output, codepoint))
              return fail_string("decoded string exceeds size limit");
            break;
          }
          default: return fail_string("invalid string escape");
        }
      }
      if (output.size() > kMaximumStringBytes)
        return fail_string("string exceeds size limit");
    }
    return fail_string("unterminated string");
  }

  std::optional<JsonValue> parse_number() {
    const std::size_t start = position_;
    if (consume('-') && position_ >= input_.size()) return fail("truncated number");

    if (consume('0')) {
      if (position_ < input_.size() && input_[position_] >= '0' && input_[position_] <= '9')
        return fail("leading zero in number");
    } else {
      if (position_ >= input_.size() || input_[position_] < '1' || input_[position_] > '9')
        return fail("invalid number");
      while (position_ < input_.size() && input_[position_] >= '0' && input_[position_] <= '9')
        ++position_;
    }

    if (consume('.')) {
      if (position_ >= input_.size() || input_[position_] < '0' || input_[position_] > '9')
        return fail("fraction requires digits");
      while (position_ < input_.size() && input_[position_] >= '0' && input_[position_] <= '9')
        ++position_;
    }

    if (position_ < input_.size() && (input_[position_] == 'e' || input_[position_] == 'E')) {
      ++position_;
      if (position_ < input_.size() && (input_[position_] == '+' || input_[position_] == '-'))
        ++position_;
      if (position_ >= input_.size() || input_[position_] < '0' || input_[position_] > '9')
        return fail("exponent requires digits");
      while (position_ < input_.size() && input_[position_] >= '0' && input_[position_] <= '9')
        ++position_;
    }

    const std::string token(input_.substr(start, position_ - start));
    char* end = nullptr;
    errno = 0;
    const double value = std::strtod(token.c_str(), &end);
    if (errno == ERANGE || end != token.c_str() + token.size() || !std::isfinite(value))
      return fail("invalid or non-finite number");
    return JsonValue{value};
  }

  std::optional<JsonValue> parse_literal(std::string_view literal, JsonValue value) {
    if (input_.substr(position_, literal.size()) != literal)
      return fail("invalid literal");
    position_ += literal.size();
    return value;
  }

  void skip_whitespace() {
    while (position_ < input_.size()) {
      const char ch = input_[position_];
      if (ch != ' ' && ch != '\t' && ch != '\n' && ch != '\r') break;
      ++position_;
    }
  }

  bool consume(char expected) {
    if (position_ < input_.size() && input_[position_] == expected) {
      ++position_;
      return true;
    }
    return false;
  }

  std::optional<JsonValue> fail(std::string message) {
    if (error_.empty()) {
      error_ = std::move(message) + " at byte " + std::to_string(position_);
    }
    return std::nullopt;
  }

  std::optional<std::string> fail_string(std::string message) {
    fail(std::move(message));
    return std::nullopt;
  }

  std::string_view input_;
  std::size_t position_{0};
  std::size_t nodes_{0};
  std::string error_;
};

void add(ProjectValidation& validation, DiagnosticSeverity severity,
         std::string path, std::string message) {
  validation.diagnostics.push_back(
      {severity, std::move(path), std::move(message)});
}

void add_error(ProjectValidation& validation, std::string path, std::string message) {
  add(validation, DiagnosticSeverity::error, std::move(path), std::move(message));
}

void add_warning(ProjectValidation& validation, std::string path, std::string message) {
  add(validation, DiagnosticSeverity::warning, std::move(path), std::move(message));
}

const JsonValue* member(const JsonObject& object, std::string_view name) {
  const auto found = object.find(std::string(name));
  return found == object.end() ? nullptr : &found->second;
}

const JsonObject* require_object(const JsonObject& object, std::string_view name,
                                 std::string_view path, ProjectValidation& validation) {
  const JsonValue* value = member(object, name);
  if (value == nullptr || value->object() == nullptr) {
    add_error(validation, std::string(path), "required object is missing or invalid");
    return nullptr;
  }
  return value->object();
}

const JsonArray* require_array(const JsonObject& object, std::string_view name,
                               std::string_view path, ProjectValidation& validation) {
  const JsonValue* value = member(object, name);
  if (value == nullptr || value->array() == nullptr) {
    add_error(validation, std::string(path), "required array is missing or invalid");
    return nullptr;
  }
  return value->array();
}

std::optional<std::string> require_string(const JsonObject& object, std::string_view name,
                                          std::string_view path,
                                          ProjectValidation& validation) {
  const JsonValue* value = member(object, name);
  if (value == nullptr || value->string() == nullptr) {
    add_error(validation, std::string(path), "required string is missing or invalid");
    return std::nullopt;
  }
  return *value->string();
}

std::optional<bool> require_bool(const JsonObject& object, std::string_view name,
                                 std::string_view path, ProjectValidation& validation) {
  const JsonValue* value = member(object, name);
  if (value == nullptr || value->boolean() == nullptr) {
    add_error(validation, std::string(path), "required boolean is missing or invalid");
    return std::nullopt;
  }
  return *value->boolean();
}

std::optional<double> require_number(const JsonObject& object, std::string_view name,
                                     std::string_view path,
                                     ProjectValidation& validation) {
  const JsonValue* value = member(object, name);
  if (value == nullptr || value->number() == nullptr) {
    add_error(validation, std::string(path), "required number is missing or invalid");
    return std::nullopt;
  }
  return *value->number();
}

std::optional<std::uint16_t> require_u16(const JsonObject& object, std::string_view name,
                                         std::string_view path, std::uint16_t maximum,
                                         ProjectValidation& validation) {
  const auto value = require_number(object, name, path, validation);
  if (!value.has_value()) return std::nullopt;
  if (*value < 0.0 || *value > static_cast<double>(maximum) || std::floor(*value) != *value) {
    add_error(validation, std::string(path), "integer is outside the supported range");
    return std::nullopt;
  }
  return static_cast<std::uint16_t>(*value);
}

std::optional<float> require_unit_float(const JsonObject& object, std::string_view name,
                                        std::string_view path,
                                        ProjectValidation& validation) {
  const auto value = require_number(object, name, path, validation);
  if (!value.has_value()) return std::nullopt;
  if (!std::isfinite(*value) || *value < 0.0 || *value > 1.0) {
    add_error(validation, std::string(path), "number must be finite and between 0 and 1");
    return std::nullopt;
  }
  return static_cast<float>(*value);
}

bool known_attribute(std::string_view value) {
  static constexpr std::array<std::string_view, 16> names = {
      "dimmer", "shutter", "strobe", "red", "green", "blue", "white",
      "amber", "uv", "lime", "macro", "speed", "reset", "zoom", "fan", "haze"};
  return std::find(names.begin(), names.end(), value) != names.end();
}

bool known_source(std::string_view value) {
  static constexpr std::array<std::string_view, 5> names = {
      "solid", "gradient", "wave", "noise", "chase"};
  return std::find(names.begin(), names.end(), value) != names.end();
}

bool known_backend(std::string_view value) {
  return value == "none" || value == "artnet" || value == "usb-dmx";
}

bool valid_identifier(std::string_view value) {
  if (value.empty() || value.size() > 128U) return false;
  return std::all_of(value.begin(), value.end(), [](unsigned char ch) {
    return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
           (ch >= '0' && ch <= '9') || ch == '-' || ch == '_' || ch == '.';
  });
}

bool valid_sha256(std::string_view value) {
  if (value.size() != 64U) return false;
  return std::all_of(value.begin(), value.end(), [](char ch) {
    return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f');
  });
}

std::string escape_json(std::string_view value) {
  std::ostringstream output;
  output << '"';
  for (unsigned char ch : value) {
    switch (ch) {
      case '"': output << "\\\""; break;
      case '\\': output << "\\\\"; break;
      case '\b': output << "\\b"; break;
      case '\f': output << "\\f"; break;
      case '\n': output << "\\n"; break;
      case '\r': output << "\\r"; break;
      case '\t': output << "\\t"; break;
      default:
        if (ch < 0x20U) {
          output << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                 << static_cast<int>(ch) << std::dec << std::setfill(' ');
        } else {
          output << static_cast<char>(ch);
        }
    }
  }
  output << '"';
  return output.str();
}

std::string number_string(float value) {
  if (value == 0.0F) return "0";
  std::ostringstream output;
  output.imbue(std::locale::classic());
  output << std::setprecision(std::numeric_limits<float>::max_digits10) << value;
  return output.str();
}

void indent(std::ostringstream& output, int level) {
  for (int index = 0; index < level * 2; ++index) output.put(' ');
}

std::optional<ProjectDocument> project_from_json(const JsonObject& root,
                                                 ProjectValidation& validation) {
  ProjectDocument document;

  const JsonObject* schema = require_object(root, "schemaVersion", "schemaVersion", validation);
  if (schema != nullptr) {
    if (auto major = require_u16(*schema, "major", "schemaVersion.major", 65535U, validation))
      document.schema_version.major = *major;
    if (auto minor = require_u16(*schema, "minor", "schemaVersion.minor", 65535U, validation))
      document.schema_version.minor = *minor;
  }

  if (auto value = require_string(root, "appVersion", "appVersion", validation))
    document.app_version = *value;
  if (auto value = require_string(root, "projectId", "projectId", validation))
    document.project_id = *value;
  if (auto value = require_string(root, "createdAt", "createdAt", validation))
    document.created_at = *value;
  if (auto value = require_string(root, "modifiedAt", "modifiedAt", validation))
    document.modified_at = *value;

  const JsonObject* project = require_object(root, "project", "project", validation);
  if (project != nullptr) {
    if (auto value = require_string(*project, "name", "project.name", validation))
      document.name = *value;
  }

  if (const JsonArray* profiles = require_array(root, "fixtureProfiles", "fixtureProfiles", validation)) {
    for (std::size_t index = 0; index < profiles->size(); ++index) {
      const std::string path = "fixtureProfiles[" + std::to_string(index) + "]";
      const JsonObject* value = (*profiles)[index].object();
      if (value == nullptr) {
        add_error(validation, path, "profile must be an object");
        continue;
      }
      FixtureProfileDocument profile;
      if (auto field = require_string(*value, "profileId", path + ".profileId", validation)) profile.profile_id = *field;
      if (auto field = require_string(*value, "displayName", path + ".displayName", validation)) profile.display_name = *field;
      if (auto field = require_string(*value, "manufacturer", path + ".manufacturer", validation)) profile.manufacturer = *field;
      if (auto field = require_string(*value, "model", path + ".model", validation)) profile.model = *field;
      if (auto field = require_string(*value, "modeName", path + ".modeName", validation)) profile.mode_name = *field;
      if (auto field = require_u16(*value, "footprint", path + ".footprint", 512U, validation)) profile.footprint = *field;

      if (const JsonArray* channels = require_array(*value, "channels", path + ".channels", validation)) {
        for (std::size_t channel_index = 0; channel_index < channels->size(); ++channel_index) {
          const std::string channel_path = path + ".channels[" + std::to_string(channel_index) + "]";
          const JsonObject* channel_value = (*channels)[channel_index].object();
          if (channel_value == nullptr) {
            add_error(validation, channel_path, "channel must be an object");
            continue;
          }
          ChannelDocument channel;
          if (auto field = require_u16(*channel_value, "slot", channel_path + ".slot", 512U, validation)) channel.slot = *field;
          if (auto field = require_string(*channel_value, "attribute", channel_path + ".attribute", validation)) channel.attribute = *field;
          profile.channels.push_back(std::move(channel));
        }
      }

      if (const JsonArray* tags = require_array(*value, "tags", path + ".tags", validation)) {
        for (std::size_t tag_index = 0; tag_index < tags->size(); ++tag_index) {
          const auto* tag = (*tags)[tag_index].string();
          if (tag == nullptr) {
            add_error(validation, path + ".tags[" + std::to_string(tag_index) + "]",
                      "tag must be a string");
          } else {
            profile.tags.push_back(*tag);
          }
        }
      }
      document.fixture_profiles.push_back(std::move(profile));
    }
  }

  const JsonObject* rig = require_object(root, "rig", "rig", validation);
  if (rig != nullptr) {
    if (const JsonArray* fixtures = require_array(*rig, "fixtures", "rig.fixtures", validation)) {
      for (std::size_t index = 0; index < fixtures->size(); ++index) {
        const std::string path = "rig.fixtures[" + std::to_string(index) + "]";
        const JsonObject* value = (*fixtures)[index].object();
        if (value == nullptr) {
          add_error(validation, path, "fixture must be an object");
          continue;
        }
        FixtureDocument fixture;
        if (auto field = require_string(*value, "logicalFixtureId", path + ".logicalFixtureId", validation)) fixture.logical_fixture_id = *field;
        if (auto field = require_string(*value, "fixtureProfileId", path + ".fixtureProfileId", validation)) fixture.fixture_profile_id = *field;
        if (auto field = require_u16(*value, "universe", path + ".universe", kMaximumUniverse, validation)) fixture.universe = *field;
        if (auto field = require_u16(*value, "address", path + ".address", 512U, validation)) fixture.address = *field;
        if (auto field = require_bool(*value, "enabled", path + ".enabled", validation)) fixture.enabled = *field;
        if (auto field = require_unit_float(*value, "x", path + ".x", validation)) fixture.x = *field;
        if (auto field = require_unit_float(*value, "y", path + ".y", validation)) fixture.y = *field;
        document.fixtures.push_back(std::move(fixture));
      }
    }
  }

  if (const JsonArray* looks = require_array(root, "looks", "looks", validation)) {
    for (std::size_t index = 0; index < looks->size(); ++index) {
      const std::string path = "looks[" + std::to_string(index) + "]";
      const JsonObject* value = (*looks)[index].object();
      if (value == nullptr) {
        add_error(validation, path, "look must be an object");
        continue;
      }
      LookDocument look;
      if (auto field = require_string(*value, "lookId", path + ".lookId", validation)) look.look_id = *field;
      if (auto field = require_string(*value, "name", path + ".name", validation)) look.name = *field;
      if (auto field = require_string(*value, "source", path + ".source", validation)) look.source = *field;
      document.looks.push_back(std::move(look));
    }
  }

  const JsonObject* visual = require_object(root, "visual", "visual", validation);
  if (visual != nullptr) {
    if (auto value = require_string(*visual, "activeLookId", "visual.activeLookId", validation)) document.visual.active_look_id = *value;
    if (auto value = require_unit_float(*visual, "speed", "visual.speed", validation)) document.visual.speed = *value;
    if (auto value = require_unit_float(*visual, "whiteExtraction", "visual.whiteExtraction", validation)) document.visual.white_extraction = *value;
    if (auto value = require_unit_float(*visual, "amberExtraction", "visual.amberExtraction", validation)) document.visual.amber_extraction = *value;
    if (auto value = require_unit_float(*visual, "uvManual", "visual.uvManual", validation)) document.visual.uv_manual = *value;
  }

  if (const JsonArray* mappings = require_array(root, "midiMappings", "midiMappings", validation)) {
    for (std::size_t index = 0; index < mappings->size(); ++index) {
      const std::string path = "midiMappings[" + std::to_string(index) + "]";
      const JsonObject* value = (*mappings)[index].object();
      if (value == nullptr) {
        add_error(validation, path, "mapping must be an object");
        continue;
      }
      MidiMappingDocument mapping;
      if (auto field = require_u16(*value, "note", path + ".note", 127U, validation)) mapping.note = static_cast<std::uint8_t>(*field);
      if (auto field = require_u16(*value, "executor", path + ".executor", 7U, validation)) mapping.executor = static_cast<std::uint8_t>(*field);
      document.midi_mappings.push_back(mapping);
    }
  }

  const JsonObject* output = require_object(root, "output", "output", validation);
  if (output != nullptr) {
    if (auto value = require_string(*output, "backend", "output.backend", validation)) document.output.backend = *value;
    if (auto value = require_string(*output, "target", "output.target", validation)) document.output.target = *value;
    if (auto value = require_u16(*output, "universe", "output.universe", kMaximumUniverse, validation)) document.output.universe = *value;
    if (auto value = require_bool(*output, "armed", "output.armed", validation)) {
      if (*value) add_warning(validation, "output.armed", "persisted arm state was ignored for safety");
      document.output.armed = false;
    }
  }

  if (const JsonArray* recent = require_array(root, "recentFiles", "recentFiles", validation)) {
    for (std::size_t index = 0; index < recent->size(); ++index) {
      const auto* value = (*recent)[index].string();
      if (value == nullptr) add_error(validation, "recentFiles[" + std::to_string(index) + "]", "entry must be a string");
      else document.recent_files.push_back(*value);
    }
  }

  const JsonObject* ui = require_object(root, "uiState", "uiState", validation);
  if (ui != nullptr) {
    if (auto value = require_string(*ui, "selectedFixtureId", "uiState.selectedFixtureId", validation)) document.ui_state.selected_fixture_id = *value;
    if (auto value = require_string(*ui, "activePanel", "uiState.activePanel", validation)) document.ui_state.active_panel = *value;
  }

  if (const JsonArray* assets = require_array(root, "assets", "assets", validation)) {
    for (std::size_t index = 0; index < assets->size(); ++index) {
      const std::string path = "assets[" + std::to_string(index) + "]";
      const JsonObject* value = (*assets)[index].object();
      if (value == nullptr) {
        add_error(validation, path, "asset must be an object");
        continue;
      }
      AssetDocument asset;
      if (auto field = require_string(*value, "path", path + ".path", validation)) asset.path = *field;
      if (auto field = require_string(*value, "sha256", path + ".sha256", validation)) asset.sha256 = *field;
      document.assets.push_back(std::move(asset));
    }
  }

  const JsonObject* checksums = require_object(root, "checksums", "checksums", validation);
  if (checksums != nullptr) {
    for (const auto& [path, value] : *checksums) {
      if (value.string() == nullptr) add_error(validation, "checksums." + path, "checksum must be a string");
      else document.checksums.emplace(path, *value.string());
    }
  }

  if (!validation.ok()) return std::nullopt;
  return document;
}

}  // namespace

bool ProjectValidation::ok() const noexcept { return error_count() == 0U; }

std::size_t ProjectValidation::error_count() const noexcept {
  return static_cast<std::size_t>(std::count_if(
      diagnostics.begin(), diagnostics.end(), [](const ProjectDiagnostic& diagnostic) {
        return diagnostic.severity == DiagnosticSeverity::error;
      }));
}

std::size_t ProjectValidation::warning_count() const noexcept {
  return static_cast<std::size_t>(std::count_if(
      diagnostics.begin(), diagnostics.end(), [](const ProjectDiagnostic& diagnostic) {
        return diagnostic.severity == DiagnosticSeverity::warning;
      }));
}

bool is_canonical_uuid(std::string_view value) noexcept {
  if (value.size() != 36U) return false;
  for (std::size_t index = 0; index < value.size(); ++index) {
    if (index == 8U || index == 13U || index == 18U || index == 23U) {
      if (value[index] != '-') return false;
      continue;
    }
    const char ch = value[index];
    if (!((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f'))) return false;
  }
  return true;
}

bool is_safe_package_path(std::string_view path) noexcept {
  if (path.empty() || path.size() > 4096U) return false;
  if (path.front() == '/' || path.front() == '\\') return false;
  if (path.size() >= 2U && ((path[0] >= 'A' && path[0] <= 'Z') ||
                            (path[0] >= 'a' && path[0] <= 'z')) && path[1] == ':')
    return false;
  if (path.find('\\') != std::string_view::npos) return false;
  if (path.find('\0') != std::string_view::npos) return false;

  std::size_t start = 0U;
  while (start <= path.size()) {
    const std::size_t end = path.find('/', start);
    const std::string_view segment = path.substr(start, end - start);
    if (segment.empty() || segment == "." || segment == "..") return false;
    if (end == std::string_view::npos) break;
    start = end + 1U;
  }
  return true;
}

ProjectDocument make_default_project_document(std::string project_id,
                                              std::string timestamp_utc) {
  ProjectDocument document;
  document.project_id = std::move(project_id);
  document.created_at = timestamp_utc;
  document.modified_at = std::move(timestamp_utc);
  document.name = "Untitled AEYLA Show";

  FixtureProfileDocument profile;
  profile.profile_id = "aeyla-rgbwaluv-10ch";
  profile.display_name = "AEYLA RGBWALUV 10ch reference";
  profile.manufacturer = "RGB Estudios";
  profile.model = "AEYLA Reference Emitter";
  profile.mode_name = "10 Channel";
  profile.footprint = 10U;
  profile.tags = {"reference", "rgbwaluv", "reorderable"};
  static constexpr std::array<std::string_view, 10> attributes = {
      "dimmer", "shutter", "strobe", "red", "green",
      "blue", "white", "amber", "uv", "lime"};
  for (std::size_t index = 0; index < attributes.size(); ++index) {
    profile.channels.push_back(
        {static_cast<std::uint16_t>(index + 1U), std::string(attributes[index])});
  }
  document.fixture_profiles.push_back(std::move(profile));

  static constexpr std::array<float, 14> x_positions = {
      0.08F, 0.22F, 0.36F, 0.50F, 0.64F, 0.78F, 0.92F,
      0.08F, 0.22F, 0.36F, 0.50F, 0.64F, 0.78F, 0.92F};
  for (std::size_t index = 0; index < kExpectedFixtureCount; ++index) {
    FixtureDocument fixture;
    fixture.logical_fixture_id = "floor-" + std::to_string(index + 1U);
    fixture.fixture_profile_id = "aeyla-rgbwaluv-10ch";
    fixture.universe = 0U;
    fixture.address = static_cast<std::uint16_t>(1U + index * 10U);
    fixture.enabled = index < 10U;
    fixture.x = x_positions[index];
    fixture.y = index < 7U ? 0.25F : 0.75F;
    document.fixtures.push_back(std::move(fixture));
  }

  document.looks = {
      {"look-solid", "Solid", "solid"},
      {"look-gradient", "Gradient", "gradient"},
      {"look-wave", "Cold Wave", "wave"},
      {"look-noise", "Noise", "noise"},
      {"look-chase", "Chase", "chase"},
  };
  document.visual.active_look_id = "look-gradient";

  for (std::uint8_t executor = 0U; executor < 8U; ++executor) {
    document.midi_mappings.push_back(
        {static_cast<std::uint8_t>(36U + executor), executor});
  }

  document.output.backend = "none";
  document.output.target.clear();
  document.output.universe = 0U;
  document.output.armed = false;
  document.ui_state.selected_fixture_id = "floor-1";
  document.ui_state.active_panel = "editor";
  return document;
}

ProjectValidation validate_project_document(const ProjectDocument& document) {
  ProjectValidation validation;

  if (document.schema_version.major != 1U)
    add_error(validation, "schemaVersion.major", "unsupported project schema major version");
  if (document.app_version.empty() || document.app_version.size() > 64U)
    add_error(validation, "appVersion", "app version must contain 1 to 64 bytes");
  if (!is_canonical_uuid(document.project_id))
    add_error(validation, "projectId", "project ID must be a lowercase canonical UUID");
  if (document.created_at.empty()) add_error(validation, "createdAt", "timestamp is required");
  if (document.modified_at.empty()) add_error(validation, "modifiedAt", "timestamp is required");
  if (document.name.empty() || document.name.size() > 256U)
    add_error(validation, "project.name", "project name must contain 1 to 256 bytes");

  std::map<std::string, std::uint16_t> profile_footprints;
  std::set<std::string> profile_ids;
  for (std::size_t index = 0; index < document.fixture_profiles.size(); ++index) {
    const auto& profile = document.fixture_profiles[index];
    const std::string path = "fixtureProfiles[" + std::to_string(index) + "]";
    if (!valid_identifier(profile.profile_id)) add_error(validation, path + ".profileId", "invalid profile identifier");
    if (!profile_ids.insert(profile.profile_id).second) add_error(validation, path + ".profileId", "duplicate profile identifier");
    if (profile.display_name.empty()) add_error(validation, path + ".displayName", "display name is required");
    if (profile.footprint == 0U || profile.footprint > 512U) add_error(validation, path + ".footprint", "footprint must be between 1 and 512");

    std::set<std::uint16_t> slots;
    for (std::size_t channel_index = 0; channel_index < profile.channels.size(); ++channel_index) {
      const auto& channel = profile.channels[channel_index];
      const std::string channel_path = path + ".channels[" + std::to_string(channel_index) + "]";
      if (channel.slot == 0U || channel.slot > profile.footprint) add_error(validation, channel_path + ".slot", "channel slot falls outside fixture footprint");
      if (!slots.insert(channel.slot).second) add_error(validation, channel_path + ".slot", "duplicate channel slot");
      if (!known_attribute(channel.attribute)) add_error(validation, channel_path + ".attribute", "unknown semantic attribute");
    }
    if (profile.channels.empty()) add_error(validation, path + ".channels", "at least one channel is required");
    profile_footprints[profile.profile_id] = profile.footprint;
  }
  if (document.fixture_profiles.empty()) add_error(validation, "fixtureProfiles", "at least one embedded profile is required");

  if (document.fixtures.size() != kExpectedFixtureCount)
    add_error(validation, "rig.fixtures", "Alpha 0.3 requires exactly 14 logical positions");

  std::set<std::string> fixture_ids;
  std::map<std::uint16_t, std::array<std::string, 513>> occupancy;
  for (std::size_t index = 0; index < document.fixtures.size(); ++index) {
    const auto& fixture = document.fixtures[index];
    const std::string path = "rig.fixtures[" + std::to_string(index) + "]";
    if (!valid_identifier(fixture.logical_fixture_id)) add_error(validation, path + ".logicalFixtureId", "invalid logical fixture identifier");
    if (!fixture_ids.insert(fixture.logical_fixture_id).second) add_error(validation, path + ".logicalFixtureId", "duplicate logical fixture identifier");
    const auto profile = profile_footprints.find(fixture.fixture_profile_id);
    if (profile == profile_footprints.end()) {
      add_error(validation, path + ".fixtureProfileId", "fixture references a missing embedded profile");
      continue;
    }
    if (fixture.universe > kMaximumUniverse) add_error(validation, path + ".universe", "universe is outside supported range");
    if (fixture.address == 0U || fixture.address > 512U) {
      add_error(validation, path + ".address", "DMX address must be between 1 and 512");
      continue;
    }
    const std::uint32_t end = static_cast<std::uint32_t>(fixture.address) + profile->second - 1U;
    if (end > 512U) {
      add_error(validation, path + ".address", "fixture footprint exceeds universe boundary");
      continue;
    }
    if (!std::isfinite(fixture.x) || fixture.x < 0.0F || fixture.x > 1.0F ||
        !std::isfinite(fixture.y) || fixture.y < 0.0F || fixture.y > 1.0F)
      add_error(validation, path, "fixture coordinates must be finite normalized values");

    if (fixture.enabled) {
      auto& slots = occupancy[fixture.universe];
      for (std::uint16_t slot = fixture.address; slot <= end; ++slot) {
        if (!slots[slot].empty()) {
          add_error(validation, path + ".address",
                    "DMX patch overlaps fixture '" + slots[slot] + "'");
          break;
        }
        slots[slot] = fixture.logical_fixture_id;
      }
    }
  }

  std::set<std::string> look_ids;
  for (std::size_t index = 0; index < document.looks.size(); ++index) {
    const auto& look = document.looks[index];
    const std::string path = "looks[" + std::to_string(index) + "]";
    if (!valid_identifier(look.look_id)) add_error(validation, path + ".lookId", "invalid look identifier");
    if (!look_ids.insert(look.look_id).second) add_error(validation, path + ".lookId", "duplicate look identifier");
    if (look.name.empty()) add_error(validation, path + ".name", "look name is required");
    if (!known_source(look.source)) add_error(validation, path + ".source", "unsupported visual source");
  }
  if (look_ids.find(document.visual.active_look_id) == look_ids.end())
    add_error(validation, "visual.activeLookId", "active look does not exist");

  const auto valid_unit = [](float value) {
    return std::isfinite(value) && value >= 0.0F && value <= 1.0F;
  };
  if (!valid_unit(document.visual.speed)) add_error(validation, "visual.speed", "value must be between 0 and 1");
  if (!valid_unit(document.visual.white_extraction)) add_error(validation, "visual.whiteExtraction", "value must be between 0 and 1");
  if (!valid_unit(document.visual.amber_extraction)) add_error(validation, "visual.amberExtraction", "value must be between 0 and 1");
  if (!valid_unit(document.visual.uv_manual)) add_error(validation, "visual.uvManual", "value must be between 0 and 1");

  std::set<std::uint8_t> notes;
  std::set<std::uint8_t> executors;
  for (std::size_t index = 0; index < document.midi_mappings.size(); ++index) {
    const auto& mapping = document.midi_mappings[index];
    const std::string path = "midiMappings[" + std::to_string(index) + "]";
    if (mapping.note > 127U) add_error(validation, path + ".note", "MIDI note must be between 0 and 127");
    if (mapping.executor > 7U) add_error(validation, path + ".executor", "executor must be between 0 and 7");
    if (!notes.insert(mapping.note).second) add_error(validation, path + ".note", "duplicate MIDI note mapping");
    if (!executors.insert(mapping.executor).second) add_error(validation, path + ".executor", "duplicate executor mapping");
  }

  if (!known_backend(document.output.backend)) add_error(validation, "output.backend", "unsupported output backend");
  if (document.output.universe > kMaximumUniverse) add_error(validation, "output.universe", "universe is outside supported range");
  if (document.output.backend == "none" && !document.output.target.empty())
    add_warning(validation, "output.target", "target is ignored while backend is 'none'");
  if (document.output.armed)
    add_warning(validation, "output.armed", "arm state is never serialized or restored");

  if (!document.ui_state.selected_fixture_id.empty() &&
      fixture_ids.find(document.ui_state.selected_fixture_id) == fixture_ids.end())
    add_error(validation, "uiState.selectedFixtureId", "selected fixture does not exist");

  for (std::size_t index = 0; index < document.recent_files.size(); ++index) {
    if (!is_safe_package_path(document.recent_files[index]))
      add_error(validation, "recentFiles[" + std::to_string(index) + "]",
                "path must be normalized and relative to the project package");
  }

  std::set<std::string> asset_paths;
  for (std::size_t index = 0; index < document.assets.size(); ++index) {
    const auto& asset = document.assets[index];
    const std::string path = "assets[" + std::to_string(index) + "]";
    if (!is_safe_package_path(asset.path)) add_error(validation, path + ".path", "unsafe package path");
    if (!asset_paths.insert(asset.path).second) add_error(validation, path + ".path", "duplicate asset path");
    if (!valid_sha256(asset.sha256)) add_error(validation, path + ".sha256", "SHA-256 must be 64 lowercase hexadecimal characters");
  }

  for (const auto& [path, checksum] : document.checksums) {
    if (!is_safe_package_path(path)) add_error(validation, "checksums." + path, "unsafe package path");
    if (!valid_sha256(checksum)) add_error(validation, "checksums." + path, "SHA-256 must be 64 lowercase hexadecimal characters");
  }

  return validation;
}

std::string serialize_project_document(const ProjectDocument& document) {
  std::ostringstream output;
  output.imbue(std::locale::classic());
  output << "{\n";
  output << "  \"schemaVersion\": {\"major\": " << document.schema_version.major
         << ", \"minor\": " << document.schema_version.minor << "},\n";
  output << "  \"appVersion\": " << escape_json(document.app_version) << ",\n";
  output << "  \"projectId\": " << escape_json(document.project_id) << ",\n";
  output << "  \"createdAt\": " << escape_json(document.created_at) << ",\n";
  output << "  \"modifiedAt\": " << escape_json(document.modified_at) << ",\n";
  output << "  \"project\": {\"name\": " << escape_json(document.name) << "},\n";

  output << "  \"fixtureProfiles\": [\n";
  for (std::size_t index = 0; index < document.fixture_profiles.size(); ++index) {
    const auto& profile = document.fixture_profiles[index];
    output << "    {\n";
    output << "      \"profileId\": " << escape_json(profile.profile_id) << ",\n";
    output << "      \"displayName\": " << escape_json(profile.display_name) << ",\n";
    output << "      \"manufacturer\": " << escape_json(profile.manufacturer) << ",\n";
    output << "      \"model\": " << escape_json(profile.model) << ",\n";
    output << "      \"modeName\": " << escape_json(profile.mode_name) << ",\n";
    output << "      \"footprint\": " << profile.footprint << ",\n";
    output << "      \"channels\": [\n";
    for (std::size_t channel_index = 0; channel_index < profile.channels.size(); ++channel_index) {
      const auto& channel = profile.channels[channel_index];
      output << "        {\"slot\": " << channel.slot << ", \"attribute\": "
             << escape_json(channel.attribute) << "}";
      output << (channel_index + 1U == profile.channels.size() ? "\n" : ",\n");
    }
    output << "      ],\n";
    output << "      \"tags\": [";
    for (std::size_t tag_index = 0; tag_index < profile.tags.size(); ++tag_index) {
      if (tag_index != 0U) output << ", ";
      output << escape_json(profile.tags[tag_index]);
    }
    output << "]\n";
    output << "    }" << (index + 1U == document.fixture_profiles.size() ? "\n" : ",\n");
  }
  output << "  ],\n";

  output << "  \"rig\": {\n    \"fixtures\": [\n";
  for (std::size_t index = 0; index < document.fixtures.size(); ++index) {
    const auto& fixture = document.fixtures[index];
    output << "      {\"logicalFixtureId\": " << escape_json(fixture.logical_fixture_id)
           << ", \"fixtureProfileId\": " << escape_json(fixture.fixture_profile_id)
           << ", \"universe\": " << fixture.universe
           << ", \"address\": " << fixture.address
           << ", \"enabled\": " << (fixture.enabled ? "true" : "false")
           << ", \"x\": " << number_string(fixture.x)
           << ", \"y\": " << number_string(fixture.y) << "}";
    output << (index + 1U == document.fixtures.size() ? "\n" : ",\n");
  }
  output << "    ]\n  },\n";

  output << "  \"looks\": [\n";
  for (std::size_t index = 0; index < document.looks.size(); ++index) {
    const auto& look = document.looks[index];
    output << "    {\"lookId\": " << escape_json(look.look_id)
           << ", \"name\": " << escape_json(look.name)
           << ", \"source\": " << escape_json(look.source) << "}";
    output << (index + 1U == document.looks.size() ? "\n" : ",\n");
  }
  output << "  ],\n";

  output << "  \"visual\": {\"activeLookId\": " << escape_json(document.visual.active_look_id)
         << ", \"speed\": " << number_string(document.visual.speed)
         << ", \"whiteExtraction\": " << number_string(document.visual.white_extraction)
         << ", \"amberExtraction\": " << number_string(document.visual.amber_extraction)
         << ", \"uvManual\": " << number_string(document.visual.uv_manual) << "},\n";

  output << "  \"midiMappings\": [\n";
  for (std::size_t index = 0; index < document.midi_mappings.size(); ++index) {
    const auto& mapping = document.midi_mappings[index];
    output << "    {\"note\": " << static_cast<unsigned int>(mapping.note)
           << ", \"executor\": " << static_cast<unsigned int>(mapping.executor) << "}";
    output << (index + 1U == document.midi_mappings.size() ? "\n" : ",\n");
  }
  output << "  ],\n";

  output << "  \"output\": {\"backend\": " << escape_json(document.output.backend)
         << ", \"target\": " << escape_json(document.output.target)
         << ", \"universe\": " << document.output.universe
         << ", \"armed\": false},\n";

  output << "  \"recentFiles\": [";
  for (std::size_t index = 0; index < document.recent_files.size(); ++index) {
    if (index != 0U) output << ", ";
    output << escape_json(document.recent_files[index]);
  }
  output << "],\n";

  output << "  \"uiState\": {\"selectedFixtureId\": "
         << escape_json(document.ui_state.selected_fixture_id)
         << ", \"activePanel\": " << escape_json(document.ui_state.active_panel) << "},\n";

  output << "  \"assets\": [\n";
  for (std::size_t index = 0; index < document.assets.size(); ++index) {
    const auto& asset = document.assets[index];
    output << "    {\"path\": " << escape_json(asset.path)
           << ", \"sha256\": " << escape_json(asset.sha256) << "}";
    output << (index + 1U == document.assets.size() ? "\n" : ",\n");
  }
  output << "  ],\n";

  output << "  \"checksums\": {";
  if (!document.checksums.empty()) output << '\n';
  std::size_t checksum_index = 0U;
  for (const auto& [path, checksum] : document.checksums) {
    indent(output, 2);
    output << escape_json(path) << ": " << escape_json(checksum);
    output << (++checksum_index == document.checksums.size() ? "\n" : ",\n");
  }
  if (!document.checksums.empty()) indent(output, 1);
  output << "}\n";
  output << "}\n";
  return output.str();
}

ProjectParseResult deserialize_project_document(std::string_view json) {
  ProjectParseResult result;
  std::string parser_error;
  JsonParser parser(json);
  auto root_value = parser.parse(parser_error);
  if (!root_value.has_value()) {
    add_error(result.validation, "$", parser_error);
    return result;
  }
  const JsonObject* root = root_value->object();
  if (root == nullptr) {
    add_error(result.validation, "$", "project root must be a JSON object");
    return result;
  }

  auto document = project_from_json(*root, result.validation);
  if (!document.has_value()) return result;

  ProjectValidation semantic = validate_project_document(*document);
  result.validation.diagnostics.insert(result.validation.diagnostics.end(),
                                       semantic.diagnostics.begin(), semantic.diagnostics.end());
  if (!result.validation.ok()) return result;
  result.document = std::move(document);
  return result;
}

}  // namespace aeyla::project
