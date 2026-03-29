#include <euxis/bridge/audit.hpp>

#include <fstream>
#include <iomanip>
#include <sstream>

#include <sodium.h>

namespace euxis::bridge {

auto AuditLogger::compute_hash(const std::string& data) -> std::string {
    unsigned char hash[crypto_hash_sha256_BYTES];
    crypto_hash_sha256(
        hash,
        reinterpret_cast<const unsigned char*>(data.data()),
        data.size()
    );
    std::ostringstream ss;
    for (size_t i = 0; i < crypto_hash_sha256_BYTES; ++i) {
        ss << std::hex << std::setfill('0') << std::setw(2)
           << static_cast<int>(hash[i]);
    }
    return ss.str();
}

AuditLogger::AuditLogger(std::filesystem::path log_path)
    : log_path_(std::move(log_path)) {
    // Ensure parent directory exists
    auto parent = log_path_.parent_path();
    if (!parent.empty() && !std::filesystem::exists(parent)) {
        std::filesystem::create_directories(parent);
    }

    // Scan existing log to compute chain head hash
    if (std::filesystem::exists(log_path_)) {
        std::ifstream file(log_path_);
        std::string line;
        while (std::getline(file, line)) {
            if (!line.empty()) {
                prev_hash_ = compute_hash(line);
            }
        }
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

auto AuditLogger::chain_head_hash() const -> std::string {
    return prev_hash_;
}

auto AuditLogger::verify_chain(const std::filesystem::path& log_path) -> bool {
    if (!std::filesystem::exists(log_path)) {
        return true;  // Empty/missing file is valid
    }

    std::ifstream file(log_path);
    std::string line;
    std::string expected_prev_hash;

    while (std::getline(file, line)) {
        if (line.empty()) continue;

        try {
            auto entry = nlohmann::json::parse(line);
            std::string entry_prev_hash = entry.value("prev_hash", "");

            if (entry_prev_hash != expected_prev_hash) {
                return false;  // Chain broken
            }

            expected_prev_hash = compute_hash(line);
        } catch (const nlohmann::json::parse_error&) {
            return false;  // Corrupted entry
        }
    }

    return true;
}

void AuditLogger::append(const nlohmann::json& entry) {
    nlohmann::json chained = entry;
    chained["prev_hash"] = prev_hash_;

    std::string serialized = chained.dump();
    prev_hash_ = compute_hash(serialized);

    std::ofstream file(log_path_, std::ios::app);
    if (file.is_open()) {
        file << serialized << '\n';
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
