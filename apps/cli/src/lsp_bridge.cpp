#include "euxis/cli/lsp_bridge.hpp"

#include <unordered_map>

namespace euxis::cli {

auto sarif_to_lsp_diagnostic(const SarifFinding& finding) -> LspDiagnostic {
    LspDiagnostic diag;
    diag.file_path = finding.file_path.empty() ? "unknown" : finding.file_path;
    diag.start_line = finding.line > 0 ? finding.line - 1 : 0;  // LSP is 0-indexed
    diag.start_char = 0;
    diag.end_line = diag.start_line;
    diag.end_char = 0;
    diag.message = finding.message;
    diag.source = "euxis";
    diag.code = finding.rule_id;

    if (finding.level == "error") diag.severity = LspSeverity::Error;
    else if (finding.level == "warning") diag.severity = LspSeverity::Warning;
    else if (finding.level == "note") diag.severity = LspSeverity::Information;
    else diag.severity = LspSeverity::Hint;

    return diag;
}

auto diagnostic_to_json(const LspDiagnostic& diag) -> nlohmann::json {
    nlohmann::json j;
    j["range"]["start"]["line"] = diag.start_line;
    j["range"]["start"]["character"] = diag.start_char;
    j["range"]["end"]["line"] = diag.end_line;
    j["range"]["end"]["character"] = diag.end_char;
    j["severity"] = static_cast<int>(diag.severity);
    j["message"] = diag.message;
    j["source"] = diag.source;
    j["code"] = diag.code;
    return j;
}

auto findings_to_lsp_diagnostics(
    const std::vector<SarifFinding>& findings) -> nlohmann::json {

    // Group by file URI
    std::unordered_map<std::string, nlohmann::json> by_file;
    for (const auto& f : findings) {
        auto diag = sarif_to_lsp_diagnostic(f);
        auto uri = "file://" + diag.file_path;
        if (!by_file.contains(uri)) {
            by_file[uri] = nlohmann::json::array();
        }
        by_file[uri].push_back(diagnostic_to_json(diag));
    }

    // Emit as array of textDocument/publishDiagnostics notifications
    nlohmann::json result = nlohmann::json::array();
    for (const auto& [uri, diags] : by_file) {
        result.push_back({
            {"method", "textDocument/publishDiagnostics"},
            {"params", {
                {"uri", uri},
                {"diagnostics", diags}
            }}
        });
    }
    return result;
}

} // namespace euxis::cli
