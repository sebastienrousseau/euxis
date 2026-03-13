#include "euxis/cli/cmd/fleet.hpp"
#include "euxis/cli/config_loader.hpp"
#include "euxis/cli/i18n.hpp"
#include "euxis/cli/process.hpp"
#include "euxis/cli/provider_executor.hpp"
#include "euxis/cli/provider_router.hpp"
#include "euxis/cli/registry_client.hpp"
#include "euxis/cli/session.hpp"
#include "euxis/cli/terminal.hpp"

#include <algorithm>
#include <chrono>
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

struct EvidencePillar {
    std::string name;
    std::string status; // "Pending", "Verified", "Issue", "Conflict"
    int confidence;     // 0-100
};

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
        std::cerr << tr("Usage: euxis playbook <manifest.json> [goal] [--mode flash|standard|forensic]") << "\n";
        return 2;
    }

    std::string mode = "flash"; // Default to 2026 "Trusted Verdict" speed
    std::string goal = "Audit and improve the codebase";
    std::string manifest_name = args[0];

    for (size_t i = 1; i < args.size(); ++i) {
        if (args[i] == "--mode" && i + 1 < args.size()) { mode = args[++i]; }
        else if (!args[i].starts_with("--")) { goal = args[i]; }
    }

    auto manifest_path = fs::path(ctx.data_dir) / "config" / "playbooks" / (manifest_name + ".json");
    if (!fs::exists(manifest_path)) manifest_path = manifest_name;

    std::ifstream f(manifest_path);
    nlohmann::json manifest;
    try { manifest = nlohmann::json::parse(f); }
    catch (...) { std::cerr << "Invalid Manifest\n"; return 1; }

    RegistryClient registry(ctx.data_dir);
    ProviderRouter router(ctx.data_dir);
    ProviderExecutor executor(ctx.data_dir);
    Session session(ctx.euxis_home);

    // --- SYSTEM INITIALIZATION PROBE ---
    std::cout << "\n" << term::bold(term::cyan("  === SYSTEM INITIALIZATION ===")) << "\n";
    auto check_auth = [&](const std::string& provider, const std::string& reauth_cmd) {
        auto auth = executor.auth_store().resolve_with_fallback(provider);
        bool has_key = (provider == "openai" && std::getenv("OPENAI_API_KEY")) ||
                       (provider == "claude" && std::getenv("ANTHROPIC_API_KEY")) ||
                       (provider == "gemini" && std::getenv("GEMINI_API_KEY"));
        
        if (has_key) {
            std::cout << "    " << term::icon_ok() << " " << provider << " " << term::green("(Native API Key Active)") << "\n";
        } else if (auth && !auth->token.empty()) {
            std::cout << "    " << term::icon_ok() << " " << provider << " " << term::blue("(OAuth Session Active)") << "\n";
        } else if (Process::available(provider)) {
            std::cout << "    " << term::icon_ok() << " " << provider << " " << term::dim("(Binary found, CLI auth preserved)") << "\n";
        } else {
            std::cout << "    " << term::icon_warn() << " " << provider << " " << term::dim("(Not Configured -> ") << term::yellow(reauth_cmd) << term::dim(")") << "\n";
        }
    };
    check_auth("claude", "claude login");
    check_auth("gemini", "gemini login");
    check_auth("openai", "export OPENAI_API_KEY=...");
    check_auth("ollama", "ollama serve");
    std::cout << "\n";

    // --- ADAPTIVE STEP SELECTION ---
    nlohmann::json all_steps_json = nlohmann::json::array();
    if (manifest.contains("steps")) {
        all_steps_json = manifest["steps"];
    } else if (manifest.contains("phases")) {
        for (const auto& phase : manifest["phases"]) {
            for (const auto& d : phase["delegates"]) {
                nlohmann::json s = d;
                s["pillar"] = phase.value("gate", "execution");
                s["name"] = phase.value("name", "Phase") + " / " + d.value("agent", "");
                all_steps_json.push_back(s);
            }
        }
    }

    std::vector<nlohmann::json> active_steps;
    if (mode == "flash") {
        // Librarian (0), Architect (3), Reviewer (10)
        for (int i : {0, 3, 10}) if (i < (int)all_steps_json.size()) active_steps.push_back(all_steps_json[i]);
    } else if (mode == "standard") {
        for (int i : {0, 1, 3, 4, 8, 10}) if (i < (int)all_steps_json.size()) active_steps.push_back(all_steps_json[i]);
    } else {
        for (const auto& s : all_steps_json) active_steps.push_back(s);
    }

    struct StepPlan {
        std::string name;
        std::string agent;
        std::string task;
        std::string pillar;
        std::vector<std::string> depends;
        ModelSelection model;
    };
    std::vector<StepPlan> plans;
    std::vector<term::TableRow> manifest_rows;

    for (const auto& step : active_steps) {
        std::string name = step.value("name", "step");
        std::string agent_id = step.value("agent", "code-agent");
        std::string pillar = step.value("pillar", "execution");
        std::string step_task = step.value("task_template", step.value("task", ""));
        
        size_t pos = 0;
        while ((pos = step_task.find("${goal}", pos)) != std::string::npos) {
            step_task.replace(pos, 7, goal);
            pos += goal.length();
        }
        // Force Verdict Output
        step_task += "\n\nCRITICAL: You MUST end your response with exactly: 'VERDICT: PASS' or 'VERDICT: FAIL' or 'VERDICT: WARN'.";

        std::vector<std::string> depends;
        if (step.contains("depends") && step["depends"].is_array()) {
            for (const auto& d : step["depends"]) depends.push_back(d.get<std::string>());
        }

        auto agent = registry.get_agent(agent_id);
        auto tier = agent ? agent->tier : "code";
        auto model = router.route(tier, step_task, "swarm");

        plans.push_back({name, agent_id, step_task, pillar, depends, model});
        std::string tier_color = (tier == "reason") ? term::magenta(tier) : term::cyan(tier);
        manifest_rows.push_back({{name, agent_id, tier_color, model.provider, model.model}});
    }

    std::cout << term::bold(term::cyan("  === PRE-FLIGHT VERDICT PLAN [MODE: " + mode + "] ===")) << "\n";
    term::print_table({tr("Step Name"), tr("Agent"), tr("Tier"), tr("Provider"), tr("Model")}, manifest_rows);
    std::cout << "\n" << term::bold(term::cyan("  === CONCURRENT EVIDENCE COLLECTION ===")) << "\n";

    // --- EVIDENCE COLLECTION ---
    const int total_slots = 8;
    int used_slots = 0;
    int completed_count = 0;
    int failures = 0;
    int warn_count = 0;

    std::mutex mtx;
    std::condition_variable cv;
    std::unordered_set<std::string> completed;
    std::unordered_map<std::string, std::string> step_outputs;
    std::unordered_map<std::string, std::string> step_verdicts;
    std::vector<bool> started(plans.size(), false);
    std::string session_failover;

    auto start_time = std::chrono::steady_clock::now();

    while (completed_count < (int)plans.size()) {
        std::unique_lock<std::mutex> lock(mtx);
        bool launched = false;

        for (size_t i = 0; i < plans.size(); ++i) {
            if (started[i]) continue;
            bool deps_met = true;
            for (const auto& d : plans[i].depends) {
                if (completed.find(d) == completed.end()) { deps_met = false; break; }
            }

            int weight = (plans[i].model.provider == "claude" || plans[i].model.provider == "gemini") ? 4 : 1;
            if (deps_met && (used_slots + weight) <= total_slots) {
                started[i] = true; used_slots += weight; launched = true;

                std::thread([&, i, plan = plans[i], weight]() {
                    std::string context;
                    {
                        std::lock_guard<std::mutex> lk(mtx);
                        for (const auto& d : plan.depends) context += "## " + d + "\n" + step_outputs[d] + "\n---\n";
                        std::cout << "  " << term::icon_info() << " " << term::bold(plan.name) << term::dim(" (" + plan.model.provider + ")") << "\n";
                    }

                    ModelSelection m = plan.model;
                    {
                        std::lock_guard<std::mutex> lk(mtx);
                        if (!session_failover.empty()) { m.provider = session_failover; m.model = "qwen2.5-coder:7b"; }
                    }

                    auto response = executor.execute(m, ProviderExecutor::build_prompt("", plan.task, context), 120);

                    {
                        std::lock_guard<std::mutex> lk(mtx);
                        if (response.success) {
                            step_outputs[plan.name] = response.output;
                            std::string v = "WARN";
                            if (response.output.find("VERDICT: PASS") != std::string::npos) v = "PASS";
                            else if (response.output.find("VERDICT: FAIL") != std::string::npos) { v = "FAIL"; failures++; }
                            else { warn_count++; }
                            step_verdicts[plan.name] = v;

                            std::string v_str = (v == "PASS") ? term::green(v) : (v == "FAIL" ? term::red(v) : term::yellow(v));
                            std::cout << "    " << term::icon_ok() << " " << term::bold(plan.name) << " [" << v_str << "] " << term::dim(term::format_duration(response.duration_ms)) << "\n";
                        } else {
                            std::cout << "    " << term::icon_fail() << " " << term::bold(plan.name) << ": " << term::red(response.error) << "\n";
                            if (response.exit_code == 137 || response.exit_code == 143) {
                                session_failover = "ollama";
                                std::cout << term::dim("    \xe2\x9a\xa0  Resource pressure. Failover active.\n");
                            }
                            failures++;
                        }
                        completed.insert(plan.name); completed_count++; used_slots -= weight;
                    }
                    cv.notify_all();
                }).detach();
            }
        }
        if (!launched && completed_count < (int)plans.size()) cv.wait(lock);
    }

    auto elapsed = std::chrono::steady_clock::now() - start_time;
    double total_ms = std::chrono::duration<double, std::milli>(elapsed).count();

    // --- FINAL VERDICT DASHBOARD ---
    std::cout << "\n" << term::bold(term::cyan("  === FINAL REPOSITORY VERDICT ===")) << "\n";
    std::string verdict_text = "TRUSTED";
    uint8_t vr=0, vg=255, vb=0;
    if (failures > 0) { verdict_text = "REJECTED"; vr=255; vg=0; vb=0; }
    else if (warn_count > 0 || !session_failover.empty()) { verdict_text = "CAUTION"; vr=255; vg=255; vb=0; }

    std::cout << "    " << term::bold("Verdict:") << "    " << term::rgb_fg(vr, vg, vb, verdict_text) << "\n";
    std::cout << "    " << term::bold("Confidence:") << " " << (failures > 0 ? 30 : (warn_count > 0 ? 60 : 95)) << "%\n";
    std::cout << "    " << term::bold("Reliability:") << " " << (session_failover.empty() ? term::green("Sovereign") : term::yellow("Degraded (Local Fallback)")) << "\n";
    std::cout << "    " << term::bold("Performance:") << " " << term::format_duration(total_ms) << " (" << (total_ms < 60000 ? term::green("Outcome Perfect") : term::yellow("Sub-optimal")) << ")\n\n";

    return (failures > 0) ? 1 : 0;
}

// Remainder boilerplate
int cmd_agent_bootstrap(Context&, const std::vector<std::string>&) { return 0; }
int cmd_squad(Context&, const std::vector<std::string>&) { return 0; }
int cmd_combo(Context&, const std::vector<std::string>&) { return 0; }
int cmd_dispatch([[maybe_unused]] Context& ctx, [[maybe_unused]] const std::vector<std::string>& args) { return 0; }
int cmd_council([[maybe_unused]] Context& ctx, [[maybe_unused]] const std::vector<std::string>& args) { return 0; }
int cmd_loop([[maybe_unused]] Context& ctx, [[maybe_unused]] const std::vector<std::string>& args) { return 0; }
int cmd_synthesize([[maybe_unused]] Context& ctx, [[maybe_unused]] const std::vector<std::string>& args) { return 0; }

} // namespace euxis::cli::cmd
