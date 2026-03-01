#include "euxis/core/swarm.hpp"
#include "euxis/core/ws_client.hpp"

#include <future>
#include <random>
#include <spdlog/spdlog.h>

namespace euxis::core {
namespace {

auto generate_run_id() -> std::string {
    static std::mt19937 gen(std::random_device{}());
    static std::uniform_int_distribution<uint32_t> dist;
    char buf[16];
    std::snprintf(buf, sizeof(buf), "run_%08x", dist(gen));
    return buf;
}

/// Derive the WebSocket URL from an HTTP gateway URL.
/// e.g. "http://example.com:8080" -> "ws://example.com:8081"
auto derive_ws_url(const std::string& gateway_url) -> std::string {
    // Replace http:// with ws://
    std::string ws_url = gateway_url;
    if (ws_url.starts_with("https://")) {
        ws_url = "wss://" + ws_url.substr(8);
    } else if (ws_url.starts_with("http://")) {
        ws_url = "ws://" + ws_url.substr(7);
    } else {
        ws_url = "ws://" + ws_url;
    }

    // Bump port by 1 (HTTP port -> WS port)
    auto colon = ws_url.rfind(':');
    if (colon != std::string::npos && colon > 5) {
        auto port_str = ws_url.substr(colon + 1);
        // Strip trailing path
        auto slash = port_str.find('/');
        if (slash != std::string::npos) {
            port_str = port_str.substr(0, slash);
        }
        try {
            int port = std::stoi(port_str) + 1;
            ws_url = ws_url.substr(0, colon + 1) + std::to_string(port);
        } catch (...) {
            // Leave as-is
        }
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

auto SwarmOrchestrator::execute_playbook(const nlohmann::json& playbook,
                                         const std::string& goal)
    -> std::vector<nlohmann::json> {
    spdlog::info("Starting swarm playbook: {}",
                 playbook.value("name", "unnamed"));

    for (const auto& phase : playbook.value("phases", nlohmann::json::array())) {
        execute_phase(phase, goal);
    }

    spdlog::info("Swarm playbook execution complete.");
    return history_;
}

void SwarmOrchestrator::execute_phase(const nlohmann::json& phase,
                                      const std::string& goal) {
    auto name = phase.value("name", "Unknown Phase");
    auto mode = phase.value("mode", "sequential");
    spdlog::info("Executing phase: {} (mode: {})", name, mode);

    std::vector<SwarmTask> tasks;
    for (const auto& d : phase.value("delegates", nlohmann::json::array())) {
        std::string content = d.value("task_template", "");
        auto pos = content.find("${goal}");
        if (pos != std::string::npos) {
            content.replace(pos, 7, goal);
        }
        tasks.push_back({
            .agent_id = d.value("agent", ""),
            .task_template = content,
            .result = {},
            .run_id = generate_run_id(),
        });
    }

    if (mode == "parallel") {
        execute_phase_parallel(tasks);
    } else {
        execute_phase_sequential(tasks);
    }
}

void SwarmOrchestrator::execute_phase_sequential(
    std::vector<SwarmTask>& tasks) {
    for (auto& task : tasks) {
        run_task(task);
    }
}

void SwarmOrchestrator::execute_phase_parallel(
    std::vector<SwarmTask>& tasks) {
    std::vector<std::future<void>> futures;
    futures.reserve(tasks.size());

    for (auto& task : tasks) {
        futures.push_back(std::async(std::launch::async, [this, &task] {
            run_task(task);
        }));
    }

    for (auto& f : futures) {
        f.get();
    }
}

void SwarmOrchestrator::run_task(SwarmTask& task) {
    auto provider = router_.select_provider(task.complexity);
    spdlog::info("Dispatching task to agent {} via {}...",
                 task.agent_id, provider);
    task.status = "running";

    if (simulation_mode_) {
        task.status = "completed";
        task.result = "Simulated result from " + task.agent_id;
        router_.track_usage(provider, 500);
        record_task(task, provider);
        return;
    }

    // Non-simulation: dispatch via WebSocket to gateway
    auto ws_url = derive_ws_url(gateway_url_);
    WebSocketClient client(ws_url);
    client.connect();

    if (!client.is_connected()) {
        task.status = "failed";
        task.result = "WebSocket connection failed to " + ws_url;
        record_task(task, provider);
        return;
    }

    nlohmann::json dispatch_msg = {
        {"type", "dispatch"},
        {"agent", task.agent_id},
        {"task", task.task_template},
        {"run_id", task.run_id},
        {"provider", provider},
    };

    auto response = client.send_and_wait(dispatch_msg);
    if (response && response->value("status", "") == "accepted") {
        task.status = "completed";
        task.result = response->dump();
        router_.track_usage(provider, 500);
    } else {
        task.status = "failed";
        task.result = response ? response->dump() : "No response from gateway";
    }
    record_task(task, provider);
}

void SwarmOrchestrator::record_task(const SwarmTask& task,
                                     const std::string& provider) {
    std::lock_guard lock(history_mutex_);
    history_.push_back({
        {"agent", task.agent_id},
        {"provider", provider},
        {"task", task.task_template},
        {"result", *task.result},
    });
}

} // namespace euxis::core
