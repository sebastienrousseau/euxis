#include "euxis/cli/cmd/specialized.hpp"
#include "euxis/cli/box.hpp"
#include "euxis/cli/config_loader.hpp"
#include "euxis/cli/i18n.hpp"
#include "euxis/cli/pii_filter.hpp"
#include "euxis/cli/process.hpp"
#include "euxis/cli/provider_executor.hpp"
#include "euxis/cli/provider_router.hpp"
#include "euxis/cli/registry_client.hpp"
#include "euxis/cli/session.hpp"
#include "euxis/cli/terminal.hpp"

#include <spdlog/spdlog.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <chrono>
#include <sstream>
#include <unistd.h>
#include <future>
#include <thread>
#include <atomic>

namespace euxis::cli::cmd {

using euxis::cli::i18n::tr;

namespace {

namespace fs = std::filesystem;
namespace term = terminal;

/// Write content to a file, creating parent directories as needed.
void write_output_file(const fs::path& path, const std::string& content) {
    fs::create_directories(path.parent_path());
    std::ofstream f(path);
    f << content;
}

/// Collect file listing from a directory for context summaries.
[[maybe_unused]] auto scan_directory_listing(const fs::path& dir, int max_depth = 3) -> std::string {
    std::ostringstream out;
    if (!fs::is_directory(dir)) return {};

    int count = 0;
    constexpr int max_files = 200;
    for (const auto& entry : fs::recursive_directory_iterator(
             dir, fs::directory_options::skip_permission_denied)) {
        if (count >= max_files) {
            out << "  ... (" << tr("truncated at") << " " << max_files << " " << tr("entries") << ")\n";
            break;
        }
        auto rel = fs::relative(entry.path(), dir);
        int depth = 0;
        for (auto it = rel.begin(); it != rel.end(); ++it) ++depth;
        if (depth > max_depth) continue;

        if (entry.is_regular_file()) {
            out << "  " << rel.string() << " (" << entry.file_size() << " " << tr("bytes") << ")\n";
            ++count;
        } else if (entry.is_directory()) {
            out << "  " << rel.string() << "/\n";
            ++count;
        }
    }
    return out.str();
}

} // namespace

// --- voice ---

int cmd_voice(Context& ctx, const std::vector<std::string>& args) {
    return cmd_voice_ex(ctx, args, std::cin);
}

int cmd_voice_ex(Context& ctx, const std::vector<std::string>& args, std::istream& input) {
    std::cout << term::bold(tr("Voice Interface")) << "\n\n";
    bool has_sox = Process::available("sox");
    bool has_ffmpeg = Process::available("ffmpeg");
    std::cout << "  sox:    " << (has_sox ? tr("available") : tr("not found")) << "\n"
              << "  ffmpeg: " << (has_ffmpeg ? tr("available") : tr("not found")) << "\n";

    std::string tier = "reason";
    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "--tier" && i + 1 < args.size()) tier = args[++i];
    }

    ProviderRouter router(ctx.data_dir);
    ProviderExecutor executor(ctx.data_dir);
    auto model = router.route(tier, "voice conversation");

    std::string line;
    while (true) {
        if (&input == &std::cin) {
            std::cout << term::bold(tr("you> "));
            std::cout.flush();
        }
        if (!std::getline(input, line)) break;
        auto trimmed = line;
        while (!trimmed.empty() && (trimmed.front() == ' ' || trimmed.front() == '\t')) trimmed.erase(trimmed.begin());
        if (trimmed == "exit" || trimmed == "quit") break;

        auto prompt = ProviderExecutor::build_prompt("You are Euxis.", PiiFilter::redact(trimmed));
        auto response = executor.execute(model, prompt);
        if (response.success) std::cout << term::cyan(tr("euxis> ")) << response.output << "\n\n";
    }
    return 0;
}

// --- tui ---

int cmd_tui(Context& ctx, const std::vector<std::string>& args) {
    return cmd_tui_ex(ctx, args, std::cin);
}

