#pragma once

#include <string>

namespace euxis::cli {

/// Project and session directory management.
class Session {
public:
    explicit Session(const std::string& euxis_home);

    /// Get the current project name (EUXIS_PROJECT or PWD basename).
    [[nodiscard]] auto project_name() const -> std::string;

    /// Get the or create a session ID (EUXIS_SESSION_ID or timestamp).
    [[nodiscard]] auto session_id() const -> std::string;

    /// Get the root euxis_home directory.
    [[nodiscard]] auto euxis_home() const -> std::string;

    /// Ensure project/agent directories exist. Returns the project dir.
    [[nodiscard]] auto ensure_project_dirs(const std::string& agent_id) const -> std::string;

    /// Get the last N lines of agent memory context.
    [[nodiscard]] auto get_memory_context(const std::string& agent_id, int lines = 50) const -> std::string;

private:
    std::string euxis_home_;
};

} // namespace euxis::cli
