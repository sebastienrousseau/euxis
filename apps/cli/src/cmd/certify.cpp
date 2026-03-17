#include "euxis/cli/cmd/certify.hpp"
#include "euxis/cli/certification.hpp"
#include "euxis/cli/i18n.hpp"
#include "euxis/cli/terminal.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

using euxis::cli::i18n::tr;

namespace euxis::cli::cmd {
namespace {

namespace fs = std::filesystem;
namespace term = terminal;
namespace cert = certification;

constexpr auto kDisclaimer =
    "Note:\n"
    "  This command reports internal certification readiness and evidence coverage.\n"
    "  It does not claim external audit, legal compliance, or official certification.";

// --- Help ---

void print_help() {
    std::cerr << tr("Certification readiness assessment.") << "\n\n"
              << tr("Usage:") << "\n"
              << "  euxis certify-readiness [target] [options]\n"
              << "  euxis certify-readiness controls\n"
              << "  euxis certify-readiness report <artifact>\n\n"
              << tr("Options:") << "\n"
              << "  --framework <general|soc2|iso27001>  " << tr("Framework overlay (default: general)") << "\n"
              << "  --strict                             " << tr("Make soft gates blocking") << "\n"
              << "  --ci                                 " << tr("CI-safe output (TUI to stderr, JSON to stdout)") << "\n"
              << "  --json                               " << tr("Emit artifact JSON to stdout") << "\n"
              << "  --no-build                           " << tr("Skip build integrity gate") << "\n"
              << "  --no-tests                           " << tr("Skip unit test health gate") << "\n"
              << "  --no-security                        " << tr("Skip security critical gate") << "\n"
              << "  --commit-window <N>                  " << tr("Number of commits to check for signing (default: 20)") << "\n"
              << "  --since-ref <git-ref>                " << tr("Check commits since this git ref") << "\n"
              << "  --output <path>                      " << tr("Custom artifact output path") << "\n";
}

// --- Controls subcommand ---

int cmd_controls() {
    auto domains = cert::default_domains();

    std::cout << term::bold(tr("=== CERTIFICATION DOMAIN MODEL (18 DOMAINS) ===")) << "\n\n";

    // Show domains with framework variants
    for (const auto& fw_name : {"general", "soc2", "iso27001"}) {
        auto fw = cert::parse_framework(fw_name);
        if (!fw) continue;
        auto defs = cert::default_domains();
        cert::apply_framework_overlay(*fw, defs);

        std::cout << term::cyan(std::string("Framework: ") + fw_name) << "\n";
        int i = 1;
        for (const auto& d : defs) {
            std::string icon = d.critical ? term::icon_fail() : term::icon_info();
            std::string crit = d.critical ? term::red(" [CRITICAL]") : "";
            std::cout << "  " << std::to_string(i++) << ". " << d.name << crit << "\n";
            if (!d.required_signals.empty()) {
                std::cout << "     " << tr("Required:") << " ";
                for (size_t j = 0; j < d.required_signals.size(); ++j) {
                    if (j > 0) std::cout << ", ";
                    std::cout << d.required_signals[j];
                }
                std::cout << "\n";
            }
        }
        std::cout << "\n";
    }

    std::cout << "\n" << term::dim(kDisclaimer) << "\n";
    return 0;
}

// --- Report subcommand ---

int cmd_report(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        std::cerr << tr("Usage: euxis certify-readiness report <artifact-path>") << "\n";
        return 2;
    }

    auto path = fs::path(args[1]);
    if (!fs::exists(path)) {
        std::cerr << tr("Artifact not found:") << " " << path.string() << "\n";
        return 1;
    }

    nlohmann::json artifact;
    try {
        std::ifstream f(path);
        artifact = nlohmann::json::parse(f);
    } catch (const std::exception& e) {
        std::cerr << tr("Malformed artifact:") << " " << e.what() << "\n";
        return 2;
    }

    if (!artifact.contains("schema") || artifact["schema"] != "euxis.certification") {
        std::cerr << tr("Not a certification artifact") << "\n";
        return 2;
    }

    // Pretty-print
    std::cout << term::bold(tr("=== CERTIFICATION READINESS REPORT ===")) << "\n";
    std::cout << tr("Target:") << "      " << artifact.value("target", "unknown") << "\n";
    std::cout << tr("Framework:") << "   " << artifact.value("framework", "general") << "\n";
    std::cout << tr("Status:") << "      " << artifact.value("status", "UNKNOWN") << "\n";
    std::cout << tr("Confidence:") << "  " << artifact.value("confidence", 0) << "%\n";
    std::cout << tr("Timestamp:") << "   " << artifact.value("timestamp", "") << "\n\n";

