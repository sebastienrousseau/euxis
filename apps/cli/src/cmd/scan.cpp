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
#include <memory>
#include <unordered_set>
#include <vector>

#include <nlohmann/json.hpp>

#include "euxis/cache/hash.hpp"
#include "euxis/cache/key.hpp"
#include "euxis/cache/store.hpp"
#include "euxis/cpg/builder.hpp"
#include "euxis/cpg/ddg.hpp"
#include "euxis/ensemble/deterministic.hpp"
#include "euxis/ensemble/runner.hpp"
#include "euxis/parse/parser.hpp"
#include "euxis/parse/types.hpp"
#include "euxis/reach/callgraph.hpp"
#include "euxis/reach/reachability.hpp"
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

    /// libs/taint forward-DDG analyzer toggle. Default: on.
    bool run_taint{true};

    /// libs/reach call-graph + reachability annotation. Default: on.
    /// Adds `euxis:reachable:{true,false,unknown}` to every Finding's
    /// compliance_taxa.
    bool run_reach{true};

    /// libs/ensemble multi-verifier quorum runner. Default: off
    /// (opt-in until real LLM providers ship). When on, every
    /// Finding gets one or more EnsembleVote entries attached and
    /// its confidence escalated or downgraded.
    bool run_ensemble{false};

    /// libs/cache incremental scan cache. Default: on. Re-runs over
    /// unchanged files restore cached findings instead of re-parsing.
    bool use_cache{true};

    /// Cache database override. Empty = use the libs/cache default.
    std::filesystem::path cache_db;
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
        if (s == "--no-taint")    { a.run_taint = false;    continue; }
        if (s == "--taint")       { a.run_taint = true;     continue; }
        if (s == "--no-reach")    { a.run_reach = false;    continue; }
        if (s == "--reach")       { a.run_reach = true;     continue; }
        if (s == "--ensemble")    { a.run_ensemble = true;  continue; }
        if (s == "--no-ensemble") { a.run_ensemble = false; continue; }
        if (s == "--no-cache")    { a.use_cache = false;    continue; }
        if (s == "--cache")       { a.use_cache = true;     continue; }
        if (s.starts_with("--cache-db=")) {
            a.cache_db = s.substr(11);
            continue;
        }
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
    std::println("  --reach              Annotate findings with reachability taxa (default: on)");
    std::println("  --no-reach           Disable reachability annotation");
    std::println("  --ensemble           Run libs/ensemble multi-verifier quorum (default: off)");
    std::println("  --no-ensemble        (explicit) disable ensemble");
    std::println("  --cache              Use the incremental scan cache (default: on)");
    std::println("  --no-cache           Disable the cache (every file re-parsed)");
    std::println("  --cache-db=<path>    Override the cache database location");
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
    /// Cache + reach + ensemble accounting.
    std::size_t cache_hits{0};
    std::size_t cache_misses{0};
    std::size_t ensemble_confirmed{0};
    std::size_t ensemble_rejected{0};

    /// Per-scan-invocation ruleset fingerprint — folded into the
    /// cache key so a ruleset edit invalidates every prior entry.
    std::string ruleset_hash;
    /// Open cache handle (when use_cache=true). Set up once per
    /// scan invocation by `cmd_scan`.
    std::unique_ptr<euxis::cache::ScanCache> cache;
};

// ---- Cache-side helpers ---------------------------------------------------

/// Minimal JSON serialisation of the subset of Finding fields we
/// round-trip through the scan cache. The full canonical Finding
/// carries fixes, ensemble votes, related_locations, etc.; for the
/// v0.0.3 cache we keep only what `euxis scan` produces and what
/// SARIF emission needs.
auto serialize_findings(const std::vector<euxis::security::Finding>& findings)
    -> std::string {
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& f : findings) {
        nlohmann::json item;
        item["rule_id"]            = f.rule_id;
        item["message"]            = f.message;
        item["severity"]           = static_cast<int>(f.severity);
        item["confidence"]         = static_cast<int>(f.confidence);
        item["stable_fingerprint"] = f.stable_fingerprint;
        item["primary_location"]   = {
            {"path",         f.primary_location.path},
            {"start_row",    f.primary_location.start_line},
            {"start_column", f.primary_location.start_column},
            {"end_row",      f.primary_location.end_line},
            {"end_column",   f.primary_location.end_column},
        };
        if (f.cwe.has_value()) {
            item["cwe"] = f.cwe->id;
        }
        item["owasp"]            = static_cast<int>(f.owasp);
        item["compliance_taxa"]  = f.compliance_taxa;
        arr.push_back(std::move(item));
    }
    nlohmann::json doc;
    doc["findings"] = std::move(arr);
    return doc.dump();
}