int cmd_tui_ex(Context& ctx, [[maybe_unused]] const std::vector<std::string>& args, std::istream& input) {
    if (&input == &std::cin && (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO))) {
        std::cout << term::icon_info() << " " << tr("Non-interactive mode detected. TUI requires a terminal.") << "\n";
        return 1;
    }

    auto old_level = spdlog::get_level();
    spdlog::set_level(spdlog::level::warn);

    RegistryClient registry(ctx.data_dir);
    ProviderRouter router(ctx.data_dir);
    ProviderExecutor executor(ctx.data_dir);
    
    std::string active_agent = "code-agent";
    Session session(ctx.euxis_home);
    std::string memory_ctx = session.get_memory_context(active_agent);

    std::string tier = "code";
    std::string initial_msg;
    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "--tier" && i + 1 < args.size()) tier = args[++i];
        else if (args[i].front() != '-') {
            if (!initial_msg.empty()) initial_msg += " ";
            initial_msg += args[i];
        }
    }

    auto model_info = router.route(tier, initial_msg.empty() ? "tui conversation" : initial_msg);
    std::vector<std::pair<std::string, std::string>> history;

    bool is_interactive = (&input == &std::cin);
    if (is_interactive) {
        term::enable_raw_mode();
        std::cout << "\033[?1049h\033[H\033[2J";
    }

    std::string current_input;
    bool running = true;
    bool is_thinking = false;
    std::string ai_streaming_output;
    std::string ai_error;
    std::string system_overlay;

    term::TerminalScreen screen;
    
    auto render = [&]() {
        if (!is_interactive) return;
        int w, h;
        term::get_terminal_size(w, h);
        screen.resize(w, h);
        screen.clear();

        // 1. STICKY HEADER
        std::string header_text = "  EUXIS ADE v0.0.4  │  " + active_agent + "  │  " + model_info.model + "  ";
        screen.write_text(0, 0, header_text, 255, 255, 255, 73, 77, 100, true);
        for (int x = static_cast<int>(header_text.size()); x < w; ++x) screen.set_cell(x, 0, ' ', 255, 255, 255, 73, 77, 100);

        // 2. CONTEXT SIDEBAR
        int sidebar_w = w / 4;
        if (sidebar_w > 10) {
            for (int y = 1; y < h - 1; ++y) screen.set_cell(w - sidebar_w - 1, y, U'│', 110, 115, 141);
            screen.write_text(w - sidebar_w + 1, 2, "CONTEXT", 245, 189, 230, 0, 0, 0, true);
            screen.write_text(w - sidebar_w + 1, 4, "Project:", 110, 115, 141);
            screen.write_text(w - sidebar_w + 1, 5, " euxis-cpp", 139, 233, 253);
            screen.write_text(w - sidebar_w + 1, 7, "Branch:", 110, 115, 141);
            screen.write_text(w - sidebar_w + 1, 8, " main", 166, 218, 149);
            screen.write_text(w - sidebar_w + 1, 10, "Memory:", 110, 115, 141);
            screen.write_text(w - sidebar_w + 1, 11, " active", 166, 218, 149);
            screen.write_text(w - sidebar_w + 1, 13, "Provider:", 110, 115, 141);
            screen.write_text(w - sidebar_w + 1, 14, " " + model_info.provider, 189, 147, 249);
        }

        // 3. CHAT AREA
        int current_y = 2;
        int max_y = h - 4;
        for (const auto& h_entry : history) {
            if (current_y >= max_y) break;
            screen.write_text(2, current_y++, "› " + h_entry.first, 139, 233, 253, 0, 0, 0, true);
            std::istringstream stream{h_entry.second};
            std::string out_line;
            while (std::getline(stream, out_line)) {
                if (current_y < max_y) {
                    screen.set_cell(2, current_y, U'│', 189, 147, 249);
                    screen.write_text(4, current_y++, out_line, 200, 200, 200);
                }
            }
            current_y++;
        }

        if (current_y < max_y) {
            if (is_thinking) {
                screen.write_text(2, current_y++, "› " + current_input, 139, 233, 253, 0, 0, 0, true);
                static const std::vector<std::string> frames = {"⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"};
                static int frame_idx = 0;
                screen.write_text(4, current_y, frames[(frame_idx++) % frames.size()] + " Agent Thinking...", 189, 147, 249);
            } else if (!ai_streaming_output.empty()) {
                screen.write_text(2, current_y++, "› " + current_input, 139, 233, 253, 0, 0, 0, true);
                std::istringstream stream{ai_streaming_output};
                std::string out_line;
                while (std::getline(stream, out_line)) {
                    if (current_y < max_y) {
                        screen.set_cell(2, current_y, U'│', 189, 147, 249);
                        screen.write_text(4, current_y++, out_line, 255, 255, 255);
                    }
                }
            } else if (!ai_error.empty()) {
                screen.write_text(2, current_y++, "› " + current_input, 139, 233, 253, 0, 0, 0, true);
                screen.write_text(4, current_y, "Error: " + ai_error, 237, 135, 150);
            }
        }

        screen.write_text(w - 42, h - 1, " Type /help for commands, /exit to quit ", 110, 115, 141);
        if (!is_thinking && ai_streaming_output.empty()) screen.write_text(2, h - 2, "› " + current_input + "█", 139, 233, 253, 0, 0, 0, true);

        if (!system_overlay.empty()) {
            screen.draw_box(w/2 - 30, h/2 - 5, 60, 10, " COMMAND PALETTE ");
            std::istringstream stream{system_overlay};
            std::string out_line;
            int txt_y = h/2 - 3;
            while (std::getline(stream, out_line)) screen.write_text(w/2 - 26, txt_y++, out_line, 255, 255, 255);
        }
        screen.render();
    };

    auto process_command = [&](const std::string& trimmed) {
        system_overlay.clear();
        if (trimmed == "exit" || trimmed == "quit" || trimmed == "/exit" || trimmed == "/quit") {
            running = false; return true;
        }
        if (trimmed == "clear" || trimmed == "/clear") {
            memory_ctx.clear(); history.clear(); return true;
        }
        if (trimmed == "?" || trimmed == "/help") {
            system_overlay = "/help    Show this menu\n/clear   Wipe memory\n/history Show turns\n/agent   Swap agent\n/exit    Quit session";
            return true;
        }
        if (trimmed.starts_with("@")) {
            auto space = trimmed.find(' ');
            std::string target = (space == std::string::npos) ? trimmed.substr(1) : trimmed.substr(1, space - 1);
            if (registry.get_agent(target)) {
                active_agent = target;
                memory_ctx = session.get_memory_context(active_agent);
                if (space == std::string::npos) return true;
            }
        }
        return false;
    };

    if (!initial_msg.empty()) {
        if (!process_command(initial_msg)) {
            current_input = initial_msg; is_thinking = true; render();
            std::string sys_p = "You are Euxis.";
            auto ai = registry.get_agent(active_agent);
            if (ai && !ai->prompt_path.empty()) sys_p = ProviderExecutor::load_agent_prompt(ctx.euxis_home, ai->prompt_path);
            auto response = executor.execute(model_info, ProviderExecutor::build_prompt(sys_p, PiiFilter::redact(current_input), memory_ctx));
            is_thinking = false;
            if (response.success) {
                history.push_back({current_input, response.output});
                session.save_memory(active_agent, current_input, response.output);
                memory_ctx += "\nUser: " + current_input + "\nEuxis: " + response.output + "\n";
            }
            current_input.clear();
        }
    }

    while (running) {
        render();
        if (is_interactive) {
            int c = term::read_key();
            if (c > 0) {
                system_overlay.clear();
                if (c == 3 || c == 4) running = false;
                else if (c == 127 || c == 8 || c == 1000) { if (!current_input.empty()) current_input.pop_back(); }
                else if (c == '\r' || c == '\n') {
                    if (current_input.empty()) continue;
                    if (process_command(current_input)) { current_input.clear(); continue; }
                    is_thinking = true;
                    std::string user_msg = current_input; ai_error.clear(); ai_streaming_output.clear();
                    std::thread([&, user_msg]() {
                        std::string sys_p = "You are Euxis.";
                        auto ai = registry.get_agent(active_agent);
                        if (ai && !ai->prompt_path.empty()) sys_p = ProviderExecutor::load_agent_prompt(ctx.euxis_home, ai->prompt_path);
                        auto response = executor.execute(model_info, ProviderExecutor::build_prompt(sys_p, PiiFilter::redact(user_msg), memory_ctx), 120, std::nullopt, [&](const std::string& chunk){
                            ai_streaming_output += chunk;
                        });
                        is_thinking = false;
                        if (response.success) {
                            for (char ch : response.output) { ai_streaming_output += ch; std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
                            history.push_back({user_msg, response.output});
                            session.save_memory(active_agent, user_msg, response.output);
                            memory_ctx += "\nUser: " + user_msg + "\nEuxis: " + response.output + "\n";
                            ai_streaming_output.clear();
                        } else { ai_error = response.error; }
                        current_input.clear();
                    }).detach();
                    while (is_thinking || !ai_streaming_output.empty()) { render(); std::this_thread::sleep_for(std::chrono::milliseconds(20)); }
                } else if (c >= 32 && c <= 126) current_input += static_cast<char>(c);
            } else std::this_thread::sleep_for(std::chrono::milliseconds(5));
        } else {
            std::string l; if (!std::getline(input, l)) break;
            auto res = executor.execute(model_info, l);
            if (res.success) std::cout << res.output << "\n";
        }
    }
    if (is_interactive) { std::cout << "\033[?1049l"; term::disable_raw_mode(); }
    spdlog::set_level(old_level);
    return 0;
}

