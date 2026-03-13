#include "euxis/cli/cmd/fleet.hpp"
#include "euxis/cli/config_loader.hpp"
#include "euxis/cli/i18n.hpp"
#include "euxis/cli/process.hpp"
#include "euxis/cli/provider_executor.hpp"
#include "euxis/cli/provider_router.hpp"
#include "euxis/cli/registry_client.hpp"
#include "euxis/cli/session.hpp"
#include "euxis/cli/terminal.hpp"

#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <unordered_set>

using euxis::cli::i18n::tr;

namespace euxis::cli::cmd {
namespace {

namespace fs = std::filesystem;
namespace term = terminal;

void write_output_file(const fs::path& path, const std::string& content) {
    fs::create_directories(path.parent_path());
    std::ofstream f(path);
    f << content;
}

[[maybe_unused]] auto read_file_contents(const fs::path& path) -> std::string {
    if (!fs::exists(path)) return {};
    std::ifstream f(path);
    return {std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}

[[maybe_unused]] auto has_converged(const std::string& prev, const std::string& curr,
                   double threshold = 0.05) -> bool {
    if (prev.empty() || curr.empty()) return false;
    if (prev == curr) return true;
    auto min_len = std::min(prev.size(), curr.size());
    auto max_len = std::max(prev.size(), curr.size());
    size_t diffs = max_len - min_len;
    for (size_t i = 0; i < min_len; ++i) if (prev[i] != curr[i]) ++diffs;
    return (static_cast<double>(diffs) / static_cast<double>(max_len)) < threshold;
}

} // namespace

// --- agent ---
int cmd_agent(Context& ctx, const std::vector<std::string>& args) {
    RegistryClient registry(ctx.data_dir);
    if (args.empty() || args[0] == "list") {
        auto agents = registry.list_agents();
        std::cout << term::bold(tr("Registered Agents")) << " (" << agents.size() << ")\n\n";
        std::vector<term::TableRow> rows;
        for (const auto& a : agents) rows.push_back({{a.id, a.role, a.tier, a.version}});
        term::print_table({tr("ID"), tr("Role"), tr("Tier"), tr("Version")}, rows);
        return 0;
    }
    return 0;
}

int cmd_agent_bootstrap(Context&, const std::vector<std::string>&) { return 0; }
int cmd_squad(Context&, const std::vector<std::string>&) { return 0; }
int cmd_combo(Context&, const std::vector<std::string>&) { return 0; }

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
        auto std_path = fs::path(ctx.data_dir) / "config" / "playbooks" / (manifest_path_str + ".json");
        if (fs::exists(std_path)) manifest_path = std_path;
        else { std::cerr << tr("Manifest not found: ") << manifest_path_str << "\n"; return 1; }
    }

    std::ifstream f(manifest_path);
    nlohmann::json manifest;
    try { manifest = nlohmann::json::parse(f); }
    catch (const std::exception& e) { std::cerr << tr("Invalid JSON: ") << e.what() << "\n"; return 1; }

    nlohmann::json steps_json = nlohmann::json::array();
    if (manifest.contains("steps") && manifest["steps"].is_array()) steps_json = manifest["steps"];
    else if (manifest.contains("phases") && manifest["phases"].is_array()) {
        for (const auto& phase : manifest["phases"]) {
            std::string phase_name = phase.value("name", "Phase");
            if (phase.contains("delegates") && phase["delegates"].is_array()) {
                for (const auto& d : phase["delegates"]) {
                    nlohmann::json step;
                    step["name"] = phase_name + " / " + d.value("agent", "delegate");
                    step["agent"] = d.value("agent", "");
                    step["task"] = d.value("task_template", "");
                    if (d.contains("depends")) step["depends"] = d["depends"];
                    steps_json.push_back(step);
                }
            }
        }
    }

    if (steps_json.empty()) { std::cerr << tr("Manifest missing 'steps' or 'phases'") << "\n"; return 1; }

    RegistryClient registry(ctx.data_dir);
    ProviderRouter router(ctx.data_dir);
    ProviderExecutor executor(ctx.data_dir);
    Session session(ctx.euxis_home);

