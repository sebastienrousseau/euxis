/// @file
/// @brief Pipfile.lock parser (Pipenv).
///
/// Pipfile.lock structure:
///
///   {
///     "_meta": { "sources": [...], "requires": {...}, "pipfile-spec": 6 },
///     "default": {
///       "django": {
///         "hashes": ["sha256:..."],
///         "version": "==5.0.4"
///       }
///     },
///     "develop": { ... }
///   }
///
/// Version strings come with the comparator prefix (`==5.0.4`); we
/// strip the leading `==` because the canonical purl version is the
/// version string verbatim, and downstream consumers don't expect
/// the operator.
#pragma once

#include "euxis/sca/manifest.hpp"

namespace euxis::sca {

[[nodiscard]] auto parse_pipfile_lock(const std::string& contents,
                                      const std::filesystem::path& source = {})
    -> std::expected<ParsedManifest, ParseError>;

[[nodiscard]] auto parse_pipfile_lock_file(const std::filesystem::path& path)
    -> std::expected<ParsedManifest, ParseError>;

} // namespace euxis::sca