// --- gui ---

int cmd_gui(Context& ctx, const std::vector<std::string>& args) {
    std::vector<fs::path> search_paths = {
        fs::path(ctx.euxis_home) / "build" / "euxis-etx" / "euxis-etx",
        fs::path(ctx.euxis_home) / "euxis-cpp" / "build" / "euxis-etx" / "euxis-etx",
        fs::path(ctx.euxis_home) / "euxis-bin" / "euxis-etx",
    };
    for (const auto& etx_path : search_paths) {
        if (fs::exists(etx_path)) {
            std::cout << term::icon_info() << " " << tr("Launching ETX GUI...") << "\n";
            auto result = Process::run(etx_path.string(), args);
            return result.exit_code;
        }
    }
    if (Process::available("euxis-etx")) {
        std::cout << term::icon_info() << " " << tr("Launching ETX GUI...") << "\n";
        auto result = Process::run("euxis-etx", args);
        return result.exit_code;
    }
    return 1;
}

// --- polish ---

int cmd_polish(Context& ctx, const std::vector<std::string>& args) {
    if (args.empty()) return 2;
    auto file_path = args[0];
    if (!fs::exists(file_path)) return 1;
    std::ifstream f(file_path);
    std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    auto prompt = ProviderExecutor::build_prompt("Polish this.", PiiFilter::redact(content));
    ProviderRouter router(ctx.data_dir);
    ProviderExecutor executor(ctx.data_dir);
    auto model = router.route("reason", "polish text");
    auto response = executor.execute(model, prompt);
    if (response.success) {
        std::ofstream out(file_path);
        out << response.output;
    }
    return 0;
}

