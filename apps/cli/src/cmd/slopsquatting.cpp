/// @file
/// @brief Implementation of `euxis slopsquatting`.

#include "euxis/cli/cmd/slopsquatting.hpp"
#include "euxis/cli/exit_codes.hpp"
#include "euxis/cli/process.hpp"
#include "euxis/cli/sarif.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <print>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include <nlohmann/json.hpp>

#include "euxis/sca/cargo_lock.hpp"
#include "euxis/sca/go_sum.hpp"
#include "euxis/sca/npm_lock.hpp"
#include "euxis/sca/pipfile_lock.hpp"
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
    euxis::sca::ScanResult scan_owned;
    const euxis::sca::ScanResult* scan = nullptr;

    if (a.staged) {
        // Restrict to lockfiles among the staged changes. The pre-
        // commit hook from .pre-commit-hooks.yaml relies on this:
        // a freshly-edited package-lock.json triggers a focused scan
        // without re-walking the whole tree.
        static const std::unordered_set<std::string_view> kLockfileNames{
            "Cargo.lock", "package-lock.json", "Pipfile.lock", "go.sum",
        };
        auto git = Process::run("git",
            {"diff", "--cached", "--name-only", "--diff-filter=ACMR"},
            10);
        if (!git.success()) {
            std::println(stderr,
                "euxis slopsquatting: `git diff --cached` failed (exit {}). "
                "Are you inside a git repository?", git.exit_code);
            return to_int(ExitCode::InfraError);
        }
        std::istringstream iss(git.stdout_output);
        std::string rel;
        std::size_t lockfile_count = 0;
        while (std::getline(iss, rel)) {
            if (rel.empty()) continue;
            std::filesystem::path p{rel};
            if (kLockfileNames.find(p.filename().string()) == kLockfileNames.end()) {
                continue;
            }
            ++lockfile_count;
            const auto name = p.filename().string();
            std::expected<euxis::sca::ParsedManifest, euxis::sca::ParseError> parsed;
            if      (name == "Cargo.lock")        parsed = euxis::sca::parse_cargo_lock_file(p);
            else if (name == "package-lock.json") parsed = euxis::sca::parse_npm_lock_file(p);
            else if (name == "Pipfile.lock")      parsed = euxis::sca::parse_pipfile_lock_file(p);
            else if (name == "go.sum")            parsed = euxis::sca::parse_go_sum_file(p);
            if (parsed) {
                scan_owned.manifests.push_back(std::move(*parsed));
            } else {
                scan_owned.errors.push_back(parsed.error());
            }
        }
        if (lockfile_count == 0) {
            std::println("euxis slopsquatting: no staged lockfiles; nothing to do.");
            print_summary(guard.size(), 0, 0);
            return to_int(ExitCode::Success);
        }
        scan = &scan_owned;
    } else {
        auto walk = euxis::sca::scan_directory(a.target);
        if (!walk) {
            std::println(stderr, "euxis slopsquatting: {}", walk.error().message);
            return to_int(ExitCode::InfraError);
        }
        scan_owned = std::move(*walk);
        scan = &scan_owned;
    }

    auto findings = euxis::slopsquatting::check_scan_result(guard, *scan);
    const auto manifest_count = scan->manifests.size();

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
    print_summary(guard.size(), manifest_count, findings.size());

    return findings.empty()
        ? to_int(ExitCode::Success)
        : to_int(ExitCode::BlockingFindings);
}

} // namespace euxis::cli::cmd
