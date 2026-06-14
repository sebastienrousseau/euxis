/// @file
/// @brief Implementation of `euxis scan`.

#include "euxis/cli/cmd/scan.hpp"
#include "euxis/cli/exit_codes.hpp"
#include "euxis/cli/sarif.hpp"

#include <algorithm>
#include <array>
#include <expected>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <print>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

#include <nlohmann/json.hpp>

#include "euxis/cpg/builder.hpp"
#include "euxis/cpg/ddg.hpp"
#include "euxis/parse/parser.hpp"
#include "euxis/parse/types.hpp"
#include "euxis/scan/rule_engine.hpp"
#include "euxis/scan/rule_loader.hpp"
#include "euxis/security/finding.hpp"
#include "euxis/taint/analyzer.hpp"
#include "euxis/taint/builtin_specs.hpp"
#include "euxis/taint/spec.hpp"

namespace euxis::cli::cmd {

namespace {

struct ScanArgs {
    std::filesystem::path target{"."};
    std::vector<std::filesystem::path> rule_files;
    std::filesystem::path sarif_output{"euxis-scan.sarif.json"};
    bool emit_sarif{true};
    bool quiet{false};
    std::optional<euxis::parse::Language> language_filter;
    euxis::security::Severity fail_on{euxis::security::Severity::High};
    std::size_t max_files{10'000};
    /// Whether to run the libs/taint forward-DDG analyzer alongside
    /// the OpenGrep rule engine. Default: on. Findings from both
    /// engines land in the same SARIF stream.
    bool run_taint{true};
};

// Default exclude segments. Matches the libs/sca scanner so behaviour
// is consistent across `euxis sbom` and `euxis scan`.
constexpr std::array kDefaultExcludes = {
    std::string_view{"node_modules"},
    std::string_view{"vendor"},
    std::string_view{".venv"},
    std::string_view{".git"},
    std::string_view{"target"},
    std::string_view{"build"},
    std::string_view{"cmake-build"},
    std::string_view{"dist"},
};

auto parse_severity_token(std::string_view s)
    -> std::optional<euxis::security::Severity> {
    auto sev = euxis::security::parse_severity(std::string{s});
    // parse_severity returns Severity::None for both literal "none"
    // and unrecognised input. Disambiguate.
    if (sev == euxis::security::Severity::None) {
        if (s == "none" || s == "None" || s == "NONE") return sev;
        return std::nullopt;
    }
    return sev;
}

auto parse_args(const std::vector<std::string>& argv)
    -> std::expected<ScanArgs, std::string> {
    ScanArgs a;
    bool target_set = false;
    for (const auto& s : argv) {
        if (s == "--quiet")    { a.quiet = true;        continue; }
        if (s == "--no-sarif") { a.emit_sarif = false;  continue; }
        if (s == "--no-taint") { a.run_taint = false;   continue; }
        if (s == "--taint")    { a.run_taint = true;    continue; }
        if (s.starts_with("--rules=")) {
            a.rule_files.emplace_back(s.substr(8));
            continue;
        }
        if (s.starts_with("--sarif=")) {
            a.sarif_output = s.substr(8);
            a.emit_sarif = true;
            continue;
        }
        if (s.starts_with("--language=")) {
            auto lang = euxis::parse::detect_language_by_extension(
                "." + s.substr(11));
            if (!lang) {
                return std::unexpected(
                    "--language=<x>: unknown language token '" + s.substr(11) + "'");
            }
            a.language_filter = *lang;
            continue;
        }
        if (s.starts_with("--fail-on=")) {
            auto sev = parse_severity_token(s.substr(10));
            if (!sev) return std::unexpected("--fail-on=<sev> must be one of critical | high | medium | low | info | none");
            a.fail_on = *sev;
            continue;
        }
        if (s.starts_with("--max-files=")) {
            try { a.max_files = std::stoul(s.substr(12)); }
            catch (...) {
                return std::unexpected("--max-files=<n>: invalid integer");
            }
            continue;
        }
        if (s == "--help" || s == "-h") return std::unexpected("__help__");
        if (s.starts_with("-")) return std::unexpected("Unknown flag: " + s);
        if (!target_set) { a.target = s; target_set = true; continue; }
        return std::unexpected("Unexpected positional argument: " + s);
    }
    // --rules is required UNLESS the taint analyzer is the only
    // configured engine; the analyzer can stand on its own with
    // builtin_specs() and still produce meaningful findings.
    if (a.rule_files.empty() && !a.run_taint) {
        return std::unexpected(
            "either --rules=<path> or --taint (default) must produce findings");
    }
    return a;
}

void print_help() {
    std::println("Usage: euxis scan [target] --rules=<path> [options]");
    std::println("");
    std::println("Applies OpenGrep / Semgrep-format YAML rule packs to the");
    std::println("target directory (default: cwd). Walks the tree, parses each");
    std::println("supported file, projects the AST to a Code Property Graph, and");
    std::println("runs each rule's pattern against every node's source bytes.");
    std::println("");
    std::println("Options:");
    std::println("  --rules=<path>       YAML rule pack (repeatable; optional if --taint is on)");
    std::println("  --taint              Run the forward-DDG taint analyzer (default: on)");
    std::println("  --no-taint           Disable the taint analyzer");
    std::println("  --sarif=<path>       SARIF 2.1.0 output (default: euxis-scan.sarif.json)");
    std::println("  --no-sarif           Skip SARIF emission (text summary only)");
    std::println("  --language=<lang>    Restrict to one language (c, cpp, rust, ...)");
    std::println("  --fail-on=<sev>      Severity threshold for blocking exit code 3");
    std::println("                       (critical | high | medium | low | info | none; default: high)");
    std::println("  --max-files=<n>      Cap on files scanned (default: 10000)");
    std::println("  --quiet              Print only the summary line");
    std::println("");
    std::println("Exit codes:");
    std::println("  0  no findings at or above --fail-on");
    std::println("  1  infra error");
    std::println("  2  advisory findings (below --fail-on)");
    std::println("  3  blocking findings (at or above --fail-on)");
}

auto should_skip(const std::filesystem::path& relative) -> bool {
    for (const auto& seg : relative) {
        for (auto excl : kDefaultExcludes) {
            if (seg.string() == excl) return true;
        }
    }
    return false;
}

struct ScanContext {
    std::vector<euxis::scan::RulePack> packs;
    std::vector<euxis::security::Finding> findings;
    std::size_t files_scanned{0};
    std::size_t files_skipped{0};
    std::size_t parse_failures{0};
    /// Per-engine aggregate counts surfaced in the summary line so
    /// the user can see at a glance whether the OpenGrep engine
    /// (rule packs) or the taint analyzer contributed each finding.
    std::size_t rule_findings{0};
    std::size_t taint_findings{0};
    std::size_t taint_flows{0};
};

void scan_file(ScanContext& ctx,
               const std::filesystem::path& file,
               const ScanArgs& args) {
    auto lang = euxis::parse::detect_language(file);
    if (!lang) {
        ++ctx.files_skipped;
        return;
    }
    if (args.language_filter && *lang != *args.language_filter) {
        ++ctx.files_skipped;
        return;
    }

    euxis::parse::Parser parser(*lang);
    auto ast = parser.parse_file(file);
    if (!ast) {
        ++ctx.parse_failures;
        return;
    }
    auto graph = euxis::cpg::build(*ast);
    if (!graph) {
        ++ctx.parse_failures;
        return;
    }

    auto file_str = file.string();

    // OpenGrep rule engine — runs whenever any rule packs were
    // loaded.
    for (const auto& pack : ctx.packs) {
        auto result = euxis::scan::apply_rules(pack, *ast, *graph, file_str);
        for (auto& f : result.findings) {
            ctx.findings.push_back(std::move(f));
            ++ctx.rule_findings;
        }
    }

    // Taint analyzer — runs whenever --taint is on. We have to
    // build the DDG ourselves (the OpenGrep engine doesn't need
    // it). The analyzer is function-local so running it per-file
    // is the right granularity; cross-file flows land with P1.7.
    if (args.run_taint) {
        euxis::cpg::build_ddg(*graph);
        auto taint_spec   = euxis::taint::builtin_specs();
        auto taint_result = euxis::taint::analyze(*graph, *ast, taint_spec);
        ctx.taint_flows += taint_result.stats.flows_emitted;
        auto taint_findings =
            euxis::taint::flows_to_findings(taint_result, *graph, file_str);
        for (auto& f : taint_findings) {
            ctx.findings.push_back(std::move(f));
            ++ctx.taint_findings;
        }
    }

    ++ctx.files_scanned;
}

auto walk_target(ScanContext& ctx, const ScanArgs& args)
    -> std::expected<void, std::string> {
    namespace fs = std::filesystem;
    if (!fs::exists(args.target)) {
        return std::unexpected("scan target does not exist: " + args.target.string());
    }

    if (fs::is_regular_file(args.target)) {
        scan_file(ctx, args.target, args);
        return {};
    }

    std::error_code ec;
    fs::recursive_directory_iterator it(
        args.target,
        fs::directory_options::skip_permission_denied,
        ec);
    if (ec) {
        return std::unexpected("scan target walk: " + ec.message());
    }
    fs::recursive_directory_iterator end;
    for (; it != end; it.increment(ec)) {
        if (ec) { ec.clear(); continue; }
        if (ctx.files_scanned >= args.max_files) break;
        auto rel = it->path().lexically_relative(args.target);
        if (should_skip(rel)) {
            if (it->is_directory(ec)) it.disable_recursion_pending();
            continue;
        }
        if (!it->is_regular_file(ec)) continue;
        scan_file(ctx, it->path(), args);
    }
    return {};
}

void print_finding(const euxis::security::Finding& f) {
    std::println("  [{}] {}:{}:{}: {}",
        euxis::security::severity_label(f.severity),
        f.primary_location.path,
        f.primary_location.start_row,
        f.primary_location.start_column,
        f.message);
}

auto severity_rank(euxis::security::Severity s) noexcept -> int {
    return static_cast<int>(s);
}

auto count_at_or_above(const std::vector<euxis::security::Finding>& findings,
                       euxis::security::Severity threshold) -> std::size_t {
    std::size_t n = 0;
    for (const auto& f : findings) {
        if (severity_rank(f.severity) >= severity_rank(threshold)) ++n;
    }
    return n;
}

} // namespace

int cmd_scan(Context& /*ctx*/, const std::vector<std::string>& argv) {
    auto parsed = parse_args(argv);
    if (!parsed) {
        if (parsed.error() == "__help__") {
            print_help();
            return to_int(ExitCode::Success);
        }
        std::println(stderr, "euxis scan: {}", parsed.error());
        return to_int(ExitCode::InfraError);
    }
    const ScanArgs& a = *parsed;

    ScanContext ctx;
    for (const auto& rf : a.rule_files) {
        auto pack = euxis::scan::load_rules_file(rf);
        if (!pack) {
            std::println(stderr, "euxis scan: {}: {}",
                rf.string(), pack.error().message);
            return to_int(ExitCode::InfraError);
        }
        ctx.packs.push_back(std::move(*pack));
    }

    auto walked = walk_target(ctx, a);
    if (!walked) {
        std::println(stderr, "euxis scan: {}", walked.error());
        return to_int(ExitCode::InfraError);
    }

    // SARIF emission.
    if (a.emit_sarif) {
        auto json = findings_to_sarif(ctx.findings, "0.0.3");
        std::ofstream out(a.sarif_output);
        if (!out.is_open()) {
            std::println(stderr,
                "euxis scan: cannot write SARIF to {}",
                a.sarif_output.string());
            return to_int(ExitCode::InfraError);
        }
        out << json.dump(2) << "\n";
    }

    // Text summary.
    if (!a.quiet) {
        for (const auto& f : ctx.findings) print_finding(f);
    }

    std::size_t blocking = count_at_or_above(ctx.findings, a.fail_on);
    std::println(
        "euxis scan: scanned {} file(s) [{} skipped, {} parse failures], "
        "{} rule pack(s), {} finding(s) ({} rule, {} taint from {} flow(s); "
        "{} at/above {})",
        ctx.files_scanned,
        ctx.files_skipped,
        ctx.parse_failures,
        ctx.packs.size(),
        ctx.findings.size(),
        ctx.rule_findings,
        ctx.taint_findings,
        ctx.taint_flows,
        blocking,
        euxis::security::severity_label(a.fail_on));
    if (a.emit_sarif) {
        std::println("euxis scan: SARIF written to {}", a.sarif_output.string());
    }

    if (blocking > 0) return to_int(ExitCode::BlockingFindings);
    if (!ctx.findings.empty()) return to_int(ExitCode::AdvisoryFindings);
    return to_int(ExitCode::Success);
}

} // namespace euxis::cli::cmd
