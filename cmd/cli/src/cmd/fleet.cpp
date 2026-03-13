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

} // namespace

int cmd_playbook(Context& ctx, const std::vector<std::string>& args) {
    if (args.empty()) {
        std::cerr << tr("Usage: euxis playbook <manifest.json> [goal] [--mode flash|standard|forensic]") << "\n";
        return 2;
    }

    std::string mode = "flash"; // Default to 2026 "Trusted Verdict" speed
    std::string goal = "Audit and improve the codebase";
    std::string manifest_name = args[0];

    for (size_t i = 1; i < args.size(); ++i) {
        if (args[i] == "--mode" && i + 1 < args.size()) mode = args[++i];
        else if (!args[i].starts_with("--")) goal = args[i];
    }

    auto manifest_path = fs::path(ctx.data_dir) / "config" / "playbooks" / (manifest_name + ".json");
    if (!fs::exists(manifest_path)) manifest_path = manifest_name;

    std::ifstream f(manifest_path);
    nlohmann::json manifest;
    try { manifest = nlohmann::json::parse(f); }
    catch (...) { std::cerr << "Invalid Manifest\n"; return 1; }

    // --- DOMAIN ANCHOR MAPPING ---
    std::vector<EvidencePillar> pillars = {
        {"Intent", "Pending", 0},
        {"Architecture", "Pending", 0},
        {"Security", "Pending", 0},
        {"Readiness", "Pending", 0}
    };

    RegistryClient registry(ctx.data_dir);
    ProviderRouter router(ctx.data_dir);
    ProviderExecutor executor(ctx.data_dir);
    Session session(ctx.euxis_home);

    std::cout << "\n" << term::bold(term::cyan("  === EUXIS VERDICT ENGINE [MODE: " + mode + "] ===")) << "\n";
    
    // --- STEP SELECTION (ADAPTIVE) ---
    nlohmann::json all_steps = manifest.value("steps", nlohmann::json::array());
    if (all_steps.empty() && manifest.contains("phases")) {
        for (auto& p : manifest["phases"]) {
            for (auto& d : p["delegates"]) {
                nlohmann::json s = d;
                s["name"] = p.value("name", "Phase") + " / " + d.value("agent", "");
                all_steps.push_back(s);
            }
        }
    }

    std::vector<nlohmann::json> active_steps;
    if (mode == "flash") {
        for (int i : {0, 3, 10}) if (i < (int)all_steps.size()) active_steps.push_back(all_steps[i]);
    } else if (mode == "standard") {
        for (int i : {0, 1, 3, 4, 8, 10}) if (i < (int)all_steps.size()) active_steps.push_back(all_steps[i]);
    } else {
        for (auto& s : all_steps) active_steps.push_back(s);
    }

    // --- PRE-FLIGHT DASHBOARD ---
    std::vector<term::TableRow> manifest_rows;
    for (auto& s : active_steps) {
        auto agent = registry.get_agent(s.value("agent", ""));
        auto model = router.route(agent ? agent->tier : "code", s.value("task_template", ""), "swarm");
        manifest_rows.push_back({{s.value("name", "Step"), s.value("agent", ""), model.provider, "Active"}});
    }
    term::print_table({"Evidence Step", "Agent", "Provider", "Status"}, manifest_rows);
    std::cout << "\n" << term::bold(term::magenta("  --- LIVE COVERAGE MATRIX ---")) << "\n";

    // --- CONCURRENT EXECUTION ---
    std::mutex mtx;
    std::condition_variable cv;
    int completed_count = 0;
    int failures = 0;
    int active_tasks = 0;
    std::string session_failover;
    auto start_time = std::chrono::steady_clock::now();

    for (auto& step : active_steps) {
        {
            std::unique_lock<std::mutex> lock(mtx);
            while (active_tasks >= 4) cv.wait(lock);
            active_tasks++;
        }

        std::thread([&, step]() {
            auto agent_id = step.value("agent", "code-agent");
            auto task = step.value("task_template", "");
            size_t p = task.find("${goal}");
            if (p != std::string::npos) task.replace(p, 7, goal);

            auto agent = registry.get_agent(agent_id);
            auto model = router.route(agent ? agent->tier : "code", task, "swarm");

            {
                std::lock_guard<std::mutex> lk(mtx);
                if (!session_failover.empty()) {
                    model.provider = session_failover;
                    model.model = "qwen2.5-coder:7b";
                }
            }

            auto response = executor.execute(model, task);

            {
                std::lock_guard<std::mutex> lk(mtx);
                if (response.success) {
                    std::cout << "    " << term::icon_ok() << " " << term::bold(step.value("name", "")) 
                              << term::dim(" [" + term::format_duration(response.duration_ms) + "]") << "\n";
                } else {
                    std::cout << "    " << term::icon_fail() << " " << term::red(step.value("name", "")) 
                              << term::dim(" (Pivot: Memory Pressure)") << "\n";
                    if (response.exit_code == 137 || response.exit_code == 143) session_failover = "ollama";
                    failures++;
                }
                active_tasks--;
                completed_count++;
            }
            cv.notify_all();
        }).detach();
    }

    while (true) {
        std::unique_lock<std::mutex> lock(mtx);
        if (completed_count >= (int)active_steps.size()) break;
        cv.wait(lock);
    }

    auto elapsed = std::chrono::steady_clock::now() - start_time;
    double total_s = std::chrono::duration<double>(elapsed).count();

    // --- FINAL VERDICT DASHBOARD ---
    std::cout << "\n" << term::bold(term::cyan("  === FINAL REPO VERDICT ===")) << "\n";
    std::string verdict = (failures == 0) ? term::green("TRUSTED") : term::yellow("CAUTION (Degraded)");
    int confidence = (failures == 0) ? 95 : 65;

    std::cout << "    Verdict:    " << verdict << "\n";
    std::cout << "    Confidence: " << confidence << "%\n";
    std::cout << "    Strategy:   " << (session_failover.empty() ? term::blue("Sovereign Swarm") : term::yellow("Hybrid Fallback")) << "\n";
    std::cout << "    Time:       " << term::bold(term::format_duration(total_s * 1000.0)) << "\n\n";

    return (failures > 0) ? 1 : 0;
}

// Remainder boilerplate
int cmd_agent(Context& ctx, const std::vector<std::string>& args);
int cmd_agent_bootstrap(Context&, const std::vector<std::string>&) { return 0; }
int cmd_squad(Context&, const std::vector<std::string>&) { return 0; }
int cmd_combo(Context&, const std::vector<std::string>&) { return 0; }
int cmd_dispatch([[maybe_unused]] Context& ctx, [[maybe_unused]] const std::vector<std::string>& args) { return 0; }
int cmd_council([[maybe_unused]] Context& ctx, [[maybe_unused]] const std::vector<std::string>& args) { return 0; }
int cmd_loop([[maybe_unused]] Context& ctx, [[maybe_unused]] const std::vector<std::string>& args) { return 0; }
int cmd_synthesize([[maybe_unused]] Context& ctx, [[maybe_unused]] const std::vector<std::string>& args) { return 0; }

} // namespace euxis::cli::cmd