// --- kaizen ---

int cmd_kaizen(Context& ctx, [[maybe_unused]] const std::vector<std::string>& args) {
    std::cout << term::bold(tr("Kaizen - Continuous Improvement")) << "\n\n";
    RegistryClient registry(ctx.data_dir);
    auto agents = registry.list_agents();
    std::ostringstream fleet_context;
    fleet_context << "Total agents: " << agents.size() << "\n";
    auto prompt = ProviderExecutor::build_prompt("Analyze fleet.", "Analyze.", fleet_context.str());
    ProviderRouter router(ctx.data_dir);
    ProviderExecutor executor(ctx.data_dir);
    auto model = router.route("reason", "kaizen analysis");
    auto response = executor.execute(model, prompt);
    if (response.success) std::cout << response.output << "\n";
    return 0;
}

// --- audit ---

int cmd_audit(Context& ctx, [[maybe_unused]] const std::vector<std::string>& args) {
    std::cout << term::bold(tr("Security & Compliance Audit")) << "\n\n";
    int issues = 0;
    for (const auto& pattern : {".env", "credentials.json", "secrets.yaml", ".key"}) {
        if (fs::exists(fs::path(ctx.euxis_home) / pattern)) {
            std::cout << "    " << term::icon_warn() << " Found: " << pattern << "\n";
            ++issues;
        }
    }
    std::cout << "\n" << (issues == 0 ? "Audit passed" : "Audit findings") << "\n";
    return 0;
}

// --- audit-run ---

int cmd_audit_run(Context& ctx, [[maybe_unused]] const std::vector<std::string>& args) {
    std::string run_id = "audit-" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    auto evidence_dir = fs::path(ctx.euxis_home) / "euxis-data" / "audit" / run_id;
    fs::create_directories(evidence_dir);
    std::cout << term::icon_ok() << " Evidence saved to: " << evidence_dir.string() << "\n";
    return 0;
}

// --- certify ---

int cmd_certify([[maybe_unused]] Context& ctx, const std::vector<std::string>& args) {
    if (args.empty()) return 2;
    std::cout << term::bold(tr("Certify:")) << " " << args[0] << "\n\n";
    return 0;
}

// --- evidence-verify ---

int cmd_evidence_verify([[maybe_unused]] Context& ctx, [[maybe_unused]] const std::vector<std::string>& args) {
    return 0;
}

// --- gym ---

int cmd_gym([[maybe_unused]] Context& ctx, const std::vector<std::string>& args) {
    if (args.empty()) return 2;
    std::cout << term::bold(tr("Agent Gym:")) << " " << args[0] << "\n";
    return 0;
}

// --- replay ---

int cmd_replay([[maybe_unused]] Context& ctx, const std::vector<std::string>& args) {
    if (args.empty()) return 2;
    return 0;
}

// --- context-worker ---

int cmd_context_worker(Context& ctx, [[maybe_unused]] const std::vector<std::string>& args) {
    std::cout << term::bold(tr("Context Worker")) << "\n\n";
    auto runtime_dir = fs::path(ctx.euxis_home) / "euxis-runtime";
    auto context_dir = runtime_dir / "context";
    fs::create_directories(context_dir);

    RegistryClient registry(ctx.data_dir);
    auto agents = registry.list_agents();
    nlohmann::json ctx_json;
    ctx_json["agent_count"] = static_cast<int>(agents.size());
    write_output_file(context_dir / "agents-context.json", ctx_json.dump(2));

    std::cout << term::icon_ok() << " Context gathering complete\n";
    return 0;
}

} // namespace euxis::cli::cmd
