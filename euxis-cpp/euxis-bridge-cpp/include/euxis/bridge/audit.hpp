/// @file
/// @brief Audit logging for bridge events and skill executions.
#pragma once

#include <chrono>
#include <filesystem>
#include <string>

#include <nlohmann/json.hpp>

namespace euxis::bridge {

/// @brief Records security-sensitive events and execution history to a persistent log.
class AuditLogger {
public:
    /// @brief Construct logger targeting a specific file path.
    explicit AuditLogger(std::filesystem::path log_path);

    /// @brief Log a generic event.
    void log(const std::string& event_type,
             const std::string& skill_name,
             const nlohmann::json& details = {});

    /// @brief Specialized log for admission decisions.
    void log_admission(const std::string& skill_name,
                       bool admitted,
                       const nlohmann::json& details = {});

    /// @brief Specialized log for execution results.
    void log_execution(const std::string& skill_name,
                       int exit_code,
                       std::chrono::milliseconds duration,
                       const nlohmann::json& details = {});

    /// @brief Get the configured log path.
    auto path() const -> const std::filesystem::path&;

private:
    std::filesystem::path log_path_;

    void append(const nlohmann::json& entry);
    static auto now_iso8601() -> std::string;
};

} // namespace euxis::bridge
