/// @file
/// @brief LSP diagnostics bridge — converts verdict findings to LSP Diagnostic objects.
#pragma once

#include "euxis/cli/sarif.hpp"

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace euxis::cli {

/// LSP Diagnostic severity levels.
enum class LspSeverity { Error = 1, Warning = 2, Information = 3, Hint = 4 };

/// A single LSP Diagnostic.
struct LspDiagnostic {
    std::string file_path;
    int start_line{0};
    int start_char{0};
    int end_line{0};
    int end_char{0};
    LspSeverity severity{LspSeverity::Warning};
    std::string message;
    std::string source;      // "euxis"
    std::string code;        // rule_id
};

/// Convert verdict findings to LSP-compatible diagnostics JSON.
/// Groups diagnostics by file URI for textDocument/publishDiagnostics.
[[nodiscard]] auto findings_to_lsp_diagnostics(
    const std::vector<SarifFinding>& findings) -> nlohmann::json;

/// Convert a single SarifFinding to an LspDiagnostic.
[[nodiscard]] auto sarif_to_lsp_diagnostic(const SarifFinding& finding) -> LspDiagnostic;

/// Serialize an LspDiagnostic to JSON.
[[nodiscard]] auto diagnostic_to_json(const LspDiagnostic& diag) -> nlohmann::json;

} // namespace euxis::cli
