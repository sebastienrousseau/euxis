/// @file
/// @brief go.sum parser.
///
/// go.sum entries:
///   <module> <version> h1:<base64-sha256>
///   <module> <version>/go.mod h1:<base64-sha256>
///
/// Each module appears twice — once for the zip-archive checksum and
/// once for the go.mod checksum. We dedupe by `(module, version)` and
/// keep the non-`/go.mod` h1 line as the canonical hash.
#pragma once

#include "euxis/sca/manifest.hpp"

namespace euxis::sca {

[[nodiscard]] auto parse_go_sum(const std::string& contents,
                                const std::filesystem::path& source = {})
    -> std::expected<ParsedManifest, ParseError>;

[[nodiscard]] auto parse_go_sum_file(const std::filesystem::path& path)
    -> std::expected<ParsedManifest, ParseError>;

} // namespace euxis::sca
