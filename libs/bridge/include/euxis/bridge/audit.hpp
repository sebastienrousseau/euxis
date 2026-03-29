/// @file
/// @brief Audit logging for bridge events and skill executions.
#pragma once

#include <chrono>
#include <filesystem>
#include <string>

#include <nlohmann/json.hpp>

namespace euxis::bridge {

/// @brief Records security-sensitive events and execution history to a persistent log.
///
/// Each log entry contains a `prev_hash` field linking it to the SHA-256 hash of the
/// previous entry, forming a tamper-evident hash chain.
class AuditLogger {
public:
    /// @brief Construct logger targeting a specific file path.
    /// Scans existing log to compute the chain head hash.
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
    [[nodiscard]] auto path() const -> const std::filesystem::path&;

    /// @brief Get the current chain head hash.
    [[nodiscard]] auto chain_head_hash() const -> std::string;

    /// @brief Verify integrity of an entire audit log file.
    /// Returns true if the hash chain is unbroken (empty file is valid).
    [[nodiscard]] static auto verify_chain(const std::filesystem::path& log_path) -> bool;

private:
    std::filesystem::path log_path_;
    std::string prev_hash_;

    void append(const nlohmann::json& entry);
    static auto now_iso8601() -> std::string;
    static auto compute_hash(const std::string& data) -> std::string;
};

} // namespace euxis::bridge
