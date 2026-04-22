#include "euxis/gateway/mcp_fleet_tools.hpp"

#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

namespace euxis::gateway {
namespace {

/// Reject shell metacharacters, control chars, null bytes, >4096 length.
auto is_safe_arg(const std::string& arg) -> bool {
    if (arg.empty() || arg.size() > 4096) return false;
    for (unsigned char c : arg) {
        if (c == '\0') return false;
        if (c < 0x20 && c != '\t') return false;  // control chars except tab
        switch (c) {
            case ';': case '&': case '|': case '$': case '`':
            case '(': case ')': case '{': case '}':
            case '<': case '>': case '!': case '\\':
            case '\'': case '"': case '\n': case '\r':
                return false;
            default: break;
        }
    }
    return true;
}

/// Maximum output size (16 MB) to prevent OOM on unbounded child output (P2-3).
static constexpr size_t kMaxOutputBytes = 16u * 1024u * 1024u;

/// Execute an euxis subcommand via fork+execvp (no shell, no popen).
/// @param euxis_home  EUXIS_HOME to set in child environment
/// @param command     The euxis subcommand (e.g. "check", "triage")
/// @param args        Pre-validated argument vector (each element must pass is_safe_arg)
auto run_euxis_command(const std::string& euxis_home,
                        const std::string& command,
                        const std::vector<std::string>& args = {}) -> nlohmann::json {
    // Validate every argument
    for (const auto& a : args) {
        if (!is_safe_arg(a)) {
            return {{"error", "rejected: unsafe argument"}, {"command", command}};
        }
    }

    // Build argv: euxis <command> --json [args...]
    std::vector<std::string> argv_strings;
    argv_strings.push_back("euxis");
    // command may be multi-word like "agent list" — split on space
    {
        std::istringstream iss(command);
        std::string part;
        while (iss >> part) argv_strings.push_back(part);
    }
    argv_strings.push_back("--json");
    for (const auto& a : args) argv_strings.push_back(a);

    std::vector<const char*> argv;
    for (const auto& s : argv_strings) argv.push_back(s.c_str());
    argv.push_back(nullptr);

    // Set up stdout pipe
    int stdout_pipe[2];
    if (::pipe(stdout_pipe) != 0) {
        return {{"error", "pipe() failed"}, {"command", command}};
    }

    pid_t pid = ::fork();
    if (pid < 0) {
        ::close(stdout_pipe[0]);
        ::close(stdout_pipe[1]);
        return {{"error", "fork() failed"}, {"command", command}};
    }

    if (pid == 0) {
        // Child
        (void)::setsid();

        // Set EUXIS_HOME
        ::setenv("EUXIS_HOME", euxis_home.c_str(), 1);
        // Pass through mock execution for tests
        if (std::getenv("EUXIS_TEST_MOCK_EXECUTION")) {
            ::setenv("EUXIS_TEST_MOCK_EXECUTION", "1", 1);
        }

        // Redirect stdout to pipe, stderr to /dev/null
        ::close(stdout_pipe[0]);
        ::dup2(stdout_pipe[1], STDOUT_FILENO);
        ::close(stdout_pipe[1]);
        int devnull = ::open("/dev/null", O_WRONLY);
        if (devnull >= 0) { ::dup2(devnull, STDERR_FILENO); ::close(devnull); }

        ::execvp("euxis", const_cast<char* const*>(argv.data()));
        ::_exit(127);
    }

    // Parent
    ::close(stdout_pipe[1]);

    // Poll-based pipe drain with 300s timeout and 16MB output cap
    std::string output;
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(300);
    bool capped = false;
    {
        std::array<char, 8192> buf{};
        for (;;) {
            auto now = std::chrono::steady_clock::now();
            if (now >= deadline) break;
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now).count();
            int timeout_ms = static_cast<int>(std::min<long long>(ms, 2'000'000'000LL));

            struct pollfd pfd{stdout_pipe[0], POLLIN, 0};
            int pr = ::poll(&pfd, 1, timeout_ms);
            if (pr <= 0) break;

            ssize_t n = ::read(stdout_pipe[0], buf.data(), buf.size());
            if (n <= 0) break;
            if (output.size() + static_cast<size_t>(n) > kMaxOutputBytes) {
                output.append(buf.data(), kMaxOutputBytes - output.size());
                capped = true;
                break;
            }
            output.append(buf.data(), static_cast<size_t>(n));
        }
    }
    ::close(stdout_pipe[0]);

    // Reap child
    int status = 0;
    if (::waitpid(pid, &status, WNOHANG) == 0) {
        // Child still running — kill group
        ::kill(-pid, SIGTERM);
        ::usleep(100'000);
        if (::waitpid(pid, &status, WNOHANG) == 0) {
            ::kill(-pid, SIGKILL);
            ::waitpid(pid, &status, 0);
        }
    }

    if (capped) {
        return {{"error", "output exceeded 16MB limit"}, {"command", command}};
    }

    try {
        // Find the JSON in the output (may have TUI prefix)
        auto json_start = output.rfind("\n{");
        if (json_start == std::string::npos) json_start = output.find("{");
        else json_start++;
        if (json_start != std::string::npos) {
            return nlohmann::json::parse(output.substr(json_start));
        }
        int exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
        return {{"output", output}, {"exit_code", exit_code}};
    } catch (...) {
        int exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
        return {{"output", output}, {"exit_code", exit_code}};
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
            return run_euxis_command(euxis_home, "check", {target});
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
            return run_euxis_command(euxis_home, "triage", {target});
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
            std::vector<std::string> cmd_args = {target};
            if (args.value("forensic", false)) cmd_args.push_back("--forensic");
            return run_euxis_command(euxis_home, "review", cmd_args);
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
            std::vector<std::string> cmd_args = {manifest};
            if (args.contains("goal")) cmd_args.push_back(args["goal"].get<std::string>());
            if (args.contains("mode")) {
                cmd_args.push_back("--mode");
                cmd_args.push_back(args["mode"].get<std::string>());
            }
            return run_euxis_command(euxis_home, "playbook", cmd_args);
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
