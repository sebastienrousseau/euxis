/// @file
/// @brief CycloneDX 1.6 JSON emitter.
///
/// Produces the JSON serialisation defined at
/// https://cyclonedx.org/docs/1.6/json/. Validated against
/// `cyclonedx-cli validate --input-format json` round-trip and
/// against the `cyclonedx-bom-1.6.schema.json` published 2025-09.
#pragma once

#include <nlohmann/json.hpp>

#include "euxis/sbom/types.hpp"

namespace euxis::sbom {

/// Convert an SbomDocument to a CycloneDX 1.6 JSON object.
[[nodiscard]] auto to_cyclonedx_1_6(const SbomDocument& doc) -> nlohmann::json;

} // namespace euxis::sbom
