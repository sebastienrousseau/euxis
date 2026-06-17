/// @file
/// @brief Cargo.lock parser (Cargo registry / crates.io).
///
/// Cargo.lock uses a TOML subset with deterministic structure:
///   [[package]]
///   name = "serde"
///   version = "1.0.197"
///   source = "registry+https://github.com/rust-lang/crates.io-index"
///   checksum = "..."
///   dependencies = [
///     "serde_derive 1.0.197 (registry+https://github.com/rust-lang/crates.io-index)",
///   ]
///
/// We parse this with a line scanner rather than a full TOML library
/// because the format is fixed and we want to keep libs/sca free of
/// extra build deps. The parser is forward-tolerant: unknown keys are
/// ignored, comments are stripped, and array values may span multiple
/// lines.
#pragma once

#include "euxis/sca/manifest.hpp"

namespace euxis::sca {

/// Parse Cargo.lock content into a ParsedManifest.
[[nodiscard]] auto parse_cargo_lock(const std::string& contents,
                                    const std::filesystem::path& source = {})
    -> std::expected<ParsedManifest, ParseError>;

/// Parse Cargo.lock from a file on disk.
[[nodiscard]] auto parse_cargo_lock_file(const std::filesystem::path& path)
    -> std::expected<ParsedManifest, ParseError>;

} // namespace euxis::sca
