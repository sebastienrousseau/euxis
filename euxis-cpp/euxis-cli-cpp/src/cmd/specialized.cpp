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
#include <algorithm>

namespace euxis::cli::cmd {

using euxis::cli::i18n::tr;

namespace {
namespace fs = std::filesystem;
namespace term = terminal;
} // namespace

// --- voice ---
int cmd_voice(Context& ctx, const std::vector<std::string>& args) {
    return cmd_voice_ex(ctx, args, std::cin);
}

int cmd_voice_ex(Context& ctx, const std::vector<std::string>& args, std::istream& input) {
    std::string tier = "reason";
    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "--tier" && i + 1 < args.size()) tier = args[++i];
    }
    ProviderRouter router(ctx.data_dir);
    ProviderExecutor executor(ctx.data_dir);
    auto model = router.route(tier, "voice conversation");
    std::string line;
    while (std::getline(input, line)) {
        if (line == "exit" || line == "quit") break;
        auto res = executor.execute(model, ProviderExecutor::build_prompt("You are Euxis.", PiiFilter::redact(line)));
        if (res.success) std::cout << "euxis> " << res.output << "\n\n";
    }
    return 0;
}

// --- tui ---
int cmd_tui(Context& ctx, const std::vector<std::string>& args) {
    return cmd_tui_ex(ctx, args, std::cin);
}

