/// @file
/// @brief SPDX 3.0.1 JSON-LD emitter.
///
/// Produces the JSON-LD serialisation defined at
/// https://spdx.github.io/spdx-spec/v3.0.1/. Field set is the
/// SPDX-3.0.1 "minimum SBOM core" with the optional Security profile
/// (SPDX-Security/Vulnerability nodes are produced separately by
/// `openvex.hpp` when present).
#pragma once

#include <nlohmann/json.hpp>

#include "euxis/sbom/types.hpp"

namespace euxis::sbom {

/// Convert an SbomDocument to an SPDX 3.0.1 JSON-LD object.
[[nodiscard]] auto to_spdx_3_0_1(const SbomDocument& doc) -> nlohmann::json;

} // namespace euxis::sbom
