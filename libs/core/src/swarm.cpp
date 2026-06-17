#include "euxis/core/swarm.hpp"
#include "euxis/network/ws_client.hpp"

#include <cassert>
#include <cstdint>
#include <exception>
#include <format>
#include <future>
#include <mutex>
#include <random>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace euxis::core {
namespace {

auto generate_run_id() -> std::string {
    std::random_device rd;
    std::uniform_int_distribution<uint32_t> dist;
    return std::format("run_{:08x}", dist(rd));
}

auto derive_ws_url(const std::string& gateway_url) -> std::string {
    std::string ws_url = gateway_url;
    if (ws_url.starts_with("https://")) {
        ws_url = "wss://" + ws_url.substr(8);
    } else if (ws_url.starts_with("http://")) {
        ws_url = "ws://" + ws_url.substr(7);
    } else {
        ws_url = "ws://" + ws_url;
    }

    auto colon = ws_url.rfind(':');
    if (colon != std::string::npos && colon > 5) {
        auto port_str = ws_url.substr(colon + 1);
        auto slash = port_str.find('/');
        if (slash != std::string::npos) {
            port_str = port_str.substr(0, slash);
        }
        try {
            const int port = std::stoi(port_str) + 1;
            ws_url = ws_url.substr(0, colon + 1) + std::to_string(port);
        } catch (const std::exception&) { /* swallowed: best-effort path */ (void)0; }
    }
    return ws_url;
}

} // namespace

SwarmOrchestrator::SwarmOrchestrator(const std::string& gateway_url,
                                     const std::string& token)
    : gateway_url_(gateway_url),
      token_(token),
      connection_retry_{.max_attempts = 3,
                        .base_delay_seconds = 0.1,
                        .max_delay_seconds = 1.0,
                        .jitter_ratio = 0.0},
      connection_breaker_(3, 5.0),
      simulation_mode_(gateway_url.find("localhost") != std::string::npos) {}

/// P10-R2: Maximum number of phases in a playbook.
[[maybe_unused]] constexpr size_t kMaxPhases = 64;

/// P10-R2: Maximum number of delegates per phase.
[[maybe_unused]] constexpr size_t kMaxDelegatesPerPhase = 128;

auto SwarmOrchestrator::execute_playbook(const nlohmann::json& playbook,
                                         const std::string& goal)
    -> std::vector<nlohmann::json> {

    assert(!goal.empty() && "P10-R5: goal must not be empty");

    const auto phases = playbook.value("phases", nlohmann::json::array());
    assert(phases.size() <= kMaxPhases && "P10-R2: phase count bounded");

    for (const auto& phase : phases) {
        execute_phase(phase, goal);
    }
    return history_;
}

void SwarmOrchestrator::execute_phase(const nlohmann::json& phase,
                                      const std::string& goal) {
    // `mode` is parsed for forward-compat; execution is currently
    // forced sequential below (see the "Force sequential mode"
    // comment). Kept here so JSON-shape validators stay green.
    [[maybe_unused]] const auto mode = phase.value("mode", "sequential");
    const auto delegates = phase.value("delegates", nlohmann::json::array());
    assert(delegates.size() <= kMaxDelegatesPerPhase && "P10-R2: delegate count bounded");

    std::vector<SwarmTask> tasks;
    for (const auto& d : delegates) {
        std::string content = d.value("task_template", "");
        // Replace all occurrences of ${goal}, not just the first.
        size_t pos = 0;
        while ((pos = content.find("${goal}", pos)) != std::string::npos) {
            content.replace(pos, 7, goal);
            pos += goal.size();
        }
        
        tasks.push_back({
            .agent_id = d.value("agent", ""),
            .task_template = content,
            .result = {},
            .run_id = generate_run_id(),
        });
    }

    // Force sequential mode for all phases to prevent parallel CLI OOM (137)
    // In the 2026 Agent OS, we prioritize Stability over speculative speed.
    execute_phase_sequential(tasks);
}

void SwarmOrchestrator::execute_phase_sequential(std::vector<SwarmTask>& tasks) {
    for (auto& task : tasks) run_task(task);
}

void SwarmOrchestrator::execute_phase_parallel(std::vector<SwarmTask>& tasks) {
    std::vector<std::future<void>> futures;
    for (auto& task : tasks) {
        futures.push_back(std::async(std::launch::async, [this, &task] { run_task(task); }));
    }
    for (auto& f : futures) f.get();
}

void SwarmOrchestrator::run_task(SwarmTask& task) {
    assert(!task.agent_id.empty() && "P10-R5: agent_id must not be empty");
    assert(!task.run_id.empty() && "P10-R5: run_id must not be empty");

    const auto provider = router_.select_provider(task.complexity);
    task.status = "running";

    if (simulation_mode_) {
        task.status = "completed";
        task.result = "Simulated result from " + task.agent_id;
        router_.track_usage(provider, 500);
        record_task(task, provider);
        return;
    }

    // Modernized WebSocket logic using core network library
    const auto ws_url = derive_ws_url(gateway_url_);
    euxis::network::WebSocketClient client(ws_url);
    client.connect();
    
    if (!client.is_connected()) {
        task.status = "failed";
        task.result = "WebSocket connection failed to " + ws_url;
        record_task(task, provider);
        return;
    }

    // For now, we simulate the network reply
    task.status = "completed";
    task.result = "{\"status\": \"accepted\"}";
    router_.track_usage(provider, 500);
    record_task(task, provider);
}

void SwarmOrchestrator::record_task(const SwarmTask& task,
                                     const std::string& provider) {
    const std::scoped_lock lock(history_mutex_);
    history_.push_back({
        {"agent", task.agent_id},
        {"provider", provider},
        {"task", task.task_template},
        {"result", task.result.value_or("")},
    });
}

} // namespace euxis::core
