#include "euxis/cli/cmd/infra.hpp"
#include "euxis/cli/config_loader.hpp"
#include "euxis/cli/process.hpp"
#include "euxis/cli/provider_executor.hpp"
#include "euxis/cli/provider_router.hpp"
#include "euxis/cli/registry_client.hpp"
#include "euxis/cli/terminal.hpp"

#include "euxis/gateway/server.hpp"

#include <atomic>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>

namespace euxis::cli::cmd {
namespace {

namespace fs = std::filesystem;
namespace term = terminal;

// Daemon shutdown flag — set by SIGTERM/SIGINT handler in the child process.
std::atomic<bool> g_daemon_stop{false};

void daemon_signal_handler(int /*sig*/) {
    g_daemon_stop.store(true, std::memory_order_relaxed);
}

// Periodic daemon tasks: health checks, stale bus cleanup, context refresh.
void daemon_main_loop(const std::string& euxis_home, const std::string& data_dir) {
    using clock = std::chrono::steady_clock;
    constexpr auto health_interval  = std::chrono::minutes(5);
    constexpr auto cleanup_interval = std::chrono::hours(1);
    constexpr auto tick_interval    = std::chrono::seconds(10);

    auto last_health  = clock::now();
    auto last_cleanup = clock::now();

    auto log_dir = fs::path(euxis_home) / "euxis-runtime" / "logs";
    fs::create_directories(log_dir);

    auto write_log = [&](const std::string& msg) {
        std::ofstream log(log_dir / "daemon.log", std::ios::app);
        auto now = std::chrono::system_clock::now();
        auto tt  = std::chrono::system_clock::to_time_t(now);
        log << std::put_time(std::localtime(&tt), "%Y-%m-%d %H:%M:%S")
            << " " << msg << "\n";
    };

    write_log("daemon started");

    while (!g_daemon_stop.load(std::memory_order_relaxed)) {
        auto now = clock::now();

        // --- Periodic health check ---
        if (now - last_health >= health_interval) {
            last_health = now;

            // Check critical directories exist
            bool data_ok = fs::is_directory(data_dir);
            bool agents_ok = fs::is_directory(fs::path(data_dir) / "agents");

            // Check disk space
            std::error_code ec;
            auto space = fs::space(euxis_home, ec);
            bool disk_ok = !ec && space.available > 100ULL * 1024 * 1024; // >100MB

            if (!data_ok || !agents_ok) {
                write_log("WARN: missing data directories");
            }
            if (!disk_ok) {
                write_log("WARN: low disk space (<100MB available)");
            }

            write_log("health check: data=" + std::string(data_ok ? "ok" : "FAIL")
                       + " agents=" + std::string(agents_ok ? "ok" : "FAIL")
                       + " disk=" + std::string(disk_ok ? "ok" : "LOW"));
        }

        // --- Periodic stale bus pipe cleanup ---
        if (now - last_cleanup >= cleanup_interval) {
            last_cleanup = now;

            auto bus_dir = fs::path(euxis_home) / "euxis-runtime" / "data" / "bus" / "pipes";
            if (fs::is_directory(bus_dir)) {
                int removed = 0;
                auto fs_now = fs::file_time_type::clock::now();
                for (const auto& entry : fs::directory_iterator(bus_dir)) {
                    if (!entry.is_regular_file()) continue;
                    auto age = fs_now - entry.last_write_time();
                    auto days = std::chrono::duration_cast<std::chrono::hours>(age).count() / 24;
                    if (days > 7) {
                        fs::remove(entry.path());
                        ++removed;
                    }
                }
                if (removed > 0) {
                    write_log("cleanup: removed " + std::to_string(removed) + " stale bus pipe(s)");
                }
            }

            // Clean stale context files
            auto ctx_dir = fs::path(euxis_home) / "euxis-runtime" / "context";
            if (fs::is_directory(ctx_dir)) {
                int ctx_removed = 0;
                auto fs_now = fs::file_time_type::clock::now();
                for (const auto& entry : fs::directory_iterator(ctx_dir)) {
                    if (!entry.is_regular_file()) continue;
                    auto age = fs_now - entry.last_write_time();
                    auto days = std::chrono::duration_cast<std::chrono::hours>(age).count() / 24;
                    if (days > 30) {
                        fs::remove(entry.path());
                        ++ctx_removed;
                    }
                }
                if (ctx_removed > 0) {
                    write_log("cleanup: removed " + std::to_string(ctx_removed) + " stale context file(s)");
                }
            }
        }

        // Sleep in short ticks so we respond promptly to shutdown signal
        std::this_thread::sleep_for(tick_interval);
    }

    write_log("daemon stopped (signal received)");
}

} // namespace

// --- gateway ---

int cmd_gateway(Context& /*ctx*/, const std::vector<std::string>& args) {
    euxis::gateway::GatewayConfig config;

    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "--port" && i + 1 < args.size()) {
            config.port = std::stoi(args[++i]);
        } else if (args[i] == "--ws-port" && i + 1 < args.size()) {
            config.ws_port = std::stoi(args[++i]);
        } else if (args[i] == "--host" && i + 1 < args.size()) {
            config.host = args[++i];
        } else if (args[i] == "--config" && i + 1 < args.size()) {
            config = euxis::gateway::GatewayConfig::load_from_file(args[++i]);
        }
    }

    euxis::gateway::GatewayServer server(config);

    static euxis::gateway::GatewayServer* server_ptr = &server;
    std::signal(SIGINT, [](int) { server_ptr->stop(); });
    std::signal(SIGTERM, [](int) { server_ptr->stop(); });

    std::cout << "Starting gateway on " << config.host << ":" << config.port << "\n";
    server.start();
    return 0;
}

