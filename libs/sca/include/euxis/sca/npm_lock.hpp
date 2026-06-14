/// @file
/// @brief npm package-lock.json v3 parser.
///
/// npm package-lock.json v3 shape (the only version we support — v1
/// and v2 are obsolete as of npm 10):
///
///   {
///     "name": "my-app",
///     "version": "1.0.0",
///     "lockfileVersion": 3,
///     "packages": {
///       "": { "name": "my-app", "version": "1.0.0", "dependencies": {...} },
///       "node_modules/lodash": {
///         "version": "4.17.21",
///         "resolved": "https://registry.npmjs.org/...",
///         "integrity": "sha512-..."
///       }
///     }
///   }
///
/// Scoped packages use the `@scope/name` form; we split that into the
/// purl `ns=@scope, name=name` pair so the purl spec is satisfied.
#pragma once

#include "euxis/sca/manifest.hpp"

namespace euxis::sca {

[[nodiscard]] auto parse_npm_lock(const std::string& contents,
                                  const std::filesystem::path& source = {})
    -> std::expected<ParsedManifest, ParseError>;

[[nodiscard]] auto parse_npm_lock_file(const std::filesystem::path& path)
    -> std::expected<ParsedManifest, ParseError>;

} // namespace euxis::sca