int cmd_tui_ex(Context& ctx, [[maybe_unused]] const std::vector<std::string>& args, std::istream& input) {
    if (&input == &std::cin && (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO))) return 1;

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
    std::vector<std::string> cmd_suggestions = {
        "/help", "/model ", "/fleet", "/agent ", "/auth", "/clear", "/exit", "/quit", "/about", "/history"
    };

    bool is_interactive = (&input == &std::cin);
    if (is_interactive) {
        term::enable_raw_mode();
        std::cout << "\033[?1049h\033[H\033[2J\033[?25l";
    }

    std::string current_input;
    std::string ghost_text;
    bool running = true;
    bool is_thinking = false;
    std::string thinking_phrase = "analyzing...";
    std::string ai_streaming_output;
    std::string ai_error;

    term::TerminalScreen screen;
    
    auto get_prediction = [&](const std::string& input_str) -> std::string {
        if (input_str.empty()) return "";
        for (const auto& s : cmd_suggestions) {
            if (s.size() > input_str.size() && s.substr(0, input_str.size()) == input_str) return s.substr(input_str.size());
        }
        return "";
    };

    auto render = [&]() {
        if (!is_interactive) return;
        int w, h;
        term::get_terminal_size(w, h);
        screen.resize(w, h);
        screen.clear();

        // 1. HEADER (Tokyo Dark Gradient)
        std::string h1 = " EUXIS ADE v0.0.6 ";
        std::string h2 = " │ agent:" + active_agent + " │ " + model_info.model + " ";
        screen.write_gradient(0, 0, h1 + h2, 113, 153, 238, 164, 133, 221);
        for (int x = (int)(h1.size() + h2.size()); x < w; ++x) screen.set_cell(x, 0, ' ', 255, 255, 255, 26, 27, 42);

        // 2. CHAT AREA
        int current_y = 2;
        int max_y = h - 4;
        
        // Show as many turns as possible that fit top-down
        size_t start_idx = 0;
        // Basic heuristic to avoid scrolling issues in the simple renderer
        if (history.size() > 20) start_idx = history.size() - 20;

        for (size_t i = start_idx; i < history.size(); ++i) {
            const auto& h_entry = history[i];
            if (current_y >= max_y) break;
            
            bool is_cmd = h_entry.first.starts_with("/");
            screen.write_text(4, current_y++, "➜ " + h_entry.first, 56, 168, 157, 0, 0, 0, true);
            std::istringstream stream{h_entry.second};
            std::string out_line;
            while (std::getline(stream, out_line)) {
                if (current_y < max_y) {
                    if (is_cmd) screen.set_cell(4, current_y, U'┃', 68, 75, 106);
                    else screen.set_cell(4, current_y, U'┃', 164, 133, 221);
                    screen.write_text(6, current_y++, out_line.substr(0, w-10), 169, 177, 214);
                }
            }
            current_y++;
        }

        if (current_y < max_y) {
            if (is_thinking) {
                screen.write_text(4, current_y++, "➜ " + current_input, 56, 168, 157, 0, 0, 0, true);
                static const std::vector<std::string> frames = {"⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"};
                static int f_idx = 0;
                screen.write_text(6, current_y, frames[(f_idx++/2)%10] + " " + thinking_phrase, 164, 133, 221);
            } else if (!ai_streaming_output.empty() || !ai_error.empty()) {
                screen.write_text(4, current_y++, "➜ " + current_input, 56, 168, 157, 0, 0, 0, true);
                std::istringstream stream{ai_streaming_output.empty() ? "Error: " + ai_error : ai_streaming_output};
                std::string out_line;
                while (std::getline(stream, out_line)) {
                    if (current_y < max_y) {
                        screen.set_cell(4, current_y, U'┃', 164, 133, 221);
                        screen.write_text(6, current_y++, out_line.substr(0, w-10), 169, 177, 214);
                    }
                }
            }
        }

        // 3. FOOTER
        std::string f_hint = " Tab:autocomplete │ /model:switch │ /help:all ";
        screen.write_text(w - (int)f_hint.size() - 4, h - 1, f_hint, 68, 75, 106, 26, 27, 42);
        for(int x=0; x < w - (int)f_hint.size() - 4; ++x) screen.set_cell(x, h - 1, ' ', 0, 0, 0, 26, 27, 42);

        screen.write_text(4, h - 2, "➜ ", 56, 168, 157, 0, 0, 0, true);
        screen.write_text(7, h - 2, current_input, 169, 177, 214);
        ghost_text = get_prediction(current_input);
        if (!ghost_text.empty()) screen.write_text(7 + (int)current_input.size(), h - 2, ghost_text, 68, 75, 106);
        screen.set_cell(7 + (int)current_input.size(), h - 2, U' ', 255, 255, 255, 169, 177, 214);

        screen.render();
    };

    auto process_command = [&](const std::string& trimmed) {
        if (trimmed == "exit" || trimmed == "quit" || trimmed == "/exit" || trimmed == "/quit") { running = false; return true; }
        if (trimmed == "clear" || trimmed == "/clear") { memory_ctx.clear(); history.clear(); return true; }
        
        std::string out;
        if (trimmed == "/about") { out = "EUXIS ADE v0.0.6\nTokyo Dark Edition\nAgentic Orchestrator [C++23]"; }
        else if (trimmed == "/auth") { out = "Active Profiles:\n - claude (Anthropic API)\n - gemini (Google AI)\n - local (Llama 3)"; }
        else if (trimmed == "/fleet") {
            auto agents = registry.list_agents();
            std::ostringstream oss;
            oss << "Active Fleet (" << agents.size() << " agents):\n";
            for (size_t i = 0; i < agents.size(); ++i) {
                std::string desc = agents[i].role;
                if (agents[i].manifesto && !agents[i].manifesto->identity.description.empty()) {
                    desc = agents[i].manifesto->identity.description;
                }
                if (desc.size() > 150) desc = desc.substr(0, 147) + "...";
                oss << std::format(" {:2d}. {:<15} - {}\n", i + 1, agents[i].id, desc);
            }
            out = oss.str();
        }
        else if (trimmed.starts_with("/model ")) {
            std::string new_tier = trimmed.substr(7);
            model_info = router.route(new_tier, "model switch");
            out = "Switched to " + model_info.provider + " (" + model_info.model + ")";
        }
        else if (trimmed == "/help") {
            out = "Commands:\n /model <tier>  Switch AI (code|reason|data)\n /agent <id|num> Switch personality\n /fleet         List all agents\n /history       Show session history\n /clear         Wipe session\n /exit          Quit";
        }
        else if (trimmed == "/history") {
            std::ostringstream oss;
            oss << "Session History (" << history.size() << " turns):\n";
            for (const auto& h : history) {
                std::string summary = h.first;
                if (summary.size() > 40) summary = summary.substr(0, 37) + "...";
                oss << " - " << summary << "\n";
            }
            out = oss.str();
        }
        
        if (!out.empty()) {
            history.push_back({trimmed, out});
            return true;
        }

        if (trimmed.starts_with("/agent ") || trimmed.starts_with("@")) {
            size_t start = trimmed.starts_with("@") ? 1 : 7;
            auto space = trimmed.find(' ', start);
            std::string target = (space == std::string::npos) ? trimmed.substr(start) : trimmed.substr(start, space - start);
            
            auto agents = registry.list_agents();
            bool found = false;
            try {
                size_t idx = std::stoul(target);
                if (idx > 0 && idx <= agents.size()) {
                    target = agents[idx - 1].id;
                    found = true;
                }
            } catch (...) {}

            if (!found && registry.get_agent(target)) found = true;

            if (found) {
                active_agent = target;
                memory_ctx = session.get_memory_context(active_agent);
                history.push_back({trimmed, "Switched agent to: " + active_agent});
                return true;
            } else {
                history.push_back({trimmed, "Unknown agent: " + target + ". Use /fleet to see available agents."});
                return true;
            }
        }
        return false;
    };

    while (running) {
        render();
        if (is_interactive) {
            int c = term::read_key();
            if (c > 0) {
                if (c == 3 || c == 4) running = false;
                else if (c == 9) { if (!ghost_text.empty()) current_input += ghost_text; }
                else if (c == 127 || c == 8 || c == 1000) { if (!current_input.empty()) current_input.pop_back(); }
                else if (c == '\r' || c == '\n') {
                    if (current_input.empty()) continue;
                    if (process_command(current_input)) { current_input.clear(); continue; }
                    is_thinking = true;
                    static const std::vector<std::string> phrases = {
                        "exploring codebase...", "analyzing patterns...", "synthesizing response...", "querying agent...", "doing research on memes..."
                    };
                    thinking_phrase = phrases[rand() % phrases.size()];
                    std::string user_msg = current_input; ai_error.clear(); ai_streaming_output.clear();
                    std::thread([&, user_msg]() {
                        auto ai = registry.get_agent(active_agent);
                        std::string sys_p = (ai && !ai->prompt_path.empty()) ? ProviderExecutor::load_agent_prompt(ctx.euxis_home, ai->prompt_path) : "You are Euxis.";
                        auto response = executor.execute(model_info, ProviderExecutor::build_prompt(sys_p, PiiFilter::redact(user_msg), memory_ctx), 120, std::nullopt, [&](const std::string& chunk){
                            ai_streaming_output += chunk;
                        });
                        is_thinking = false;
                        if (response.success) {
                            history.push_back({user_msg, response.output});
                            memory_ctx += "\nUser: " + user_msg + "\nEuxis: " + response.output + "\n";
                            ai_streaming_output.clear();
                        } else ai_error = response.error;
                        current_input.clear();
                    }).detach();
                    while (is_thinking || !ai_streaming_output.empty()) { render(); std::this_thread::sleep_for(std::chrono::milliseconds(30)); }
                } else if (c >= 32 && c <= 126) current_input += (char)c;
            } else std::this_thread::sleep_for(std::chrono::milliseconds(10));
        } else {
            std::string l; if (!std::getline(input, l)) break;
            auto res = executor.execute(model_info, l);
            if (res.success) std::cout << res.output << "\n";
        }
    }
    if (is_interactive) { std::cout << "\033[?1049l\033[?25h"; term::disable_raw_mode(); }
    spdlog::set_level(old_level);
    return 0;
}