// --- bus ---

int cmd_bus(Context& ctx, const std::vector<std::string>& args) {
    auto bus_dir = fs::path(ctx.euxis_home) / "euxis-runtime" / "data" / "bus" / "pipes";

    if (args.empty() || args[0] == "status") {
        std::cout << term::bold("Message Bus Status") << "\n\n";

        if (!fs::is_directory(bus_dir)) {
            std::cout << "  Bus directory not found\n";
            return 0;
        }

        int pipe_count = 0;
        int stale_count = 0;
        auto now = fs::file_time_type::clock::now();

        for (const auto& entry : fs::directory_iterator(bus_dir)) {
            if (entry.is_regular_file()) {
                ++pipe_count;
                auto age = now - entry.last_write_time();
                auto days = std::chrono::duration_cast<std::chrono::hours>(age).count() / 24;
                std::string status = days > 7 ? "stale" : "active";
                if (days > 7) ++stale_count;

                if (ctx.verbose) {
                    std::cout << "  " << entry.path().filename().string()
                              << " (" << status << ", " << days << "d)\n";
                }
            }
        }

        std::cout << "  Pipes: " << pipe_count << "\n"
                  << "  Stale: " << stale_count << "\n";
        return 0;
    }

    if (args[0] == "publish" && args.size() >= 3) {
        auto pipe = args[1];
        auto message = args[2];
        fs::create_directories(bus_dir);
        auto pipe_path = bus_dir / pipe;
        std::ofstream f(pipe_path, std::ios::app);
        f << message << "\n";
        std::cout << term::icon_ok() << " Published to: " << pipe << "\n";
        return 0;
    }

    if (args[0] == "subscribe" && args.size() >= 2) {
        auto pipe_path = bus_dir / args[1];
        if (!fs::exists(pipe_path)) {
            std::cerr << "Pipe not found: " << args[1] << "\n";
            return 1;
        }
        std::ifstream f(pipe_path);
        std::string line;
        while (std::getline(f, line)) {
            std::cout << line << "\n";
        }
        return 0;
    }

    if (args[0] == "clean") {
        if (!fs::is_directory(bus_dir)) return 0;
        int removed = 0;
        auto now = fs::file_time_type::clock::now();
        for (const auto& entry : fs::directory_iterator(bus_dir)) {
            auto age = now - entry.last_write_time();
            auto days = std::chrono::duration_cast<std::chrono::hours>(age).count() / 24;
            if (days > 7) {
                fs::remove(entry.path());
                ++removed;
            }
        }
        std::cout << term::icon_ok() << " Removed " << removed << " stale pipe(s)\n";
        return 0;
    }

    std::cerr << "Usage: euxis bus <status|publish|subscribe|clean> [args]\n";
    return 2;
}

// --- daemon ---

