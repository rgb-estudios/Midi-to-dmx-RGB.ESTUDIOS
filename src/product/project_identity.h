#pragma once

#include <string>

namespace aeyla::product {

// Non-realtime helpers for project creation and save metadata.
std::string generate_project_uuid();
std::string current_utc_timestamp();

}  // namespace aeyla::product
