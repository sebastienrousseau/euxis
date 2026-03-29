#include "euxis/gateway/mcp_fleet_tools.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <array>

namespace euxis::gateway {
namespace {

auto run_euxis_command(const std::string& euxis_home, const std::string& command,
                        const std::string& args = "") -> nlohmann::json {
    // Build command — uses the euxis CLI with --json flag
    std::string cmd = "EUXIS_HOME=" + euxis_home;
    // Pass through mock execution for tests
    if (std::getenv("EUXIS_TEST_MOCK_EXECUTION")) {
        cmd += " EUXIS_TEST_MOCK_EXECUTION=1";
    }
    cmd += " euxis " + command + " --json";
    if (!args.empty()) cmd += " " + args;
    cmd += " 2>/dev/null";

    std::array<char, 8192> buffer{};
    std::string output;
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        return {{"error", "failed to execute euxis command"}, {"command", command}};
    }
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        output += buffer.data();
    }
    int status = pclose(pipe);

    try {
        // Find the JSON in the output (may have TUI prefix)
        auto json_start = output.rfind("\n{");
        if (json_start == std::string::npos) json_start = output.find("{");
        else json_start++;
        if (json_start != std::string::npos) {
            return nlohmann::json::parse(output.substr(json_start));
        }
        return {{"output", output}, {"exit_code", WEXITSTATUS(status)}};
    } catch (...) {
        return {{"output", output}, {"exit_code", WEXITSTATUS(status)}};
    }
}

} // namespace

