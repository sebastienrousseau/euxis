/// @file
/// @brief Implementation of `euxis sbom` — walks a target directory,
///        builds a canonical SBOM via libs/sca, and emits CycloneDX
///        1.6 / SPDX 3.0.1 / OpenVEX JSON.
///
/// Output paths follow the convention from `.github/actions/euxis-scan
/// /action.yml`: `euxis-sbom.cdx.json`, `euxis-sbom.spdx.json`,
/// `euxis-vex.openvex.json`. `--output` overrides for a single format;
/// for `--format=both` the override is treated as a basename and we
/// append `.cdx.json` and `.spdx.json`.

#include "euxis/cli/cmd/sbom.hpp"
#include "euxis/cli/exit_codes.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <print>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "euxis/sbom/cyclonedx.hpp"
#include "euxis/sbom/openvex.hpp"
#include "euxis/sbom/spdx.hpp"
#include "euxis/sca/scanner.hpp"

namespace euxis::cli::cmd {

namespace {

enum class Format {
    CycloneDx,
    Spdx,
    Both,
    OpenVex,
};

struct Args {
    std::filesystem::path target{"."};
    Format format{Format::CycloneDx};
    std::filesystem::path output;
    bool pretty{true};
};

auto parse_args(const std::vector<std::string>& argv) -> std::expected<Args, std::string> {
    Args a;
    bool target_set = false;
    for (const auto& s : argv) {
        if (s == "--pretty")       { a.pretty = true;  continue; }
        if (s == "--no-pretty")    { a.pretty = false; continue; }
        if (s.starts_with("--format=")) {
            auto v = s.substr(9);
            if      (v == "cyclonedx" || v == "cyclonedx-1.6") a.format = Format::CycloneDx;
            else if (v == "spdx"      || v == "spdx-3.0.1")    a.format = Format::Spdx;
            else if (v == "both")                               a.format = Format::Both;
            else if (v == "openvex")                            a.format = Format::OpenVex;
            else return std::unexpected("--format must be cyclonedx | spdx | both | openvex");
            continue;
        }
        if (s.starts_with("--output=")) { a.output = s.substr(9); continue; }
        if (s == "--help" || s == "-h") return std::unexpected("__help__");
        if (s.starts_with("-")) return std::unexpected("Unknown flag: " + s);
        if (!target_set) { a.target = s; target_set = true; continue; }
        return std::unexpected("Unexpected positional argument: " + s);
    }
    return a;
}

auto default_output(Format f) -> std::filesystem::path {
    switch (f) {
        case Format::CycloneDx: return "euxis-sbom.cdx.json";
        case Format::Spdx:      return "euxis-sbom.spdx.json";
        case Format::Both:      return "euxis-sbom";
        case Format::OpenVex:   return "euxis-vex.openvex.json";
    }
    return "euxis-sbom.cdx.json";
}

auto write_json(const std::filesystem::path& path,
                const nlohmann::json& j,
                bool pretty) -> bool {
    std::ofstream out(path);
    if (!out.is_open()) return false;
    if (pretty) out << j.dump(2);
    else        out << j.dump();
    out << '\n';
    return out.good();
}

void print_help() {
    std::println("Usage: euxis sbom [target] [options]");
    std::println("");
    std::println("Walks a directory tree, dispatches recognised lockfiles to");
    std::println("ecosystem parsers (Cargo.lock, package-lock.json, Pipfile.lock,");
    std::println("go.sum), and emits an SBOM in the requested format.");
    std::println("");
    std::println("Options:");
    std::println("  --format=<cyclonedx|spdx|both|openvex>  default: cyclonedx");
    std::println("  --output=<path>                         override default filename");
    std::println("  --pretty / --no-pretty                  pretty-print JSON (default: pretty)");
    std::println("");
    std::println("Default output filenames:");
    std::println("  cyclonedx -> euxis-sbom.cdx.json");
    std::println("  spdx      -> euxis-sbom.spdx.json");
    std::println("  both      -> euxis-sbom.cdx.json + euxis-sbom.spdx.json");
    std::println("  openvex   -> euxis-vex.openvex.json");
}

} // namespace

int cmd_sbom(Context& /*ctx*/, const std::vector<std::string>& argv) {
    auto parsed = parse_args(argv);
    if (!parsed) {
        if (parsed.error() == "__help__") {
            print_help();
            return to_int(ExitCode::Success);
        }
        std::println(stderr, "euxis sbom: {}", parsed.error());
        return to_int(ExitCode::InfraError);
    }
    const Args& a = *parsed;

    auto scan = euxis::sca::scan_directory(a.target);
    if (!scan) {
        std::println(stderr, "euxis sbom: {}", scan.error().message);
        return to_int(ExitCode::InfraError);
    }
    auto doc = euxis::sca::to_sbom_document(*scan, a.target);

    auto write_one = [&](Format f, const std::filesystem::path& path) -> bool {
        nlohmann::json j;
        switch (f) {
            case Format::CycloneDx: j = euxis::sbom::to_cyclonedx_1_6(doc);  break;
            case Format::Spdx:      j = euxis::sbom::to_spdx_3_0_1(doc);     break;
            case Format::OpenVex:   {
                euxis::sbom::VexDocument vex;
                j = euxis::sbom::to_openvex(vex);
                break;
            }
            case Format::Both: return false;
        }
        if (!write_json(path, j, a.pretty)) {
            std::println(stderr, "euxis sbom: failed to write {}", path.string());
            return false;
        }
        std::println("euxis sbom: wrote {}", path.string());
        return true;
    };

    if (a.format == Format::Both) {
        auto base = a.output.empty() ? default_output(Format::Both) : a.output;
        auto cdx  = base; cdx  += ".cdx.json";
        auto spdx = base; spdx += ".spdx.json";
        if (!write_one(Format::CycloneDx, cdx) ||
            !write_one(Format::Spdx,      spdx)) {
            return to_int(ExitCode::InfraError);
        }
    } else {
        auto path = a.output.empty() ? default_output(a.format) : a.output;
        if (!write_one(a.format, path)) return to_int(ExitCode::InfraError);
    }

    std::println("euxis sbom: scanned {} manifest(s), {} component(s)",
                 scan->manifests.size(), doc.components.size());
    if (!scan->errors.empty()) {
        std::println(stderr, "euxis sbom: {} parse error(s) encountered (see SARIF)", scan->errors.size());
    }
    return to_int(ExitCode::Success);
}

} // namespace euxis::cli::cmd