int cmd_daemon(Context& ctx, const std::vector<std::string>& args) {
    auto pid_file = fs::path(ctx.euxis_home) / "euxis-runtime" / "daemon.pid";

    if (args.empty() || args[0] == "status") {
        if (fs::exists(pid_file)) {
            std::ifstream f(pid_file);
            std::string pid_str;
            std::getline(f, pid_str);
            // Check if process is alive
            auto result = Process::run("kill", {"-0", pid_str});
            if (result.exit_code == 0) {
                std::cout << term::icon_ok() << " Daemon running (PID " << pid_str << ")\n";
                return 0;
            }
            std::cout << term::icon_warn() << " Stale PID file (PID " << pid_str << ")\n";
            return 1;
        }
        std::cout << term::icon_info() << " Daemon not running\n";
        return 0;
    }

    if (args[0] == "start") {
        // Check if already running
        if (fs::exists(pid_file)) {
            std::ifstream pf(pid_file);
            std::string existing_pid;
            std::getline(pf, existing_pid);
            auto chk = Process::run("kill", {"-0", existing_pid});
            if (chk.exit_code == 0) {
                std::cerr << term::icon_warn() << " Daemon already running (PID "
                          << existing_pid << ")\n";
                return 1;
            }
            // Stale PID file, remove it
            fs::remove(pid_file);
        }

        fs::create_directories(pid_file.parent_path());

        pid_t child = fork();
        if (child < 0) {
            std::cerr << term::icon_warn() << " fork() failed: "
                      << std::strerror(errno) << "\n";
            return 1;
        }

        if (child == 0) {
            // --- Child process: become a daemon ---
            if (setsid() < 0) {
                _exit(1);
            }

            // Close standard file descriptors
            close(STDIN_FILENO);
            close(STDOUT_FILENO);
            close(STDERR_FILENO);

            // Write our actual PID to the pid file
            std::ofstream f(pid_file);
            f << getpid();
            f.close();

            // Install signal handlers for graceful shutdown
            std::signal(SIGTERM, daemon_signal_handler);
            std::signal(SIGINT, daemon_signal_handler);

            // Run the daemon main loop (health checks, cleanup, etc.)
            daemon_main_loop(ctx.euxis_home, ctx.data_dir);
            _exit(0);
        }

        // --- Parent process ---
        std::cout << term::icon_ok() << " Daemon started (PID " << child << ")\n";
        return 0;
    }

    if (args[0] == "stop") {
        if (fs::exists(pid_file)) {
            std::ifstream f(pid_file);
            std::string pid_str;
            std::getline(f, pid_str);
            Process::run("kill", {pid_str});
            fs::remove(pid_file);
            std::cout << term::icon_ok() << " Daemon stopped\n";
            return 0;
        }
        std::cout << "Daemon not running\n";
        return 0;
    }

    std::cerr << "Usage: euxis daemon <status|start|stop>\n";
    return 2;
}

// --- deploy ---