void register_fleet_tools(McpHost& host, const std::string& euxis_home) {
    // euxis.check — run a standard check
    host.register_tool(
        "euxis.check",
        "Run euxis check (standard mode verdict) on a target",
        {{"type", "object"}, {"properties", {
            {"target", {{"type", "string"}, {"description", "Path to target repository"}}}
        }}},
        [euxis_home](const nlohmann::json& args) -> nlohmann::json {
            std::string target = args.value("target", ".");
            return run_euxis_command(euxis_home, "check", target);
        }
    );

    // euxis.triage — run flash triage
    host.register_tool(
        "euxis.triage",
        "Run euxis triage (flash mode) on a target",
        {{"type", "object"}, {"properties", {
            {"target", {{"type", "string"}, {"description", "Path to target repository"}}}
        }}},
        [euxis_home](const nlohmann::json& args) -> nlohmann::json {
            std::string target = args.value("target", ".");
            return run_euxis_command(euxis_home, "triage", target);
        }
    );

    // euxis.review — run deep review
    host.register_tool(
        "euxis.review",
        "Run euxis review (standard/forensic mode) on a target",
        {{"type", "object"}, {"properties", {
            {"target", {{"type", "string"}, {"description", "Path to target repository"}}},
            {"forensic", {{"type", "boolean"}, {"description", "Use forensic mode"}}}
        }}},
        [euxis_home](const nlohmann::json& args) -> nlohmann::json {
            std::string target = args.value("target", ".");
            std::string extra;
            if (args.value("forensic", false)) extra = "--forensic";
            return run_euxis_command(euxis_home, "review", target + " " + extra);
        }
    );

    // euxis.stats — get verdict statistics
    host.register_tool(
        "euxis.stats",
        "Get euxis verdict statistics",
        {{"type", "object"}, {"properties", {}}},
        [euxis_home](const nlohmann::json&) -> nlohmann::json {
            return run_euxis_command(euxis_home, "stats");
        }
    );

    // euxis.agent_list — list registered agents
    host.register_tool(
        "euxis.agent_list",
        "List all registered euxis agents",
        {{"type", "object"}, {"properties", {}}},
        [euxis_home](const nlohmann::json&) -> nlohmann::json {
            return run_euxis_command(euxis_home, "agent list");
        }
    );

    // euxis.playbook — run a playbook
    host.register_tool(
        "euxis.playbook",
        "Run an euxis playbook",
        {{"type", "object"}, {"properties", {
            {"manifest", {{"type", "string"}, {"description", "Playbook name or path"}}},
            {"goal", {{"type", "string"}, {"description", "Goal/target for the playbook"}}},
            {"mode", {{"type", "string"}, {"description", "Execution mode: flash, standard, forensic"}}}
        }}, {"required", {"manifest"}}},
        [euxis_home](const nlohmann::json& args) -> nlohmann::json {
            std::string manifest = args.value("manifest", "verify-everything");
            std::string extra = manifest;
            if (args.contains("goal")) extra += " " + args["goal"].get<std::string>();
            if (args.contains("mode")) extra += " --mode " + args["mode"].get<std::string>();
            return run_euxis_command(euxis_home, "playbook", extra);
        }
    );

    // Register resources
    host.register_resource(
        "euxis://verdict/latest", "Latest Verdict",
        "Most recent verdict artifact", "application/json",
        [euxis_home](const std::string&) -> nlohmann::json {
            auto path = euxis_home + "/data/runtime/sessions/latest_verdict.json";
            std::ifstream f(path);
            if (!f.is_open()) return {{"error", "no verdict found"}};
            try { return nlohmann::json::parse(f); }
            catch (...) { return {{"error", "invalid verdict file"}}; }
        }
    );

    host.register_resource(
        "euxis://verdict/history", "Verdict History",
        "Historical verdict entries", "application/jsonl",
        [euxis_home](const std::string&) -> nlohmann::json {
            auto path = euxis_home + "/data/runtime/sessions/history.jsonl";
            std::ifstream f(path);
            if (!f.is_open()) return {{"entries", nlohmann::json::array()}};
            nlohmann::json entries = nlohmann::json::array();
            std::string line;
            while (std::getline(f, line)) {
                if (line.empty()) continue;
                try { entries.push_back(nlohmann::json::parse(line)); }
                catch (...) { continue; }
            }
            return {{"entries", entries}};
        }
    );

    host.register_resource(
        "euxis://agents", "Agent Registry",
        "Registered agents", "application/json",
        [euxis_home](const std::string&) -> nlohmann::json {
            auto path = euxis_home + "/data/agents/registry.json";
            std::ifstream f(path);
            if (!f.is_open()) return {{"agents", nlohmann::json::array()}};
            try { return nlohmann::json::parse(f); }
            catch (...) { return {{"error", "invalid registry"}}; }
        }
    );

    host.register_resource(
        "euxis://playbooks", "Playbooks",
        "Available playbook manifests", "application/json",
        [euxis_home](const std::string&) -> nlohmann::json {
            namespace fs = std::filesystem;
            nlohmann::json playbooks = nlohmann::json::array();
            auto dir = fs::path(euxis_home) / "data" / "config" / "playbooks";
            if (fs::exists(dir)) {
                for (const auto& entry : fs::directory_iterator(dir)) {
                    if (entry.path().extension() == ".json") {
                        playbooks.push_back(entry.path().stem().string());
                    }
                }
            }
            return {{"playbooks", playbooks}};
        }
    );

    host.register_resource(
        "euxis://config/router", "Router Config",
        "Current routing configuration", "application/json",
        [euxis_home](const std::string&) -> nlohmann::json {
            auto path = euxis_home + "/data/config/router.json";
            std::ifstream f(path);
            if (!f.is_open()) return {{"error", "router config not found"}};
            try { return nlohmann::json::parse(f); }
            catch (...) { return {{"error", "invalid config"}}; }
        }
    );

    host.register_resource(
        "euxis://config/targets", "Baseline Targets",
        "Performance baseline targets", "application/json",
        [euxis_home](const std::string&) -> nlohmann::json {
            auto path = euxis_home + "/data/config/targets.json";
            std::ifstream f(path);
            if (!f.is_open()) return {{"error", "targets not found"}};
            try { return nlohmann::json::parse(f); }
            catch (...) { return {{"error", "invalid targets"}}; }
        }
    );
}

} // namespace euxis::gateway
