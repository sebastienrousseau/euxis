/// @file
/// @brief Gateway state
#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace euxis::gateway {

auto gateway_data_dir() -> std::filesystem::path;
auto sessions_dir() -> std::filesystem::path;
auto runs_dir() -> std::filesystem::path;
auto approvals_dir() -> std::filesystem::path;
auto audit_dir() -> std::filesystem::path;

auto timestamp() -> std::string;
auto make_session_id(const std::string& channel_id,
                     const std::string& chat_id,
                     const std::string& thread_id = {}) -> std::string;

void persist_message(const std::string& session_id,
                     const nlohmann::json& entry);
auto load_session_from_disk(const std::string& session_id)
    -> std::vector<nlohmann::json>;

void persist_session_meta(const std::string& session_id,
                          const nlohmann::json& meta);
auto load_session_meta(const std::string& session_id) -> nlohmann::json;

void audit_log(const nlohmann::json& event);

void persist_run_event(const std::string& run_id, const nlohmann::json& entry);
auto load_run_events(const std::string& run_id) -> std::vector<nlohmann::json>;

} // namespace euxis::gateway