    // --- SYSTEM INITIALIZATION PROBE (PRIORITIZED) ---
    std::cout << "\n" << term::bold(term::cyan("  === SYSTEM INITIALIZATION ===")) << "\n";
    auto check_auth = [&](const std::string& provider, const std::string& reauth_cmd) {
        auto auth = executor.auth_store().resolve_with_fallback(provider);
        
        bool has_native_key = false;
        if (provider == "openai") has_native_key = (std::getenv("OPENAI_API_KEY") != nullptr);
        else if (provider == "claude") has_native_key = (std::getenv("ANTHROPIC_API_KEY") != nullptr);
        else if (provider == "gemini") has_native_key = (std::getenv("GEMINI_API_KEY") != nullptr);

        if (has_native_key) {
            std::cout << "    " << term::icon_ok() << " " << provider << " " << term::green("(Native API Key Found)") << "\n";
        } else if (auth && !auth->token.empty()) {
            std::cout << "    " << term::icon_ok() << " " << provider << " " << term::blue("(OAuth Session Active)") << "\n";
        } else if (Process::available(provider)) {
            std::cout << "    " << term::icon_ok() << " " << provider << " " << term::dim("(Binary found, assuming CLI auth)") << "\n";
        } else {
            std::cout << "    " << term::icon_warn() << " " << provider << " " << term::dim("(Not Configured -> Run: `") << term::yellow(reauth_cmd) << term::dim("`)") << "\n";
        }
    };
    check_auth("claude", "claude login");
    check_auth("gemini", "gemini login");
    check_auth("openai", "export OPENAI_API_KEY=...");
    check_auth("ollama", "ollama serve");
    std::cout << "\n";

    // --- PRE-FLIGHT MANIFEST ---
    struct StepPlan {
        std::string name;
        std::string agent;
        std::string task;
        std::vector<std::string> depends;
        ModelSelection model;
    };
    std::vector<StepPlan> plans;
    std::vector<term::TableRow> manifest_rows;

    for (size_t i = 0; i < steps_json.size(); ++i) {
        const auto& step = steps_json[i];
        std::string name = step.value("name", "step-" + std::to_string(i + 1));
        std::string agent_id = step.value("agent", "code-agent");
        std::string step_task = step.value("task", manifest.value("task", ""));
        
        size_t pos = 0;
        while ((pos = step_task.find("${goal}", pos)) != std::string::npos) {
            step_task.replace(pos, 7, goal);
            pos += goal.length();
        }

        std::vector<std::string> depends;
        if (step.contains("depends") && step["depends"].is_array()) {
            for (const auto& d : step["depends"]) depends.push_back(d.get<std::string>());
        }

        auto agent = registry.get_agent(agent_id);
        auto tier = agent ? agent->tier : "code";
        auto model = router.route(tier, step_task, "swarm");

        plans.push_back({name, agent_id, step_task, depends, model});
        
        std::string tier_color = (tier == "reason") ? term::magenta(tier) : term::cyan(tier);
        manifest_rows.push_back({{name, agent_id, tier_color, model.provider, model.model}});
    }

    std::cout << term::bold(term::cyan("  === PRE-FLIGHT MANIFEST ===")) << "\n";
    term::print_table({tr("Step Name"), tr("Agent"), tr("Tier"), tr("Provider"), tr("Model")}, manifest_rows);
    std::cout << "\n" << term::bold(term::cyan("  === CONCURRENT SWARM EXECUTION ===")) << "\n";

    // --- CONCURRENT EXECUTION ENGINE ---
    const int total_resource_slots = 4;
    int used_slots = 0;
    int completed_count = 0;
    int failures = 0;

    auto get_weight = [](const std::string& provider) {
        if (provider == "claude" || provider == "gemini" || provider == "openai") return 4;
        if (provider == "aider" || provider == "opencode") return 2;
        return 1;
    };

    std::mutex mtx;
    std::condition_variable cv;
    std::unordered_set<std::string> completed;
    std::unordered_map<std::string, std::string> step_outputs;
    std::vector<bool> started(plans.size(), false);
    std::string session_failover_provider;

    auto start_time = std::chrono::steady_clock::now();

