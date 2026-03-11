#include <euxis/bridge/audit.hpp>

#include <fstream>

namespace euxis::bridge {

AuditLogger::AuditLogger(std::filesystem::path log_path)
    : log_path_(std::move(log_path)) {
    // Ensure parent directory exists
    auto parent = log_path_.parent_path();
    if (!parent.empty() && !std::filesystem::exists(parent)) {
        std::filesystem::create_directories(parent);
    }
}

void AuditLogger::log(const std::string& event_type,
                      const std::string& skill_name,
                      const nlohmann::json& details) {
    nlohmann::json entry;
    entry["timestamp"] = now_iso8601();
    entry["event_type"] = event_type;
    entry["skill_name"] = skill_name;
    entry["details"] = details;
    append(entry);
}

void AuditLogger::log_admission(const std::string& skill_name,
                                bool admitted,
                                const nlohmann::json& details) {
    nlohmann::json entry;
    entry["timestamp"] = now_iso8601();
    entry["event_type"] = "admission";
    entry["skill_name"] = skill_name;
    entry["admitted"] = admitted;
    entry["details"] = details;
    append(entry);
}

void AuditLogger::log_execution(const std::string& skill_name,
                                int exit_code,
                                std::chrono::milliseconds duration,
                                const nlohmann::json& details) {
    nlohmann::json entry;
    entry["timestamp"] = now_iso8601();
    entry["event_type"] = "execution";
    entry["skill_name"] = skill_name;
    entry["exit_code"] = exit_code;
    entry["duration_ms"] = duration.count();
    entry["details"] = details;
    append(entry);
}

auto AuditLogger::path() const -> const std::filesystem::path& {
    return log_path_;
}

void AuditLogger::append(const nlohmann::json& entry) {
    std::ofstream file(log_path_, std::ios::app);
    if (file.is_open()) {
        file << entry.dump() << '\n';
    }
}

auto AuditLogger::now_iso8601() -> std::string {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    gmtime_r(&time_t, &tm);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
    return buf;
}

}  // namespace euxis::bridge
