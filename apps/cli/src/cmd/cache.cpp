/// @file
/// @brief Implementation of `euxis cache`.

#include "euxis/cli/cmd/cache.hpp"
#include "euxis/cli/exit_codes.hpp"

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <iostream>
#include <print>
#include <string>

#include "euxis/cache/store.hpp"

namespace euxis::cli::cmd {

namespace {

auto default_db_path() -> std::filesystem::path {
    const char* xdg = std::getenv("XDG_CACHE_HOME");
    if (xdg != nullptr && *xdg != '\0') {
        return std::filesystem::path{xdg} / "euxis" / "scan-cache.sqlite";
    }
    const char* home = std::getenv("HOME");
    if (home != nullptr && *home != '\0') {
        return std::filesystem::path{home} / ".cache" / "euxis" / "scan-cache.sqlite";
    }
    return std::filesystem::temp_directory_path() / "euxis-scan-cache.sqlite";
}

auto resolve_db(const std::vector<std::string>& argv) -> std::filesystem::path {
    for (const auto& s : argv) {
        if (s.starts_with("--db=")) return s.substr(5);
    }
    return default_db_path();
}

auto resolve_ttl_days(const std::vector<std::string>& argv) -> int {
    for (const auto& s : argv) {
        if (s.starts_with("--ttl-days=")) {
            try { return std::stoi(s.substr(11)); } catch (...) { return 0; }
        }
    }
    return 30;  // sensible default matches .euxis.yaml's cache.ttl_days
}

auto format_iso(std::int64_t unix_sec) -> std::string {
    if (unix_sec <= 0) return "—";
    std::time_t t = static_cast<std::time_t>(unix_sec);
    std::tm tm{};
#if defined(_WIN32)
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    char buf[32]{};
    (void) std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
    return std::string{buf};
}

auto human_bytes(std::int64_t bytes) -> std::string {
    constexpr std::int64_t kib = 1024;
    constexpr std::int64_t mib = 1024 * 1024;
    if (bytes >= mib) {
        char buf[32]{};
        (void) std::snprintf(buf, sizeof(buf), "%.2f MiB",
            static_cast<double>(bytes) / static_cast<double>(mib));
        return buf;
    }
    if (bytes >= kib) {
        char buf[32]{};
        (void) std::snprintf(buf, sizeof(buf), "%.2f KiB",
            static_cast<double>(bytes) / static_cast<double>(kib));
        return buf;
    }
    return std::to_string(bytes) + " B";
}

void print_help() {
    std::println("Usage: euxis cache <stats|purge|purge-older|path> [options]");
    std::println("");
    std::println("Subcommands:");
    std::println("  stats          Print entry count, total size, oldest/newest dates");
    std::println("  purge          Delete every cached entry");
    std::println("  purge-older    Delete entries older than --ttl-days=<n> (default 30)");
    std::println("  path           Print the resolved cache database path");
    std::println("");
    std::println("Options:");
    std::println("  --db=<path>    Override the cache database path");
    std::println("  --ttl-days=<n> (purge-older only) age threshold in days");
}

int cmd_stats(const std::vector<std::string>& argv) {
    auto db = resolve_db(argv);
    auto cache = euxis::cache::ScanCache::open(db);
    if (!cache) {
        std::println(stderr, "euxis cache: open: {}", cache.error().message);
        return to_int(ExitCode::InfraError);
    }
    auto st = cache->stats();
    if (!st) {
        std::println(stderr, "euxis cache: stats: {}", st.error().message);
        return to_int(ExitCode::InfraError);
    }
    std::println("path:    {}", db.string());
    std::println("entries: {}", st->entry_count);
    std::println("size:    {}", human_bytes(st->total_size_bytes));
    std::println("oldest:  {}", format_iso(st->oldest_entry_unix));
    std::println("newest:  {}", format_iso(st->newest_entry_unix));
    return to_int(ExitCode::Success);
}

int cmd_purge(const std::vector<std::string>& argv) {
    auto db = resolve_db(argv);
    auto cache = euxis::cache::ScanCache::open(db);
    if (!cache) {
        std::println(stderr, "euxis cache: open: {}", cache.error().message);
        return to_int(ExitCode::InfraError);
    }
    auto removed = cache->purge();
    if (!removed) {
        std::println(stderr, "euxis cache: purge: {}", removed.error().message);
        return to_int(ExitCode::InfraError);
    }
    std::println("euxis cache: removed {} entries", *removed);
    return to_int(ExitCode::Success);
}

int cmd_purge_older(const std::vector<std::string>& argv) {
    auto db = resolve_db(argv);
    auto ttl = resolve_ttl_days(argv);
    if (ttl <= 0) {
        std::println(stderr, "euxis cache: --ttl-days must be positive");
        return to_int(ExitCode::InfraError);
    }
    auto cache = euxis::cache::ScanCache::open(db);
    if (!cache) {
        std::println(stderr, "euxis cache: open: {}", cache.error().message);
        return to_int(ExitCode::InfraError);
    }
    std::int64_t now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    auto removed = cache->purge_older_than(
        static_cast<std::int64_t>(ttl) * 24 * 3600, now);
    if (!removed) {
        std::println(stderr, "euxis cache: purge-older: {}", removed.error().message);
        return to_int(ExitCode::InfraError);
    }
    std::println("euxis cache: removed {} entries older than {} days",
                 *removed, ttl);
    return to_int(ExitCode::Success);
}

int cmd_path(const std::vector<std::string>& argv) {
    std::println("{}", resolve_db(argv).string());
    return to_int(ExitCode::Success);
}

} // namespace

int cmd_cache(Context& /*ctx*/, const std::vector<std::string>& argv) {
    std::string sub = "stats";
    std::vector<std::string> rest;
    for (std::size_t i = 0; i < argv.size(); ++i) {
        if (i == 0 && !argv[i].starts_with("-")) { sub = argv[i]; continue; }
        rest.push_back(argv[i]);
    }
    if (sub == "--help" || sub == "-h" || sub == "help") {
        print_help();
        return to_int(ExitCode::Success);
    }
    if (sub == "stats")        return cmd_stats(rest);
    if (sub == "purge")        return cmd_purge(rest);
    if (sub == "purge-older")  return cmd_purge_older(rest);
    if (sub == "path")         return cmd_path(rest);
    std::println(stderr, "euxis cache: unknown subcommand '{}'", sub);
    print_help();
    return to_int(ExitCode::InfraError);
}

} // namespace euxis::cli::cmd