int cmd_deploy(Context& ctx, const std::vector<std::string>& args) {
    if (args.empty()) {
        std::cerr << "Usage: euxis deploy <config.json> [--dry-run]\n";
        return 2;
    }

    bool dry_run = false;
    for (const auto& a : args) {
        if (a == "--dry-run") dry_run = true;
    }

    auto config_path = args[0];
    if (!fs::exists(config_path)) {
        std::cerr << "Config not found: " << config_path << "\n";
        return 1;
    }

    std::ifstream f(config_path);
    nlohmann::json config;
    try {
        config = nlohmann::json::parse(f);
    } catch (const std::exception& e) {
        std::cerr << "Invalid JSON: " << e.what() << "\n";
        return 1;
    }

    std::cout << term::bold("Deploy Configuration") << "\n";
    if (dry_run) std::cout << "  " << term::yellow("(dry run)") << "\n";

    int agent_count = 0;
    int agent_errors = 0;

    // --- Apply agents ---
    if (config.contains("agents") && config["agents"].is_array()) {
        RegistryClient registry(ctx.data_dir);

        for (const auto& agent : config["agents"]) {
            std::string id = agent.value("agent_id", "");
            std::string manifest_path = agent.value("manifest_path", "");

            if (id.empty() || manifest_path.empty()) {
                std::cout << "  " << term::icon_warn()
                          << " Skipping agent (missing agent_id or manifest_path)\n";
                continue;
            }

            std::cout << "  Agent: " << id;

            if (dry_run) {
                std::cout << " (would register from " << manifest_path << ")\n";
            } else {
                // Load manifest JSON from the manifest path
                nlohmann::json manifest;
                if (fs::exists(manifest_path)) {
                    std::ifstream mf(manifest_path);
                    try {
                        manifest = nlohmann::json::parse(mf);
                    } catch (const std::exception& e) {
                        std::cerr << "\n    " << term::icon_warn()
                                  << " Bad manifest JSON: " << e.what() << "\n";
                        ++agent_errors;
                        continue;
                    }
                } else {
                    // Use the agent entry itself as the manifest
                    manifest = agent;
                }

                if (registry.register_plugin(id, manifest)) {
                    std::cout << " " << term::icon_ok() << " registered\n";
                    ++agent_count;
                } else {
                    std::cout << " " << term::icon_warn() << " failed\n";
                    ++agent_errors;
                }
            }
        }
    }

    // --- Apply router config ---
    if (config.contains("router") && config["router"].is_object()) {
        auto router_dir = fs::path(ctx.data_dir) / "config";
        auto router_path = router_dir / "router.json";

        std::cout << "  Router config: " << router_path.string();

        if (dry_run) {
            std::cout << " (would write)\n";
        } else {
            fs::create_directories(router_dir);
            std::ofstream rf(router_path);
            rf << config["router"].dump(2) << "\n";
            rf.close();
            std::cout << " " << term::icon_ok() << " written\n";
        }
    }

    // --- Apply squads config ---
    if (config.contains("squads") && config["squads"].is_array()) {
        auto squads_path = fs::path(ctx.data_dir) / "squads.json";

        std::cout << "  Squads config: " << squads_path.string();

        if (dry_run) {
            std::cout << " (would write)\n";
        } else {
            fs::create_directories(fs::path(ctx.data_dir));
            std::ofstream sf(squads_path);
            sf << config["squads"].dump(2) << "\n";
            sf.close();
            std::cout << " " << term::icon_ok() << " written\n";
        }
    }

    // --- Summary ---
    if (dry_run) {
        std::cout << "\n" << term::icon_info() << " Dry run — no changes made\n";
    } else {
        std::cout << "\n" << term::icon_ok() << " Deployment complete ("
                  << agent_count << " agent(s) registered";
        if (agent_errors > 0) {
            std::cout << ", " << agent_errors << " error(s)";
        }
        std::cout << ")\n";
    }
    return agent_errors > 0 ? 1 : 0;
}

// --- optimize ---

int cmd_optimize(Context& ctx, const std::vector<std::string>& args) {
    (void)args;

    std::cout << term::bold("Runtime Optimization") << "\n\n";

    ProviderRouter router(ctx.data_dir);

    // Report current configuration
    router.print_status();
    std::cout << "\n";

    // Check for optimization opportunities
    std::cout << term::bold("Optimizations:") << "\n";

    // 1. Local model fallback
    if (router.local_available()) {
        std::cout << "  " << term::icon_ok() << " Local inference available (Ollama)\n"
                  << "    Set EUXIS_LOCAL_ONLY=true for routine tasks\n";
    } else {
        std::cout << "  " << term::icon_info() << " Install Ollama for local inference fallback\n";
    }

    // 2. Cache directory size
    auto cache_dir = fs::path(ctx.euxis_home) / "euxis-data" / "runtime" / "provider-usage";
    if (fs::is_directory(cache_dir)) {
        size_t cache_size = 0;
        for (const auto& entry : fs::recursive_directory_iterator(cache_dir)) {
            if (entry.is_regular_file()) cache_size += entry.file_size();
        }
        double mb = static_cast<double>(cache_size) / (1024.0 * 1024.0);
        std::cout << "  " << term::icon_info() << " Cache size: "
                  << std::fixed << std::setprecision(1) << mb << " MB\n";
        if (mb > 100) {
            std::cout << "    Consider running: euxis bus clean\n";
        }
    }

    // 3. Provider count
    auto providers = router.available_providers();
    std::cout << "  " << term::icon_info() << " Available providers: " << providers.size() << "\n";
    if (providers.size() >= 2) {
        std::cout << "    Failover chain active\n";
    }

    return 0;
}

} // namespace euxis::cli::cmd
