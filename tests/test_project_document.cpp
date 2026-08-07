#include "project/project_document.h"

#include <cstdlib>
#include <iostream>
#include <random>
#include <string>

namespace {
int failures = 0;

void check(bool condition, const std::string& message) {
  if (!condition) {
    ++failures;
    std::cerr << "FAIL: " << message << '\n';
  }
}

bool has_error_path(const aeyla::project::ProjectValidation& validation,
                    const std::string& fragment) {
  for (const auto& diagnostic : validation.diagnostics) {
    if (diagnostic.severity == aeyla::project::DiagnosticSeverity::error &&
        diagnostic.path.find(fragment) != std::string::npos) return true;
  }
  return false;
}
}  // namespace

int main() {
  using namespace aeyla::project;

  constexpr const char* kId = "11111111-2222-4333-8444-555555555555";
  constexpr const char* kTime = "2026-08-07T01:00:00Z";
  ProjectDocument document = make_default_project_document(kId, kTime);
  const ProjectValidation initial = validate_project_document(document);
  check(initial.ok(), "canonical default project must validate");
  check(document.fixtures.size() == 14U, "default project must contain 14 logical positions");

  std::size_t enabled = 0U;
  for (const auto& fixture : document.fixtures) if (fixture.enabled) ++enabled;
  check(enabled == 10U, "default project must activate exactly 10 positions");
  check(document.fixture_profiles.size() == 1U,
        "default project must embed its fixture profile");
  check(document.midi_mappings.size() == 8U,
        "default project must define executor notes 36 through 43");
  check(!document.output.armed, "default project must never start armed");

  document.name = "AEYLA – Ensayo ñ";
  const std::string first_json = serialize_project_document(document);
  const ProjectParseResult parsed = deserialize_project_document(first_json);
  check(parsed.ok(), "serialized default project must parse and validate");
  if (parsed.document.has_value()) {
    check(*parsed.document == document, "project round-trip must preserve authored state");
    check(serialize_project_document(*parsed.document) == first_json,
          "serialization must be byte-deterministic after round-trip");
  }

  ProjectDocument duplicate = document;
  duplicate.fixtures[1].logical_fixture_id = duplicate.fixtures[0].logical_fixture_id;
  ProjectValidation duplicate_validation = validate_project_document(duplicate);
  check(!duplicate_validation.ok() && has_error_path(duplicate_validation, "logicalFixtureId"),
        "duplicate fixture IDs must be rejected");

  ProjectDocument missing_profile = document;
  missing_profile.fixtures[0].fixture_profile_id = "missing-profile";
  ProjectValidation missing_validation = validate_project_document(missing_profile);
  check(!missing_validation.ok() && has_error_path(missing_validation, "fixtureProfileId"),
        "missing embedded profile references must be rejected");

  ProjectDocument overlap = document;
  overlap.fixtures[1].address = overlap.fixtures[0].address;
  ProjectValidation overlap_validation = validate_project_document(overlap);
  check(!overlap_validation.ok() && has_error_path(overlap_validation, "address"),
        "overlapping enabled fixtures must be rejected");

  ProjectDocument overflow = document;
  overflow.fixtures[0].address = 510U;
  ProjectValidation overflow_validation = validate_project_document(overflow);
  check(!overflow_validation.ok() && has_error_path(overflow_validation, "address"),
        "fixture footprint beyond channel 512 must be rejected");

  check(is_safe_package_path("assets/video/intro.mov"),
        "normalized package-relative asset path should be accepted");
  check(!is_safe_package_path("../outside.mov"), "parent traversal must be rejected");
  check(!is_safe_package_path("C:/outside.mov"), "drive-letter path must be rejected");
  check(!is_safe_package_path("/absolute.mov"), "absolute path must be rejected");
  check(!is_safe_package_path("assets\\outside.mov"), "backslash path must be rejected");

  ProjectDocument unsafe_asset = document;
  unsafe_asset.assets.push_back({"../outside.mov", std::string(64U, 'a')});
  check(!validate_project_document(unsafe_asset).ok(),
        "unsafe asset path must invalidate the project");

  std::string armed_json = first_json;
  const std::string safe_token = "\"armed\": false";
  const std::size_t armed_position = armed_json.find(safe_token);
  check(armed_position != std::string::npos, "serialized project must include safe arm token");
  if (armed_position != std::string::npos) {
    armed_json.replace(armed_position, safe_token.size(), "\"armed\": true");
    const ProjectParseResult armed = deserialize_project_document(armed_json);
    check(armed.ok(), "persisted armed=true should be safely downgraded, not crash load");
    check(armed.validation.warning_count() >= 1U,
          "ignored persisted arm must be visible as a warning");
    if (armed.document.has_value())
      check(!armed.document->output.armed, "load must force output disarmed");
  }

  std::string future_minor = first_json;
  const std::string minor_token = "\"minor\": 0";
  const std::size_t minor_position = future_minor.find(minor_token);
  if (minor_position != std::string::npos)
    future_minor.replace(minor_position, minor_token.size(), "\"minor\": 7");
  const ProjectParseResult future = deserialize_project_document(future_minor);
  check(future.ok(), "same-major future-minor project should remain readable");

  std::string unsupported_major = first_json;
  const std::string major_token = "\"major\": 1";
  const std::size_t major_position = unsupported_major.find(major_token);
  if (major_position != std::string::npos)
    unsupported_major.replace(major_position, major_token.size(), "\"major\": 2");
  const ProjectParseResult unsupported = deserialize_project_document(unsupported_major);
  check(!unsupported.ok() && has_error_path(unsupported.validation, "schemaVersion.major"),
        "unsupported schema major must be rejected");

  for (std::size_t length = 0U; length < first_json.size(); length += 31U) {
    const ProjectParseResult truncated = deserialize_project_document(
        std::string_view(first_json.data(), length));
    check(!truncated.ok(), "truncated project prefix must be rejected");
  }

  std::mt19937 generator(0xAE71A03U);
  std::uniform_int_distribution<int> length_distribution(0, 512);
  std::uniform_int_distribution<int> byte_distribution(0, 255);
  for (int iteration = 0; iteration < 2000; ++iteration) {
    std::string malformed(static_cast<std::size_t>(length_distribution(generator)), '\0');
    for (char& value : malformed)
      value = static_cast<char>(byte_distribution(generator));
    try {
      (void) deserialize_project_document(malformed);
    } catch (...) {
      check(false, "malformed project input must not throw across the load boundary");
      break;
    }
  }

  if (failures == 0) {
    std::cout << "All AEYLA project document tests passed.\n";
    return EXIT_SUCCESS;
  }
  std::cerr << failures << " test(s) failed.\n";
  return EXIT_FAILURE;
}