    // Gates
    if (artifact.contains("gates")) {
        std::cout << term::bold(tr("--- Gates ---")) << "\n";
        for (const auto& g : artifact["gates"]) {
            std::string status = g.value("status", "unknown");
            std::string icon;
            if (status == "pass") icon = term::icon_ok();
            else if (status == "fail") icon = term::icon_fail();
            else if (status == "skipped") icon = term::icon_skip();
            else icon = term::icon_warn();
            std::cout << "  " << icon << " " << g.value("name", "") << "  " << g.value("detail", "") << "\n";
        }
        std::cout << "\n";
    }

    // Domains
    if (artifact.contains("domains")) {
        std::cout << term::bold(tr("--- Domains ---")) << "\n";
        for (const auto& d : artifact["domains"]) {
            std::string status = d.value("status", "unknown");
            std::string icon;
            if (status == "verified") icon = term::icon_ok();
            else if (status == "gaps") icon = term::icon_warn();
            else if (status == "blocked") icon = term::icon_fail();
            else icon = term::icon_info();
            int pct = static_cast<int>(d.value("coverage", 0.0) * 100);
            std::string crit = d.value("critical", false) ? " *" : "";
            std::cout << "  " << icon << " " << d.value("name", "")
                      << "  " << status << "  " << pct << "%" << crit << "\n";
        }
        std::cout << "\n";
    }

    // Summary
    if (artifact.contains("recommended_actions") && !artifact["recommended_actions"].empty()) {
        std::cout << term::bold(tr("--- Recommended Actions ---")) << "\n";
        for (const auto& a : artifact["recommended_actions"]) {
            std::cout << "  - " << a.get<std::string>() << "\n";
        }
        std::cout << "\n";
    }

    std::cout << term::dim(kDisclaimer) << "\n";
    return 0;
}

// --- Print human-readable result ---

void print_result(const cert::RunResult& result, bool ci_mode) {
    auto& out = ci_mode ? std::cerr : std::cout;

    out << "\n" << term::bold(tr("=== CERTIFICATION READINESS ASSESSMENT ===")) << "\n";
    out << tr("Target:") << "      " << result.target << "\n";
    out << tr("Framework:") << "   " << cert::framework_name(result.framework) << "\n";

    // Status with color
    std::string colored_status;
    if (result.status == "READY") colored_status = term::green(result.status);
    else if (result.status == "READY WITH GAPS") colored_status = term::yellow(result.status);
    else if (result.status == "BLOCKED") colored_status = term::red(result.status);
    else colored_status = term::yellow(result.status);
    out << tr("Status:") << "      " << colored_status << "\n";
    out << tr("Confidence:") << "  " << result.confidence << "%\n";

    // Gates
    out << "\n" << term::bold(tr("--- Critical Gates ---")) << "\n";
    for (const auto& g : result.gates) {
        std::string icon;
        if (g.status == "pass") icon = term::icon_ok();
        else if (g.status == "fail") icon = term::icon_fail();
        else if (g.status == "skipped") icon = term::icon_skip();
        else icon = term::icon_warn();

        // Pad name
        std::string name = g.name;
        while (name.size() < 24) name += ' ';
        out << "  " << icon << " " << name << " " << g.detail << "\n";
    }

    // Quality risk (not a gate)
    if (!result.quality_risk.high_complexity_files.empty()) {
        out << "  " << term::icon_info() << " "
            << std::string(24 - std::string("quality_risk").size() + std::string("quality_risk").size(), ' ')
            << "quality risk             "
            << std::to_string(result.quality_risk.high_complexity_files.size())
            << " high-complexity file(s)" << "\n";
    }

    // Skipped gates warning
    if (!result.skipped_gates.empty()) {
        out << "\n  " << term::yellow(tr("Readiness reduced:")) << " "
            << std::to_string(result.skipped_gates.size()) << " gate(s) skipped: ";
        for (size_t i = 0; i < result.skipped_gates.size(); ++i) {
            if (i > 0) out << ", ";
            out << result.skipped_gates[i];
        }
        out << "\n";
    }

    // Domains
    out << "\n" << term::bold(tr("--- Domain Coverage (18 domains) ---")) << "\n";
    for (const auto& d : result.domains) {
        std::string icon;
        if (d.status == "verified") icon = term::icon_ok();
        else if (d.status == "gaps") icon = term::icon_warn();
        else if (d.status == "blocked") icon = term::icon_fail();
        else icon = term::icon_info();

        int pct = static_cast<int>(d.coverage * 100);
        std::string crit = d.critical ? " *" : "";

        // Pad name and status
        std::string name = d.name;
        while (name.size() < 44) name += ' ';
        std::string status = d.status;
        while (status.size() < 14) status += ' ';

        out << "  " << icon << " " << name << " " << status << " " << pct << "%" << crit << "\n";
    }

    // Summary
    out << "\n" << term::bold(tr("--- Summary ---")) << "\n";
    out << tr("Blocking issues:") << " " << result.blocking_issues.size() << "\n";
    out << tr("Major gaps:") << " " << result.major_gaps.size() << "\n";

    if (!result.recommended_actions.empty()) {
        out << tr("Recommended actions:") << "\n";
        for (const auto& a : result.recommended_actions) {
            out << "  - " << a << "\n";
        }
    }

    // Evidence summary
    int exec_backed = 0;
    for (const auto& e : result.evidence) {
        if (e.execution_backed) ++exec_backed;
    }
    out << "\n" << tr("Evidence:") << " " << result.evidence.size() << " total, "
        << exec_backed << " execution-backed\n";

    out << "\n" << term::dim(kDisclaimer) << "\n";
}

} // anonymous namespace