    while (completed_count < (int)plans.size()) {
        std::unique_lock<std::mutex> lock(mtx);
        bool launched_any = false;

        for (size_t i = 0; i < plans.size(); ++i) {
            if (started[i]) continue;

            bool deps_met = true;
            for (const auto& d : plans[i].depends) {
                if (completed.find(d) == completed.end()) { deps_met = false; break; }
            }

            int weight = get_weight(plans[i].model.provider);
            if (deps_met && (used_slots + weight) <= total_resource_slots) {
                started[i] = true;
                used_slots += weight;
                launched_any = true;

                std::thread([&, i, plan = plans[i], weight]() {
                    std::string context;
                    {
                        std::lock_guard<std::mutex> lk(mtx);
                        for (const auto& d : plan.depends) context += "## Output from " + d + "\n\n" + step_outputs[d] + "\n\n---\n\n";
                        std::cout << "  " << term::icon_info() << " " << tr("Starting ") << term::bold(plan.name) 
                                  << term::dim(" (" + plan.model.provider + ")") << "\n";
                    }

                    ModelSelection active_model = plan.model;
                    {
                        std::lock_guard<std::mutex> lk(mtx);
                        if (!session_failover_provider.empty()) {
                            active_model.provider = session_failover_provider;
                            if (active_model.provider == "ollama") active_model.model = "qwen2.5-coder:7b";
                        }
                    }

                    auto agent_info = registry.get_agent(plan.agent);
                    std::string sys_prompt;
                    if (agent_info && !agent_info->prompt_path.empty()) {
                        sys_prompt = ProviderExecutor::load_agent_prompt(ctx.euxis_home, agent_info->prompt_path);
                    }
                    auto combined_prompt = ProviderExecutor::build_prompt(sys_prompt, plan.task, context);

                    auto response = executor.execute(active_model, combined_prompt);

                    {
                        std::lock_guard<std::mutex> lk(mtx);
                        if (response.success) {
                            step_outputs[plan.name] = response.output;
                            std::cout << "    " << term::icon_ok() << " " << term::bold(plan.name) << " "
                                      << term::dim(std::format("[{} chars, {}]", response.output.size(), term::format_duration(response.duration_ms))) << "\n";
                        } else {
                            std::cerr << "    " << term::icon_fail() << " " << term::bold(plan.name) << ": "
                                      << term::red(response.error) << " (Exit Code: " << response.exit_code << ")\n";
                            failures++;
                            if (response.exit_code == 137 || response.exit_code == 143) {
                                session_failover_provider = "ollama";
                                std::cout << term::dim("    \xe2\x9a\xa0  Systemic failure. Pivoting to local ollama.\n");
                            }
                        }
                        
                        auto project_dir = session.ensure_project_dirs(plan.agent);
                        auto output_path = fs::path(project_dir) / "output" / ("playbook-" + plan.name + ".md");
                        write_output_file(output_path, response.output);

                        completed.insert(plan.name);
                        completed_count++;
                        used_slots -= weight;
                    }
                    cv.notify_all();
                }).detach();
            }
        }

        if (!launched_any && completed_count < (int)plans.size()) cv.wait(lock);
    }

    auto elapsed = std::chrono::steady_clock::now() - start_time;
    double total_s = std::chrono::duration<double>(elapsed).count();

    std::cout << "\n" << term::bold(term::cyan("  === SWARM SUMMARY ===")) << "\n";
    std::cout << "    " << tr("Status:") << "   " << (failures == 0 ? term::green(tr("Success")) : term::red(tr("Partial Failure"))) << "\n";
    std::cout << "    " << tr("Time:") << "     " << term::format_duration(total_s * 1000.0) << "\n";
    std::cout << "    " << tr("Steps:") << "    " << completed_count << "/" << plans.size() << "\n\n";

    return (failures > 0) ? 1 : 0;
}

// --- boilerplate remaining commands ---
int cmd_dispatch([[maybe_unused]] Context& ctx, [[maybe_unused]] const std::vector<std::string>& args) { return 0; }
int cmd_council([[maybe_unused]] Context& ctx, [[maybe_unused]] const std::vector<std::string>& args) { return 0; }
int cmd_loop([[maybe_unused]] Context& ctx, [[maybe_unused]] const std::vector<std::string>& args) { return 0; }
int cmd_synthesize([[maybe_unused]] Context& ctx, [[maybe_unused]] const std::vector<std::string>& args) { return 0; }

} // namespace euxis::cli::cmd
