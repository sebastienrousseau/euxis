#include "euxis/cli/cmd/fleet.hpp"
#include "euxis/cli/config_loader.hpp"
#include "euxis/cli/i18n.hpp"
#include "euxis/cli/process.hpp"
#include "euxis/cli/provider_executor.hpp"
#include "euxis/cli/provider_router.hpp"
#include "euxis/cli/registry_client.hpp"
#include "euxis/cli/session.hpp"
#include "euxis/cli/terminal.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <unordered_map>

using euxis::cli::i18n::tr;

namespace euxis::cli::cmd {
namespace {

namespace fs = std::filesystem;
namespace term = terminal;

/// Write content to a file, creating parent directories as needed.
void write_output_file(const fs::path& path, const std::string& content) {
    fs::create_directories(path.parent_path());
    std::ofstream f(path);
    f << content;
}

/// Read the entire contents of a file into a string. Returns empty on failure.
auto read_file_contents(const fs::path& path) -> std::string {
    if (!fs::exists(path)) return {};
    std::ifstream f(path);
    return {std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}

/// Simple convergence check: ratio of changed characters to total length.
auto has_converged(const std::string& prev, const std::string& curr,
                   double threshold = 0.05) -> bool {
    if (prev.empty() || curr.empty()) return false;
    if (prev == curr) return true;
    // Count differing characters up to the shorter length
    auto min_len = std::min(prev.size(), curr.size());
    auto max_len = std::max(prev.size(), curr.size());
    size_t diffs = max_len - min_len; // length difference counts as diffs
    for (size_t i = 0; i < min_len; ++i) {
        if (prev[i] != curr[i]) ++diffs;
    }
    double ratio = static_cast<double>(diffs) / static_cast<double>(max_len);
    return ratio < threshold;
}

} // namespace

// --- agent ---

int cmd_agent(Context& ctx, const std::vector<std::string>& args) {
    RegistryClient registry(ctx.data_dir);

    if (args.empty() || args[0] == "list") {
        auto agents = registry.list_agents();
        if (ctx.json_output) {
            nlohmann::json j = nlohmann::json::array();
            for (const auto& a : agents) {
                j.push_back({{"id", a.id}, {"role", a.role}, {"tier", a.tier}, {"version", a.version}});
            }
            std::cout << j.dump(2) << "\n";
            return 0;
        }

        std::cout << term::bold(tr("Registered Agents")) << " (" << agents.size() << ")\n\n";
        std::vector<term::TableRow> rows;
        for (const auto& a : agents) {
            rows.push_back({{a.id, a.role, a.tier, a.version}});
        }
        term::print_table({tr("ID"), tr("Role"), tr("Tier"), tr("Version")}, rows);
        return 0;
    }

    if (args[0] == "register" && args.size() >= 2) {
        auto manifest_path = args[1];
        if (!fs::exists(manifest_path)) {
            std::cerr << tr("Manifest not found: ") << manifest_path << "\n";
            return 1;
        }
        std::ifstream f(manifest_path);
        nlohmann::json manifest;
        try {
            manifest = nlohmann::json::parse(f);
        } catch (const std::exception& e) {
            std::cerr << tr("Invalid JSON: ") << e.what() << "\n";
            return 1;
        }
        auto agent_id = manifest.value("agent_id", manifest.value("id", ""));
        if (agent_id.empty()) {
            std::cerr << tr("Manifest missing agent_id") << "\n";
            return 1;
        }
        if (registry.register_plugin(agent_id, manifest)) {
            std::cout << term::icon_ok() << " " << tr("Registered: ") << agent_id << "\n";
            return 0;
        }
        std::cerr << tr("Registration failed") << "\n";
        return 1;
    }

    if (args[0] == "unregister" && args.size() >= 2) {
        if (registry.unregister_plugin(args[1])) {
            std::cout << term::icon_ok() << " " << tr("Unregistered: ") << args[1] << "\n";
            return 0;
        }
        std::cerr << tr("Agent not found: ") << args[1] << "\n";
        return 1;
    }

    if (args[0] == "info" && args.size() >= 2) {
        auto agent = registry.get_agent(args[1]);
        if (!agent) {
            std::cerr << tr("Agent not found: ") << args[1] << "\n";
            return 1;
        }
        if (ctx.json_output) {
            nlohmann::json j;
            j["id"] = agent->id;
            j["role"] = agent->role;
            j["tier"] = agent->tier;
            j["version"] = agent->version;
            j["tags"] = agent->tags;
            j["capabilities"] = agent->capabilities;
            j["prompt_path"] = agent->prompt_path;
            std::cout << j.dump(2) << "\n";
            return 0;
        }
        std::cout << term::bold(agent->id) << "\n"
                  << "  " << tr("Role:") << "    " << agent->role << "\n"
                  << "  " << tr("Tier:") << "    " << agent->tier << "\n"
                  << "  " << tr("Version:") << " " << agent->version << "\n";
        if (!agent->tags.empty()) {
            std::cout << "  " << tr("Tags:") << "    ";
            for (size_t i = 0; i < agent->tags.size(); ++i) {
                if (i > 0) std::cout << ", ";
                std::cout << agent->tags[i];
            }
            std::cout << "\n";
        }
        return 0;
    }

    std::cerr << tr("Usage: euxis agent <list|register|unregister|info> [args]") << "\n";
    return 2;
}

// --- agent-bootstrap ---

int cmd_agent_bootstrap(Context& ctx, const std::vector<std::string>& args) {
    if (args.empty()) {
        std::cerr << tr("Usage: euxis agent-bootstrap <agent-id> [--tier TIER]") << "\n";
        return 2;
    }

    std::string agent_id = args[0];
    std::string tier = "code";
    for (size_t i = 1; i < args.size(); ++i) {
        if (args[i] == "--tier" && i + 1 < args.size()) {
            tier = args[++i];
        }
    }

    // Validate agent_id
    for (char c : agent_id) {
        if (!std::isalnum(c) && c != '-' && c != '_') {
            std::cerr << tr("Invalid agent-id (alphanumeric, dash, underscore only)") << "\n";
            return 1;
        }
    }

    auto prompt_dir = fs::path(ctx.euxis_home) / "data" / "agents" / "prompts" / "fleet";
    fs::create_directories(prompt_dir);
    auto prompt_path = prompt_dir / (agent_id + ".md");

    if (fs::exists(prompt_path)) {
        std::cerr << tr("Agent already exists: ") << prompt_path.string() << "\n";
        return 1;
    }

    std::ofstream f(prompt_path);
    f << "---\n"
      << "agent_id: " << agent_id << "\n"
      << "role: \"Custom agent\"\n"
      << "version: \"1.0.0\"\n"
      << "tier: " << tier << "\n"
      << "tags: [custom]\n"
      << "last_updated: 2026-03-01\n"
      << "---\n\n"
      << "# " << agent_id << "\n\n"
      << "## Role\nDescribe this agent's purpose.\n\n"
      << "## Instructions\nAdd instructions here.\n";

    std::cout << term::icon_ok() << " " << tr("Created: ") << prompt_path.string() << "\n";
    return 0;
}

// --- squad ---

int cmd_squad(Context& ctx, const std::vector<std::string>& args) {
    RegistryClient registry(ctx.data_dir);

    if (args.empty() || args[0] == "list") {
        auto squads = registry.list_squads();
        if (ctx.json_output) {
            nlohmann::json j = nlohmann::json::array();
            for (const auto& s : squads) {
                j.push_back({{"id", s.id}, {"name", s.name}, {"purpose", s.purpose},
                             {"members", s.members.size()}});
            }
            std::cout << j.dump(2) << "\n";
            return 0;
        }

        std::cout << term::bold(tr("Squads")) << " (" << squads.size() << ")\n\n";
        std::vector<term::TableRow> rows;
        for (const auto& s : squads) {
            rows.push_back({{s.id, s.name, s.purpose, std::to_string(s.members.size())}});
        }
        term::print_table({tr("ID"), tr("Name"), tr("Purpose"), tr("Members")}, rows);
        return 0;
    }

    if (args[0] == "info" && args.size() >= 2) {
        auto squad = registry.get_squad(args[1]);
        if (!squad) {
            std::cerr << tr("Squad not found: ") << args[1] << "\n";
            return 1;
        }
        std::cout << term::bold(squad->name) << " (" << squad->id << ")\n"
                  << "  " << tr("Purpose:") << " " << squad->purpose << "\n"
                  << "  " << tr("Lead:") << "    " << squad->lead << "\n"
                  << "  " << tr("Members:") << " " << squad->members.size() << "\n";
        for (const auto& m : squad->members) {
            std::cout << "    - " << m << (m == squad->lead ? std::string(" (") + tr("lead") + ")" : "") << "\n";
        }
        return 0;
    }

    if (args[0] == "deploy" && args.size() >= 3) {
        auto squad = registry.get_squad(args[1]);
        if (!squad) {
            std::cerr << tr("Squad not found: ") << args[1] << "\n";
            return 1;
        }
        std::string task = args[2];
        std::string mode = "hierarchical";
        for (size_t i = 3; i < args.size(); ++i) {
            if (args[i] == "--mode" && i + 1 < args.size()) mode = args[++i];
        }

        std::cout << term::bold(tr("Deploying squad: ")) << squad->name << "\n"
                  << "  " << tr("Task:") << " " << task << "\n"
                  << "  " << tr("Mode:") << " " << mode << "\n"
                  << "  " << tr("Members:") << " " << squad->members.size() << "\n\n";

        // Generate dispatch manifest
        Session session(ctx.euxis_home);
        auto project_dir = session.ensure_project_dirs(squad->lead);
        auto manifest_path = fs::path(project_dir) / "output" / "squad-manifest.json";

        nlohmann::json manifest;
        manifest["squad"] = squad->id;
        manifest["task"] = task;
        manifest["mode"] = mode;
        manifest["agents"] = nlohmann::json::array();
        for (const auto& m : squad->members) {
            nlohmann::json entry;
            entry["agent_id"] = m;
            entry["priority"] = (m == squad->lead) ? "P0" : "P1";
            manifest["agents"].push_back(entry);
        }

        std::ofstream f(manifest_path);
        f << manifest.dump(2);

        std::cout << term::icon_ok() << " " << tr("Manifest: ") << manifest_path.string() << "\n";
        return 0;
    }

    if (args[0] == "members" && args.size() >= 2) {
        auto squad = registry.get_squad(args[1]);
        if (!squad) {
            std::cerr << tr("Squad not found: ") << args[1] << "\n";
            return 1;
        }
        std::vector<term::TableRow> rows;
        for (const auto& m : squad->members) {
            auto agent = registry.get_agent(m);
            std::string role_str = (m == squad->lead) ? tr("lead") : tr("member");
            std::string tier_str = agent ? agent->tier : "?";
            rows.push_back({{m, role_str, tier_str}});
        }
        term::print_table({tr("Agent"), tr("Role"), tr("Tier")}, rows);
        return 0;
    }

    if (args[0] == "validate") {
        auto squads = registry.list_squads();
        int issues = 0;
        for (const auto& sq : squads) {
            for (const auto& m : sq.members) {
                auto agent = registry.get_agent(m);
                if (!agent) {
                    std::cout << term::icon_fail() << " " << sq.id << ": " << tr("member '")
                              << m << tr("' not in registry") << "\n";
                    ++issues;
                }
            }
        }
        if (issues == 0) std::cout << term::icon_ok() << " " << tr("All squad members valid") << "\n";
        return issues > 0 ? 1 : 0;
    }

    std::cerr << tr("Usage: euxis squad <list|info|deploy|members|validate> [args]") << "\n";
    return 2;
}

// --- combo ---

int cmd_combo(Context& ctx, const std::vector<std::string>& args) {
    if (args.empty()) {
        std::cerr << tr("Usage: euxis combo <agent1,agent2,...> <task>") << "\n";
        return 2;
    }

    // Parse comma-separated agent list
    std::vector<std::string> agent_ids;
    std::string agents_str = args[0];
    size_t start = 0;
    while (start < agents_str.size()) {
        size_t end = agents_str.find(',', start);
        if (end == std::string::npos) end = agents_str.size();
        agent_ids.push_back(agents_str.substr(start, end - start));
        start = end + 1;
    }

    std::string task = args.size() > 1 ? args[1] : tr("default task");

    std::cout << term::bold(tr("Running Combo Pipeline")) << "\n"
              << "  " << tr("Agents:") << " " << agent_ids.size() << "\n"
              << "  " << tr("Task:") << "   " << task << "\n\n";

    RegistryClient registry(ctx.data_dir);
    ProviderRouter router(ctx.data_dir);
    ProviderExecutor executor(ctx.data_dir);
    Session session(ctx.euxis_home);

    std::string previous_output;
    int failures = 0;

    for (size_t i = 0; i < agent_ids.size(); ++i) {
        const auto& aid = agent_ids[i];
        auto agent = registry.get_agent(aid);
        auto model = router.route(agent ? agent->tier : "code", task);

        std::cout << "  [" << (i + 1) << "/" << agent_ids.size() << "] "
                  << term::cyan(aid) << " -> " << model.model << "\n";

        // Load agent system prompt
        std::string system_prompt;
        if (agent && !agent->prompt_path.empty()) {
            system_prompt = ProviderExecutor::load_agent_prompt(ctx.euxis_home, agent->prompt_path);
        }

        // Build prompt: task + previous agent's output as context
        auto combined_prompt = ProviderExecutor::build_prompt(system_prompt, task, previous_output);

        // Invoke provider with spinner
        int frame = 0;
        term::spinner_frame(frame++, tr("Invoking ") + aid + "...");
        auto response = executor.execute(model, combined_prompt);
        term::spinner_clear();

        if (response.success) {
            previous_output = response.output;

            // Write output to session dir
            auto project_dir = session.ensure_project_dirs(aid);
            auto output_path = fs::path(project_dir) / "output" / "combo-output.md";
            write_output_file(output_path, response.output);

            std::cout << "    " << term::icon_ok() << " "
                      << response.output.size() << " " << tr("chars") << ", "
                      << term::format_duration(response.duration_ms) << "\n";
        } else {
            std::cerr << "    " << term::icon_fail() << " " << aid << ": "
                      << response.error << "\n";
            ++failures;
            // Continue pipeline with empty context on failure
        }
    }

    std::cout << "\n";
    if (failures > 0) {
        std::cout << term::icon_warn() << " " << tr("Combo pipeline finished with ")
                  << failures << tr(" failure(s)") << "\n";
        return 1;
    }
    std::cout << term::icon_ok() << " " << tr("Combo pipeline complete") << "\n";
    return 0;
}

// --- playbook ---

int cmd_playbook(Context& ctx, const std::vector<std::string>& args) {
    if (args.empty()) {
        std::cerr << tr("Usage: euxis playbook <manifest.json> [goal]") << "\n";
        return 2;
    }

    auto manifest_path_str = args[0];
    fs::path manifest_path(manifest_path_str);
    std::string goal = (args.size() > 1) ? args[1] : "Audit and improve the codebase";

    if (!fs::exists(manifest_path)) {
        // Standard Euxis search path for playbooks
        auto std_path = fs::path(ctx.data_dir) / "config" / "playbooks" / (manifest_path_str + ".json");
        if (fs::exists(std_path)) {
            manifest_path = std_path;
        } else {
            std::cerr << tr("Manifest not found: ") << manifest_path_str << "\n";
            return 1;
        }
    }

    std::ifstream f(manifest_path);
    nlohmann::json manifest;
    try {
        manifest = nlohmann::json::parse(f);
    } catch (const std::exception& e) {
        std::cerr << tr("Invalid JSON: ") << e.what() << "\n";
        return 1;
    }

    std::cout << term::bold(tr("Executing Playbook")) << "\n"
              << "  " << tr("Source:") << " " << manifest_path << "\n";

    nlohmann::json steps;
    if (manifest.contains("steps") && manifest["steps"].is_array()) {
        steps = manifest["steps"];
    } else if (manifest.contains("phases") && manifest["phases"].is_array()) {
        // Map phases to a flat steps array for backward compatibility with executor
        for (const auto& phase : manifest["phases"]) {
            std::string phase_name = phase.value("name", "Phase");
            if (phase.contains("delegates") && phase["delegates"].is_array()) {
                for (const auto& d : phase["delegates"]) {
                    nlohmann::json step;
                    step["name"] = phase_name + " / " + d.value("agent", "delegate");
                    step["agent"] = d.value("agent", "");
                    step["task"] = d.value("task_template", "");
                    steps.push_back(step);
                }
            }
        }
    }

    if (steps.empty()) {
        std::cerr << tr("Manifest missing 'steps' or 'phases' array") << "\n";
        return 1;
    }

    std::cout << "  " << tr("Steps:") << "  " << steps.size() << "\n\n";

    RegistryClient registry(ctx.data_dir);
    ProviderRouter router(ctx.data_dir);
    ProviderExecutor executor(ctx.data_dir);
    Session session(ctx.euxis_home);

    // Track step outputs keyed by step name for dependency resolution
    std::unordered_map<std::string, std::string> step_outputs;
    int failures = 0;
    std::string session_failover_provider;

    for (size_t i = 0; i < steps.size(); ++i) {
        const auto& step = steps[i];
        std::string name = step.value("name", "step-" + std::to_string(i + 1));
        std::string agent_id = step.value("agent", "");
        std::string step_task = step.value("task", manifest.value("task", ""));
        
        // Simple interpolation for ${goal}
        size_t pos = 0;
        while ((pos = step_task.find("${goal}", pos)) != std::string::npos) {
            step_task.replace(pos, 7, goal);
            pos += goal.length();
        }

        std::cout << "  [" << (i + 1) << "/" << steps.size() << "] "
                  << term::cyan(name) << " (" << tr("agent:") << " " << agent_id << ")\n";

        if (agent_id.empty()) {
            std::cerr << "    " << term::icon_fail() << " " << tr("Step missing 'agent' field") << "\n";
            ++failures;
            continue;
        }

        auto agent = registry.get_agent(agent_id);
        auto tier = agent ? agent->tier : "code";
        auto model = router.route(tier, step_task);

        // --- Session-Wide Failover Logic ---
        if (!session_failover_provider.empty()) {
            model.provider = session_failover_provider;
            if (model.provider == "ollama") model.model = "qwen2.5-coder:7b";
            else if (model.provider == "gemini") model.model = "gemini-2.0-flash-lite";
        }

        // --- Provider Cooldown Check ---
        auto auth = executor.auth_store().resolve_with_fallback(model.provider);
        if (auth.has_value() && executor.auth_store().is_cooled_down(auth->profile_id)) {
            std::cout << term::dim("    \xe2\x9a\xa0  " + model.provider + " is in cooldown. Switching to local failover...\n");
            model.provider = "ollama";
            model.model = "qwen2.5-coder:7b";
            session_failover_provider = "ollama";
        }

        // Load agent system prompt
        std::string system_prompt;
        if (agent && !agent->prompt_path.empty()) {
            system_prompt = ProviderExecutor::load_agent_prompt(ctx.euxis_home, agent->prompt_path);
        }

        // Build context from dependency outputs
        std::string context;
        if (step.contains("depends") && step["depends"].is_array()) {
            for (const auto& dep : step["depends"]) {
                std::string dep_name = dep.get<std::string>();
                auto it = step_outputs.find(dep_name);
                if (it != step_outputs.end()) {
                    if (!context.empty()) context += "\n\n---\n\n";
                    context += "## Output from " + dep_name + "\n\n" + it->second;
                }
            }
        }

        auto combined_prompt = ProviderExecutor::build_prompt(system_prompt, step_task, context);

        // Invoke provider
        int frame = 0;
        term::spinner_frame(frame++, tr("Running ") + name + "...");
        auto response = executor.execute(model, combined_prompt);
        term::spinner_clear();

        if (response.success) {
            step_outputs[name] = response.output;

            // Write output to session dir
            auto project_dir = session.ensure_project_dirs(agent_id);
            auto output_path = fs::path(project_dir) / "output"
                               / ("playbook-" + name + ".md");
            write_output_file(output_path, response.output);

            std::cout << "    " << term::icon_ok() << " "
                      << response.output.size() << " " << tr("chars") << ", "
                      << term::format_duration(response.duration_ms) << "\n";
        } else {
            std::cerr << "    " << term::icon_fail() << " " << name << ": "
                      << response.error << "\n";
            
            if (!response.output.empty()) {
                std::cout << term::dim("    (Partial output captured)") << "\n";
            }
            
            // Trigger failover for the rest of the session if a provider crashes
            if (session_failover_provider.empty()) {
                session_failover_provider = "ollama";
                std::cout << term::dim("    \xe2\x9a\xa0  Detected crash. Pivoting to local ollama for remaining steps.\n");
            }
            
            ++failures;
        }
    }

    std::cout << "\n";
    if (failures > 0) {
        std::cout << term::icon_warn() << " " << tr("Playbook finished with ")
                  << failures << tr(" failure(s)") << "\n";
        return 1;
    }
    std::cout << term::icon_ok() << " " << tr("Playbook complete") << "\n";
    return 0;
}

// --- dispatch ---

int cmd_dispatch(Context& ctx, const std::vector<std::string>& args) {
    if (args.empty()) {
        std::cerr << tr("Usage: euxis dispatch <manifest.json>") << "\n";
        return 2;
    }

    auto manifest_path = args[0];
    if (!fs::exists(manifest_path)) {
        std::cerr << tr("Manifest not found: ") << manifest_path << "\n";
        return 1;
    }

    std::ifstream f(manifest_path);
    nlohmann::json manifest;
    try {
        manifest = nlohmann::json::parse(f);
    } catch (const std::exception& e) {
        std::cerr << tr("Invalid JSON: ") << e.what() << "\n";
        return 1;
    }

    std::cout << term::bold(tr("Dispatching Agents")) << "\n";

    if (!manifest.contains("agents") || !manifest["agents"].is_array()) {
        std::cerr << tr("Manifest missing 'agents' array") << "\n";
        return 1;
    }

    RegistryClient registry(ctx.data_dir);
    ProviderRouter router(ctx.data_dir);
    ProviderExecutor executor(ctx.data_dir);
    Session session(ctx.euxis_home);

    const auto& agents_arr = manifest["agents"];
    std::cout << "  " << tr("Agents:") << " " << agents_arr.size() << "\n\n";

    int failures = 0;

    for (size_t i = 0; i < agents_arr.size(); ++i) {
        const auto& entry = agents_arr[i];
        std::string aid = entry.value("agent_id", "");
        std::string priority = entry.value("priority", "P1");
        std::string task = entry.value("task", manifest.value("task", ""));

        auto agent = registry.get_agent(aid);
        auto model = router.route(agent ? agent->tier : "code", task);

        std::cout << "  [" << (i + 1) << "/" << agents_arr.size() << "] "
                  << term::cyan(aid) << " [" << priority << "] -> "
                  << model.model << "\n";

        // Load agent system prompt
        std::string system_prompt;
        if (agent && !agent->prompt_path.empty()) {
            system_prompt = ProviderExecutor::load_agent_prompt(ctx.euxis_home, agent->prompt_path);
        }

        auto combined_prompt = ProviderExecutor::build_prompt(system_prompt, task);

        // Invoke provider
        int frame = 0;
        term::spinner_frame(frame++, tr("Dispatching ") + aid + "...");
        auto response = executor.execute(model, combined_prompt);
        term::spinner_clear();

        if (response.success) {
            // Write output to session dir
            auto project_dir = session.ensure_project_dirs(aid);
            auto output_path = fs::path(project_dir) / "output" / "dispatch-output.md";
            write_output_file(output_path, response.output);

            std::cout << "    " << term::icon_ok() << " "
                      << response.output.size() << " " << tr("chars") << ", "
                      << term::format_duration(response.duration_ms) << "\n";
        } else {
            std::cerr << "    " << term::icon_fail() << " " << aid << ": "
                      << response.error << "\n";
            ++failures;
        }
    }

    std::cout << "\n";
    if (failures > 0) {
        std::cout << term::icon_warn() << " " << tr("Dispatch finished with ")
                  << failures << tr(" failure(s)") << "\n";
        return 1;
    }
    std::cout << term::icon_ok() << " " << tr("Dispatch complete") << "\n";
    return 0;
}

// --- council ---

int cmd_council(Context& ctx, const std::vector<std::string>& args) {
    if (args.size() < 2) {
        std::cerr << tr("Usage: euxis council <topic> <agent1,agent2,...> [--rounds N]") << "\n";
        return 2;
    }

    std::string topic = args[0];
    std::vector<std::string> participants;
    {
        size_t start = 0;
        while (start < args[1].size()) {
            size_t end = args[1].find(',', start);
            if (end == std::string::npos) end = args[1].size();
            participants.push_back(args[1].substr(start, end - start));
            start = end + 1;
        }
    }

    int rounds = 3;
    for (size_t i = 2; i < args.size(); ++i) {
        if (args[i] == "--rounds" && i + 1 < args.size()) {
            rounds = std::stoi(args[++i]);
        }
    }

    std::cout << term::bold(tr("Council Deliberation")) << "\n"
              << "  " << tr("Topic:") << "        " << topic << "\n"
              << "  " << tr("Participants:") << " " << participants.size() << "\n"
              << "  " << tr("Rounds:") << "       " << rounds << "\n\n";

    RegistryClient registry(ctx.data_dir);
    ProviderRouter router(ctx.data_dir);
    ProviderExecutor executor(ctx.data_dir);
    Session session(ctx.euxis_home);

    // Accumulate all responses across rounds for context
    std::vector<std::string> all_responses;
    int failures = 0;

    for (int r = 1; r <= rounds; ++r) {
        std::cout << "  " << tr("Round") << " " << r << "/" << rounds << ":\n";

        for (const auto& pid : participants) {
            auto agent = registry.get_agent(pid);
            auto model = router.route(agent ? agent->tier : "reason", topic);

            // Load agent system prompt
            std::string system_prompt;
            if (agent && !agent->prompt_path.empty()) {
                system_prompt = ProviderExecutor::load_agent_prompt(
                    ctx.euxis_home, agent->prompt_path);
            }

            // Build context from all previous responses
            std::string context;
            for (const auto& prev : all_responses) {
                if (!context.empty()) context += "\n\n---\n\n";
                context += prev;
            }

            auto combined_prompt = ProviderExecutor::build_prompt(system_prompt, topic, context);

            // Invoke provider
            int frame = 0;
            term::spinner_frame(frame++, pid + " " + tr("deliberating..."));
            auto response = executor.execute(model, combined_prompt);
            term::spinner_clear();

            if (response.success) {
                std::string labeled = "## " + pid + " (round " + std::to_string(r) + ")\n\n"
                                      + response.output;
                all_responses.push_back(labeled);

                std::cout << "    " << term::icon_ok() << " " << term::cyan(pid)
                          << " " << response.output.size() << " " << tr("chars") << ", "
                          << response.duration_ms << "ms\n";
            } else {
                std::cerr << "    " << term::icon_fail() << " " << pid << ": "
                          << response.error << "\n";
                ++failures;
            }
        }
    }

    // Write combined council summary
    if (!all_responses.empty()) {
        std::string summary = "# Council Deliberation: " + topic + "\n\n";
        summary += "Participants: " + std::to_string(participants.size())
                   + ", Rounds: " + std::to_string(rounds) + "\n\n";
        for (const auto& resp : all_responses) {
            summary += resp + "\n\n---\n\n";
        }

        // Write to lead participant's session dir
        auto project_dir = session.ensure_project_dirs(participants[0]);
        auto output_path = fs::path(project_dir) / "output" / "council-summary.md";
        write_output_file(output_path, summary);

        std::cout << "\n  " << tr("Summary written to: ") << output_path.string() << "\n";
    }

    std::cout << "\n";
    if (failures > 0) {
        std::cout << term::icon_warn() << " " << tr("Council finished with ")
                  << failures << tr(" failure(s)") << "\n";
        return 1;
    }
    std::cout << term::icon_ok() << " " << tr("Council complete") << "\n";
    return 0;
}

// --- loop ---

int cmd_loop(Context& ctx, const std::vector<std::string>& args) {
    if (args.size() < 2) {
        std::cerr << tr("Usage: euxis loop <agent> <task> [--max-iterations N] [--threshold T]") << "\n";
        return 2;
    }

    std::string agent_id = args[0];
    std::string task = args[1];
    int max_iter = 5;
    double threshold = 0.05;
    for (size_t i = 2; i < args.size(); ++i) {
        if (args[i] == "--max-iterations" && i + 1 < args.size()) {
            max_iter = std::stoi(args[++i]);
        } else if (args[i] == "--threshold" && i + 1 < args.size()) {
            threshold = std::stod(args[++i]);
        }
    }

    RegistryClient registry(ctx.data_dir);
    ProviderRouter router(ctx.data_dir);
    ProviderExecutor executor(ctx.data_dir);
    Session session(ctx.euxis_home);

    auto agent = registry.get_agent(agent_id);
    auto model = router.route(agent ? agent->tier : "code", task);

    std::cout << term::bold(tr("Feedback Loop")) << "\n"
              << "  " << tr("Agent:") << "      " << agent_id << "\n"
              << "  " << tr("Task:") << "       " << task << "\n"
              << "  " << tr("Model:") << "      " << model.model << "\n"
              << "  " << tr("Max iters:") << "  " << max_iter << "\n"
              << "  " << tr("Threshold:") << "  " << threshold << "\n\n";

    // Load agent system prompt
    std::string system_prompt;
    if (agent && !agent->prompt_path.empty()) {
        system_prompt = ProviderExecutor::load_agent_prompt(ctx.euxis_home, agent->prompt_path);
    }

    auto project_dir = session.ensure_project_dirs(agent_id);
    std::string previous_output;
    bool converged = false;

    for (int i = 1; i <= max_iter; ++i) {
        term::progress_bar(i, max_iter, tr("Iteration"));

        // Build prompt with previous output as context for refinement
        auto combined_prompt = ProviderExecutor::build_prompt(
            system_prompt, task, previous_output);

        int frame = 0;
        term::spinner_frame(frame++,
            tr("Iteration ") + std::to_string(i) + "/" + std::to_string(max_iter) + "...");
        auto response = executor.execute(model, combined_prompt);
        term::spinner_clear();

        if (!response.success) {
            std::cerr << "  " << term::icon_fail() << " " << tr("Iteration") << " " << i << ": "
                      << response.error << "\n";
            continue;
        }

        std::cout << "  " << tr("Iteration") << " " << i << ": "
                  << response.output.size() << " " << tr("chars") << ", "
                  << response.duration_ms << "ms\n";

        // Write iteration output
        auto output_path = fs::path(project_dir) / "output"
                           / ("loop-iter-" + std::to_string(i) + ".md");
        write_output_file(output_path, response.output);

        // Check convergence
        if (!previous_output.empty() &&
            has_converged(previous_output, response.output, threshold)) {
            converged = true;
            std::cout << "  " << term::icon_ok() << " " << tr("Converged at iteration ") << i << "\n";

            // Write final converged output
            auto final_path = fs::path(project_dir) / "output" / "loop-final.md";
            write_output_file(final_path, response.output);
            break;
        }

        previous_output = response.output;
    }

    if (!converged) {
        // Write last output as final even without convergence
        if (!previous_output.empty()) {
            auto final_path = fs::path(project_dir) / "output" / "loop-final.md";
            write_output_file(final_path, previous_output);
        }
        std::cout << term::icon_warn() << " " << tr("Loop reached max iterations without convergence") << "\n";
        return 0;
    }

    std::cout << term::icon_ok() << " " << tr("Loop complete") << "\n";
    return 0;
}

// --- synthesize ---

int cmd_synthesize(Context& ctx, const std::vector<std::string>& args) {
    if (args.empty()) {
        std::cerr << tr("Usage: euxis synthesize <output-dir> [--agents a1,a2,...] [--agent SYNTH_AGENT]") << "\n";
        return 2;
    }

    auto output_dir = fs::path(args[0]);
    std::vector<std::string> agent_ids;
    std::string synth_agent_id = "synthesizer";

    for (size_t i = 1; i < args.size(); ++i) {
        if (args[i] == "--agents" && i + 1 < args.size()) {
            size_t start = 0;
            const auto& list = args[++i];
            while (start < list.size()) {
                size_t end = list.find(',', start);
                if (end == std::string::npos) end = list.size();
                agent_ids.push_back(list.substr(start, end - start));
                start = end + 1;
            }
        } else if (args[i] == "--agent" && i + 1 < args.size()) {
            synth_agent_id = args[++i];
        }
    }

    std::cout << term::bold(tr("Synthesize Outputs")) << "\n"
              << "  " << tr("Output:") << "  " << output_dir.string() << "\n"
              << "  " << tr("Agents:") << "  " << agent_ids.size() << "\n"
              << "  " << tr("Synth:") << "   " << synth_agent_id << "\n\n";

    if (!fs::is_directory(output_dir)) {
        std::cerr << tr("Output directory not found: ") << output_dir.string() << "\n";
        return 1;
    }

    // Collect all output files from the directory
    std::vector<std::pair<std::string, std::string>> collected; // (filename, content)
    for (const auto& entry : fs::recursive_directory_iterator(output_dir)) {
        if (!entry.is_regular_file()) continue;

        // If agent filter is specified, only include matching agent dirs
        if (!agent_ids.empty()) {
            bool matches = false;
            auto rel = fs::relative(entry.path(), output_dir).string();
            for (const auto& aid : agent_ids) {
                if (rel.find(aid) != std::string::npos) {
                    matches = true;
                    break;
                }
            }
            if (!matches) continue;
        }

        auto content = read_file_contents(entry.path());
        if (!content.empty()) {
            collected.emplace_back(entry.path().filename().string(), content);
        }
    }

    std::cout << "  " << tr("Files collected:") << " " << collected.size() << "\n";

    if (collected.empty()) {
        std::cout << term::icon_warn() << " " << tr("No output files found to synthesize") << "\n";
        return 0;
    }

    // Build synthesis prompt from all collected outputs
    std::string synthesis_context;
    for (const auto& [filename, content] : collected) {
        synthesis_context += "## " + filename + "\n\n" + content + "\n\n---\n\n";
    }

    RegistryClient registry(ctx.data_dir);
    ProviderRouter router(ctx.data_dir);
    ProviderExecutor executor(ctx.data_dir);
    Session session(ctx.euxis_home);

    auto agent = registry.get_agent(synth_agent_id);
    auto model = router.route(agent ? agent->tier : "reason", "synthesize");

    // Load synthesis agent prompt
    std::string system_prompt;
    if (agent && !agent->prompt_path.empty()) {
        system_prompt = ProviderExecutor::load_agent_prompt(ctx.euxis_home, agent->prompt_path);
    }

    std::string synth_task = tr("Synthesize and merge the following ")
                             + std::to_string(collected.size())
                             + tr(" outputs into a coherent summary.");
    auto combined_prompt = ProviderExecutor::build_prompt(
        system_prompt, synth_task, synthesis_context);

    int frame = 0;
    term::spinner_frame(frame++, tr("Synthesizing..."));
    auto response = executor.execute(model, combined_prompt);
    term::spinner_clear();

    if (response.success) {
        // Write synthesized output
        auto project_dir = session.ensure_project_dirs(synth_agent_id);
        auto synth_path = fs::path(project_dir) / "output" / "synthesis.md";
        write_output_file(synth_path, response.output);

        std::cout << "  " << term::icon_ok() << " " << tr("Synthesized ")
                  << response.output.size() << " " << tr("chars") << ", "
                  << response.duration_ms << "ms\n"
                  << "  " << tr("Written to: ") << synth_path.string() << "\n";
    } else {
        std::cerr << "  " << term::icon_fail() << " " << tr("Synthesis failed: ")
                  << response.error << "\n";
        return 1;
    }

    std::cout << term::icon_ok() << " " << tr("Synthesis complete") << "\n";
    return 0;
}

} // namespace euxis::cli::cmd
