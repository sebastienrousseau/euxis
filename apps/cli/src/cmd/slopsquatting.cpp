/// @file
/// @brief Implementation of `euxis slopsquatting`.

#include "euxis/cli/cmd/slopsquatting.hpp"
#include "euxis/cli/exit_codes.hpp"
#include "euxis/cli/sarif.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <print>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "euxis/sca/scanner.hpp"
#include "euxis/slopsquatting/guard.hpp"
#include "euxis/slopsquatting/integration.hpp"

namespace euxis::cli::cmd {

namespace {

struct Args {
    std::filesystem::path target{"."};
    std::filesystem::path corpus_file;
    std::filesystem::path sarif_output;
    bool load_seed{true};
    bool quiet{false};
    bool staged{false};
};

auto parse_args(const std::vector<std::string>& argv)
    -> std::expected<Args, std::string> {
    Args a;
    bool target_set = false;
    for (const auto& s : argv) {
        if (s == "--no-seed")  { a.load_seed = false; continue; }
        if (s == "--quiet")    { a.quiet = true;      continue; }
        if (s == "--staged")   { a.staged = true;     continue; }
        if (s.starts_with("--corpus=")) { a.corpus_file = s.substr(9); continue; }
        if (s.starts_with("--sarif="))  { a.sarif_output = s.substr(8); continue; }
        if (s == "--help" || s == "-h") return std::unexpected("__help__");
        if (s.starts_with("-")) return std::unexpected("Unknown flag: " + s);
        if (!target_set) { a.target = s; target_set = true; continue; }
        return std::unexpected("Unexpected positional argument: " + s);
    }
    return a;
}

void print_help() {
    std::println("Usage: euxis slopsquatting [target] [options]");
    std::println("");
    std::println("Checks every dependency name in the target's lockfiles");
    std::println("against the slopsquatting hallucinated-package corpus.");
    std::println("");
    std::println("Options:");
    std::println("  --corpus=<path>   Add an external corpus on top of the seed");
    std::println("  --no-seed         Skip the embedded seed corpus");
    std::println("  --sarif=<path>    Write findings as SARIF 2.1.0 to <path>");
    std::println("  --quiet           Print only the summary line");
    std::println("  --staged          Restrict to lockfiles in `git diff --staged`");
    std::println("");
    std::println("Exit codes:");
    std::println("  0  no matches");
    std::println("  1  invalid arguments or scan failure");
    std::println("  3  one or more matches found (blocking finding)");
}

void print_summary(std::size_t corpus_size, std::size_t manifest_count,
                   std::size_t finding_count) {
    std::println(
        "euxis slopsquatting: corpus={} manifests={} findings={}",
        corpus_size, manifest_count, finding_count);
}

void print_finding(const euxis::security::Finding& f) {
    std::println("  [{}] {}",
        euxis::security::severity_label(f.severity),
        f.message);
    if (!f.primary_location.path.empty()) {
        std::println("       at {}",
            f.primary_location.path);
    }
}

} // namespace

int cmd_slopsquatting(Context& /*ctx*/, const std::vector<std::string>& argv) {
    auto parsed = parse_args(argv);
    if (!parsed) {
        if (parsed.error() == "__help__") {
            print_help();
            return to_int(ExitCode::Success);
        }
        std::println(stderr, "euxis slopsquatting: {}", parsed.error());
        return to_int(ExitCode::InfraError);
    }
    const Args& a = *parsed;

    // Build the guard.
    euxis::slopsquatting::Guard guard;
    if (a.load_seed) {
        guard.load_seed();
    }
    if (!a.corpus_file.empty()) {
        auto added = guard.load_corpus_file(a.corpus_file);
        if (!added) {
            std::println(stderr,
                "euxis slopsquatting: failed to load corpus {}: {}",
                a.corpus_file.string(), added.error().message);
            return to_int(ExitCode::InfraError);
        }
    }
    if (guard.size() == 0) {
        std::println(stderr,
            "euxis slopsquatting: corpus is empty (use --corpus= "
            "or drop --no-seed).");
        return to_int(ExitCode::InfraError);
    }

    // Scan.
    auto scan = euxis::sca::scan_directory(a.target);
    if (!scan) {
        std::println(stderr, "euxis slopsquatting: {}", scan.error().message);
        return to_int(ExitCode::InfraError);
    }

    auto findings = euxis::slopsquatting::check_scan_result(guard, *scan);

    // Output.
    if (!a.sarif_output.empty()) {
        auto json = findings_to_sarif(findings, "0.1.0");
        std::ofstream out(a.sarif_output);
        if (!out.is_open()) {
            std::println(stderr,
                "euxis slopsquatting: cannot write SARIF to {}",
                a.sarif_output.string());
            return to_int(ExitCode::InfraError);
        }
        out << json.dump(2) << "\n";
    }

    if (!a.quiet) {
        for (const auto& f : findings) print_finding(f);
    }
    print_summary(guard.size(), scan->manifests.size(), findings.size());

    return findings.empty()
        ? to_int(ExitCode::Success)
        : to_int(ExitCode::BlockingFindings);
}

} // namespace euxis::cli::cmd
