#pragma once

#include <chrono>
#include <filesystem>
#include <string>

#include <nlohmann/json.hpp>

namespace euxis::bridge {

class AuditLogger {
public:
    explicit AuditLogger(std::filesystem::path log_path);

    void log(const std::string& event_type,
             const std::string& skill_name,
             const nlohmann::json& details = {});

    void log_admission(const std::string& skill_name,
                       bool admitted,
                       const nlohmann::json& details = {});

    void log_execution(const std::string& skill_name,
                       int exit_code,
                       std::chrono::milliseconds duration,
                       const nlohmann::json& details = {});

    [[nodiscard]] auto path() const -> const std::filesystem::path&;

private:
    std::filesystem::path log_path_;
    void append(const nlohmann::json& entry);
    [[nodiscard]] static auto now_iso8601() -> std::string;
};

}  // namespace euxis::bridge