int cmd_gui([[maybe_unused]] Context& ctx, [[maybe_unused]] const std::vector<std::string>& args) { return 0; }
int cmd_polish([[maybe_unused]] Context& ctx, [[maybe_unused]] const std::vector<std::string>& args) { return 0; }
int cmd_kaizen([[maybe_unused]] Context& ctx, [[maybe_unused]] const std::vector<std::string>& args) { return 0; }
int cmd_audit([[maybe_unused]] Context& ctx, [[maybe_unused]] const std::vector<std::string>& args) { return 0; }
int cmd_audit_run([[maybe_unused]] Context& ctx, [[maybe_unused]] const std::vector<std::string>& args) { return 0; }
int cmd_certify([[maybe_unused]] Context& ctx, [[maybe_unused]] const std::vector<std::string>& args) { return 0; }
int cmd_evidence_verify([[maybe_unused]] Context& ctx, [[maybe_unused]] const std::vector<std::string>& args) { return 0; }
int cmd_gym([[maybe_unused]] Context& ctx, [[maybe_unused]] const std::vector<std::string>& args) { return 0; }
int cmd_replay([[maybe_unused]] Context& ctx, [[maybe_unused]] const std::vector<std::string>& args) { return 0; }
int cmd_context_worker([[maybe_unused]] Context& ctx, [[maybe_unused]] const std::vector<std::string>& args) { return 0; }

} // namespace euxis::cli::cmd