auto deserialize_findings(const std::string& json_str)
    -> std::vector<euxis::security::Finding> {
    std::vector<euxis::security::Finding> out;
    try {
        auto doc = nlohmann::json::parse(json_str);
        if (!doc.contains("findings") || !doc["findings"].is_array()) return out;
        for (const auto& item : doc["findings"]) {
            if (!item.is_object()) continue;
            euxis::security::Finding f;
            f.rule_id            = item.value("rule_id", "");
            f.message            = item.value("message", "");
            f.severity           = static_cast<euxis::security::Severity>(
                                    item.value("severity",
                                        static_cast<int>(euxis::security::Severity::Medium)));
            f.confidence         = static_cast<euxis::security::Confidence>(
                                    item.value("confidence",
                                        static_cast<int>(euxis::security::Confidence::Probable)));
            f.stable_fingerprint = item.value("stable_fingerprint", "");
            if (item.contains("cwe") && item["cwe"].is_string()) {
                f.cwe = euxis::security::CweRef{
                    .id = item["cwe"].get<std::string>(),
                };
            }
            f.owasp = static_cast<euxis::security::OwaspCategory>(
                          item.value("owasp",
                              static_cast<int>(euxis::security::OwaspCategory::None)));
            if (item.contains("primary_location") &&
                item["primary_location"].is_object()) {
                const auto& loc = item["primary_location"];
                f.primary_location.path         = loc.value("path", "");
                f.primary_location.start_line   = loc.value("start_row",    0);
                f.primary_location.start_column = loc.value("start_column", 0);
                f.primary_location.end_line     = loc.value("end_row",      0);
                f.primary_location.end_column   = loc.value("end_column",   0);
            }
            if (item.contains("compliance_taxa") &&
                item["compliance_taxa"].is_array()) {
                for (const auto& t : item["compliance_taxa"]) {
                    if (t.is_string()) f.compliance_taxa.push_back(t.get<std::string>());
                }
            }
            out.push_back(std::move(f));
        }
    } catch (...) {
        // Cache deserialisation failures are non-fatal — caller
        // falls back to a fresh scan.
    }
    return out;
}

