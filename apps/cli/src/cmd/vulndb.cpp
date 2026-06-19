/// @file
/// @brief Implementation of `euxis vulndb`.

#include "euxis/cli/cmd/vulndb.hpp"
#include "euxis/cli/exit_codes.hpp"

#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

#include "euxis/vulndb/osv_client.hpp"
#include "euxis/vulndb/types.hpp"

namespace euxis::cli::cmd {

namespace {

void print_query_usage() {
    std::cout
        << "Usage: euxis vulndb query <PURL> [--json]\n\n"
        << "  <PURL>          Package URL with a version segment, e.g.\n"
        << "                  pkg:cargo/serde@1.0.150\n"
        << "                  pkg:pypi/requests@2.20.0\n"
        << "  --json          Emit the full OSV record as JSON instead of\n"
        << "                  a human-readable summary.\n"
        << "\n"
        << "Queries the public OSV.dev REST API and prints any matched\n"
        << "vulnerabilities. Returns exit code 1 when at least one\n"
        << "High or Critical vulnerability is reported, 0 otherwise.\n";
}

[[nodiscard]] auto run_query(const std::vector<std::string>& args) -> int {
    if (args.empty()) {
        print_query_usage();
        return to_int(ExitCode::InfraError);
    }
    bool json_output = false;
    std::string purl;
    for (const auto& a : args) {
        if (a == "--json") {
            json_output = true;
        } else if (a == "-h" || a == "--help") {
            print_query_usage();
            return to_int(ExitCode::Success);
        } else if (purl.empty()) {
            purl = a;
        } else {
            std::cerr << "vulndb query: unexpected arg: " << a << '\n';
            return to_int(ExitCode::InfraError);
        }
    }
    if (purl.empty()) {
        print_query_usage();
        return to_int(ExitCode::InfraError);
    }

    using namespace euxis::vulndb;
    OsvClient client;
    auto result = client.query_by_purl(purl);
    if (!result) {
        std::cerr << "vulndb query: " << error_name(result.error().kind)
                  << " — " << result.error().message << '\n';
        return to_int(ExitCode::InfraError);
    }
    const auto& vulns = *result;

    if (json_output) {
        nlohmann::json out = nlohmann::json::array();
        for (const auto& v : vulns) {
            out.push_back(v.raw);
        }
        std::cout << out.dump(2) << '\n';
    } else {
        std::cout << "OSV lookup for " << purl
                  << " — " << vulns.size() << " vulnerabilit"
                  << (vulns.size() == 1 ? "y" : "ies") << " reported\n";
        for (const auto& v : vulns) {
            std::cout << "  " << v.id
                      << "  [" << severity_name(v.severity)
                      << " / cvss " << v.cvss_score << "]\n";
            if (!v.summary.empty()) {
                std::cout << "    " << v.summary << '\n';
            }
            if (!v.fixed_in.empty()) {
                std::cout << "    Fixed in: " << v.fixed_in << '\n';
            }
        }
    }

    for (const auto& v : vulns) {
        if (v.severity == Severity::High || v.severity == Severity::Critical) {
            return to_int(ExitCode::BlockingFindings);
        }
    }
    return to_int(ExitCode::Success);
}

[[nodiscard]] auto run_sync(const std::vector<std::string>&) -> int {
    // The offline-dump path is declared in libs/vulndb/errors.hpp
    // (ErrorKind::OfflineDumpUnreadable) but the loader is not yet
    // implemented. Surface this explicitly to users running CI air-gap
    // pipelines — they need to know the online path is the only option.
    std::cout
        << "euxis vulndb sync — offline OSV.dev dump support is not yet\n"
        << "implemented in this build. The online query path via\n"
        << "`euxis vulndb query` and `euxis sbom --enrich` works against\n"
        << "the public OSV.dev REST API.\n"
        << "\n"
        << "Track this work at:\n"
        << "  https://github.com/sebastienrousseau/euxis/issues\n";
    return to_int(ExitCode::Success);
}

void print_top_usage() {
    std::cout
        << "Usage: euxis vulndb <subcommand> [args]\n\n"
        << "Subcommands:\n"
        << "  query <PURL> [--json]    Query OSV.dev for one Package URL\n"
        << "  sync                     (placeholder) sync the offline OSV dump\n"
        << "\n"
        << "Examples:\n"
        << "  euxis vulndb query pkg:cargo/serde@1.0.150\n"
        << "  euxis vulndb query pkg:pypi/requests@2.20.0 --json\n";
}

} // namespace

auto cmd_vulndb(Context& /*ctx*/, const std::vector<std::string>& args) -> int {
    if (args.empty() || args.front() == "-h" || args.front() == "--help") {
        print_top_usage();
        return to_int(ExitCode::Success);
    }
    const auto& sub = args.front();
    const std::vector<std::string> rest{args.begin() + 1, args.end()};

    if (sub == "query") return run_query(rest);
    if (sub == "sync")  return run_sync(rest);

    std::cerr << "vulndb: unknown subcommand: " << sub << "\n\n";
    print_top_usage();
    return to_int(ExitCode::InfraError);
}

} // namespace euxis::cli::cmd