// =====================================================================
//  PUBLIC COMMAND HANDLER
// =====================================================================

int cmd_certify_readiness(Context& ctx, const std::vector<std::string>& args) {
    // Check for help
    for (const auto& a : args) {
        if (a == "--help" || a == "-h") {
            print_help();
            return 0;
        }
    }

    // Check for subcommands
    if (!args.empty()) {
        if (args[0] == "controls") return cmd_controls();
        if (args[0] == "report") return cmd_report(args);
    }

    // Parse flags
    cert::RunOptions opts;
    bool ci_mode = false;
    bool json_mode = false;
    std::string target;
    std::string output_path;

    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "--framework" && i + 1 < args.size()) {
            auto fw = cert::parse_framework(args[++i]);
            if (!fw) {
                std::cerr << tr("Invalid framework:") << " " << args[i]
                          << " (" << tr("use: general, soc2, iso27001") << ")\n";
                return 2;
            }
            opts.framework = *fw;
        } else if (args[i] == "--strict") {
            opts.strict = true;
        } else if (args[i] == "--ci") {
            ci_mode = true;
        } else if (args[i] == "--json") {
            json_mode = true;
        } else if (args[i] == "--no-build") {
            opts.run_build = false;
        } else if (args[i] == "--no-tests") {
            opts.run_tests = false;
        } else if (args[i] == "--no-security") {
            opts.run_security = false;
        } else if (args[i] == "--commit-window" && i + 1 < args.size()) {
            try { opts.commit_window = std::stoi(args[++i]); }
            catch (...) { std::cerr << tr("Invalid commit window") << "\n"; return 2; }
        } else if (args[i] == "--since-ref" && i + 1 < args.size()) {
            opts.since_ref = args[++i];
        } else if (args[i] == "--output" && i + 1 < args.size()) {
            output_path = args[++i];
        } else if (!args[i].starts_with("--") && target.empty()) {
            target = args[i];
        }
    }

    // Default target
    if (target.empty()) target = ".";
    fs::path target_path;
    try {
        target_path = fs::canonical(fs::path(target));
    } catch (const fs::filesystem_error& e) {
        std::cerr << tr("Error: cannot resolve target path:") << " " << e.what() << "\n";
        return 2;
    }

    // Run certification
    auto result = cert::run_certification(ctx, target_path, opts);

    // Write artifact
    auto artifact = result.to_json();
    auto artifact_path = output_path.empty()
        ? fs::path(ctx.euxis_home) / "data" / "runtime" / "sessions" / "latest_certification.json"
        : fs::path(output_path);
    fs::create_directories(artifact_path.parent_path());
    {
        std::ofstream f(artifact_path);
        f << artifact.dump(2);
    }

    // Output
    if (json_mode || ci_mode) {
        std::cout << artifact.dump(2) << "\n";
    }

    if (!json_mode || ci_mode) {
        print_result(result, ci_mode);
        auto& out = ci_mode ? std::cerr : std::cout;
        out << "\n" << tr("Artifact:") << " " << artifact_path.string() << "\n";
    }

    return result.exit_code;
}

} // namespace euxis::cli::cmd