/// Compute a stable ruleset hash so a rule-pack edit invalidates
/// the entire cache for this scan invocation.
auto compute_ruleset_hash(const ScanArgs& args,
                          const std::vector<euxis::scan::RulePack>& packs)
    -> std::string {
    euxis::cache::Hasher h;
    h.update(std::string_view{"v0.0.3"});
    h.update(std::string_view{args.run_taint ? "taint" : "no-taint"});
    h.update(std::string_view{args.run_reach ? "reach" : "no-reach"});
    h.update(std::string_view{args.run_ensemble ? "ensemble" : "no-ensemble"});
    if (args.language_filter) {
        h.update(std::string_view{
            euxis::parse::language_str(*args.language_filter)});
    }
    for (const auto& pack : packs) {
        h.update(std::string_view{pack.name});
        for (const auto& rule : pack.rules) {
            h.update(std::string_view{rule.id});
            h.update(std::string_view{rule.message});
            h.update(std::string_view{rule.pattern.text});
        }
    }
    return h.finalize();
}

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

    // ---- Cache lookup --------------------------------------------------
    std::string content_hash_str;
    if (args.use_cache && ctx.cache) {
        auto content_hash = euxis::cache::hash_file(file);
        if (content_hash) {
            content_hash_str = *content_hash;
            euxis::cache::KeyInputs k{
                .file         = file,
                .content_hash = content_hash_str,
                .ruleset_hash = ctx.ruleset_hash,
                .tool_version = "0.0.3",
            };
            auto cached = ctx.cache->get(euxis::cache::compose_key(k));
            if (cached && *cached) {
                auto restored = deserialize_findings((*cached)->findings_json);
                for (auto& f : restored) {
                    ctx.findings.push_back(std::move(f));
                }
                ++ctx.cache_hits;
                ++ctx.files_scanned;
                return;
            }
        }
    }

    // ---- Cache miss / disabled: do the real work ---------------------
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
    std::size_t findings_before = ctx.findings.size();

    // OpenGrep rule engine — runs whenever any rule packs were
    // loaded.
    for (const auto& pack : ctx.packs) {
        auto result = euxis::scan::apply_rules(pack, *ast, *graph, file_str);
        for (auto& f : result.findings) {
            ctx.findings.push_back(std::move(f));
            ++ctx.rule_findings;
        }
    }

    // Taint analyzer — runs whenever --taint is on.
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

    // Reachability annotation — runs whenever --reach is on.
    if (args.run_reach && ctx.findings.size() > findings_before) {
        auto cg = euxis::reach::build_call_graph(*graph, *ast);
        auto r  = euxis::reach::compute_reachable(*graph, *ast, cg.graph);
        euxis::reach::annotate_findings(
            *graph, r,
            ctx.findings.begin() + static_cast<std::ptrdiff_t>(findings_before),
            ctx.findings.end());
    }

    // ---- Cache store for next time ----------------------------------
    if (args.use_cache && ctx.cache && !content_hash_str.empty()) {
        std::vector<euxis::security::Finding> file_findings(
            ctx.findings.begin() + static_cast<std::ptrdiff_t>(findings_before),
            ctx.findings.end());
        euxis::cache::KeyInputs k{
            .file         = file,
            .content_hash = content_hash_str,
            .ruleset_hash = ctx.ruleset_hash,
            .tool_version = "0.0.3",
        };
        euxis::cache::CacheEntry e;
        e.findings_json = serialize_findings(file_findings);
        e.size_bytes    = static_cast<std::int64_t>(e.findings_json.size());
        (void)ctx.cache->put(k, e);
        ++ctx.cache_misses;
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
        f.primary_location.start_line,
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

    // Open the scan cache before any file is walked so the
    // ruleset hash is computed once and reused per file.
    if (a.use_cache) {
        ctx.ruleset_hash = compute_ruleset_hash(a, ctx.packs);
        std::filesystem::path db = a.cache_db;
        if (db.empty()) {
            const char* xdg = std::getenv("XDG_CACHE_HOME");
            if (xdg != nullptr && *xdg != '\0') {
                db = std::filesystem::path{xdg} / "euxis" / "scan-cache.sqlite";
            } else if (const char* home = std::getenv("HOME");
                        home != nullptr && *home != '\0') {
                db = std::filesystem::path{home} / ".cache" / "euxis"
                     / "scan-cache.sqlite";
            } else {
                db = std::filesystem::temp_directory_path() /
                     "euxis-scan-cache.sqlite";
            }
        }
        auto opened = euxis::cache::ScanCache::open(db);
        if (opened) {
            ctx.cache = std::make_unique<euxis::cache::ScanCache>(
                std::move(*opened));
        } else {
            // Cache unavailability is non-fatal — we just skip
            // caching and continue.
            std::println(stderr,
                "euxis scan: cache disabled ({}): {}",
                db.string(), opened.error().message);
        }
    }

    auto walked = walk_target(ctx, a);
    if (!walked) {
        std::println(stderr, "euxis scan: {}", walked.error());
        return to_int(ExitCode::InfraError);
    }

    // Ensemble verification — runs after walk so verifiers see
    // every finding from every engine.
    if (a.run_ensemble && !ctx.findings.empty()) {
        std::vector<euxis::ensemble::VerifierPtr> verifiers{
            std::make_shared<euxis::ensemble::DeterministicVerifier>(
                "euxis-deterministic-a"),
            std::make_shared<euxis::ensemble::DeterministicVerifier>(
                "euxis-deterministic-b",
                [](const euxis::ensemble::VoteRequest& r) {
                    return r.severity >= euxis::security::Severity::Medium;
                }),
        };
        euxis::ensemble::EnsembleConfig cfg;
        cfg.quorum = 2;
        auto er = euxis::ensemble::verify(
            std::move(ctx.findings), verifiers, cfg);
        ctx.findings           = std::move(er.findings);
        ctx.ensemble_confirmed = er.stats.confirmed;
        ctx.ensemble_rejected  = er.stats.rejected;
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
        "euxis scan: scanned {} file(s) [{} skipped, {} parse failures, "
        "{} cache hits, {} cache misses], "
        "{} rule pack(s), {} finding(s) ({} rule, {} taint from {} flow(s); "
        "ensemble: {} confirmed / {} rejected; "
        "{} at/above {})",
        ctx.files_scanned,
        ctx.files_skipped,
        ctx.parse_failures,
        ctx.cache_hits,
        ctx.cache_misses,
        ctx.packs.size(),
        ctx.findings.size(),
        ctx.rule_findings,
        ctx.taint_findings,
        ctx.taint_flows,
        ctx.ensemble_confirmed,
        ctx.ensemble_rejected,
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
