#include "euxis/gateway/state.hpp"

#include <chrono>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <stdexcept>

namespace euxis::gateway {
namespace {

auto euxis_home() -> std::filesystem::path {
    const char* home = std::getenv("EUXIS_HOME");
    if (home) return home;
    const char* user_home = std::getenv("HOME");
    if (!user_home) throw std::runtime_error("Neither EUXIS_HOME nor HOME is set");
    return std::filesystem::path(user_home) / ".euxis";
}

} // namespace

auto gateway_data_dir() -> std::filesystem::path {
    auto dir = euxis_home() / "data" / "gateway";
    std::filesystem::create_directories(dir);
    return dir;
}

auto sessions_dir() -> std::filesystem::path {
    auto dir = gateway_data_dir() / "sessions";
    std::filesystem::create_directories(dir);
    return dir;
}

auto runs_dir() -> std::filesystem::path {
    auto dir = gateway_data_dir() / "runs";
    std::filesystem::create_directories(dir);
    return dir;
}

auto approvals_dir() -> std::filesystem::path {
    auto dir = gateway_data_dir() / "approvals";
    std::filesystem::create_directories(dir);
    return dir;
}

auto audit_dir() -> std::filesystem::path {
    auto dir = gateway_data_dir() / "audit";
    std::filesystem::create_directories(dir);
    return dir;
}

auto timestamp() -> std::string {
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    localtime_r(&t, &tm);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S%z", &tm);
    return buf;
}

auto make_session_id(const std::string& channel_id,
                     const std::string& chat_id,
                     const std::string& thread_id) -> std::string {
    auto base = channel_id + ":" + chat_id;
    if (!thread_id.empty()) return base + ":" + thread_id;
    return base;
}

void persist_message(const std::string& session_id,
                     const nlohmann::json& entry) {
    auto path = sessions_dir() / (session_id + ".jsonl");
    std::ofstream f(path, std::ios::app);
    f << entry.dump() << '\n';
}

auto load_session_from_disk(const std::string& session_id)
    -> std::vector<nlohmann::json> {
    auto path = sessions_dir() / (session_id + ".jsonl");
    if (!std::filesystem::exists(path)) return {};

    std::vector<nlohmann::json> entries;
    std::ifstream f(path);
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty()) continue;
        try { entries.push_back(nlohmann::json::parse(line)); } catch (const std::exception&) { /* swallowed: best-effort path */ (void)0; }
    }
    return entries;
}

void persist_session_meta(const std::string& session_id,
                          const nlohmann::json& meta) {
    auto path = sessions_dir() / (session_id + ".meta.json");
    std::ofstream f(path);
    f << meta.dump(2);
}

auto load_session_meta(const std::string& session_id) -> nlohmann::json {
    auto path = sessions_dir() / (session_id + ".meta.json");
    if (!std::filesystem::exists(path)) return {};
    std::ifstream f(path);
    nlohmann::json j;
    try { f >> j; } catch (const std::exception&) { return {}; }
    return j;
}

void audit_log(const nlohmann::json& event) {
    auto path = audit_dir() / "gateway_audit.jsonl";
    std::ofstream f(path, std::ios::app);
    f << event.dump() << '\n';
}

void persist_run_event(const std::string& run_id,
                       const nlohmann::json& entry) {
    auto path = runs_dir() / (run_id + ".jsonl");
    std::ofstream f(path, std::ios::app);
    f << entry.dump() << '\n';
}

auto load_run_events(const std::string& run_id)
    -> std::vector<nlohmann::json> {
    auto path = runs_dir() / (run_id + ".jsonl");
    if (!std::filesystem::exists(path)) return {};

    std::vector<nlohmann::json> entries;
    std::ifstream f(path);
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty()) continue;
        try { entries.push_back(nlohmann::json::parse(line)); } catch (const std::exception&) { /* swallowed: best-effort path */ (void)0; }
    }
    return entries;
}

} // namespace euxis::gateway
