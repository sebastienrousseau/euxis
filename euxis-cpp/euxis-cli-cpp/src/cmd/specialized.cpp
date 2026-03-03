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

std::string synthesize_description(const AgentInfo& a) {
    if (a.manifesto && !a.manifesto->identity.description.empty()) return a.manifesto->identity.description;
    // Fallback synthesis
    std::string d = "Expert " + a.role + " operating at the " + a.tier + " tier. Specialized in autonomous task execution and domain-specific problem solving.";
    if (d.size() > 150) d = d.substr(0, 147) + "...";
    return d;
}
} // namespace

// --- voice ---
int cmd_voice(Context& ctx, const std::vector<std::string>& args) { return cmd_voice_ex(ctx, args, std::cin); }
int cmd_voice_ex(Context& ctx, const std::vector<std::string>& args, std::istream& input) {
    std::string tier = "reason";
    for (size_t i = 0; i < args.size(); ++i) { if (args[i] == "--tier" && i + 1 < args.size()) tier = args[++i]; }
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
    std::vector<std::string> cmd_suggestions = { "/help", "/model ", "/fleet", "/agent ", "/auth", "/clear", "/exit", "/about", "/history" };

    bool is_interactive = (&input == &std::cin);
    
    auto tokyo_cyan    = [](const std::string& s) { return term::rgb_fg(56, 168, 157, s); };
    auto tokyo_magenta = [](const std::string& s) { return term::rgb_fg(164, 133, 221, s); };
    auto tokyo_blue    = [](const std::string& s) { return term::rgb_fg(113, 153, 238, s); };
    auto tokyo_text    = [](const std::string& s) { return term::rgb_fg(169, 177, 214, s); };
    auto tokyo_dim     = [](const std::string& s) { return term::rgb_fg(68, 75, 106, s); };
    auto tokyo_error   = [](const std::string& s) { return term::rgb_fg(247, 118, 142, s); };

    if (is_interactive) {
        term::enable_raw_mode();
        std::cout << "\n  " << term::bold(tokyo_magenta("EUXIS ADE")) << tokyo_dim(" v0.0.7") << "\n";
        std::cout << "  " << tokyo_dim("Interactive Pager and Fleet Explorer enabled.\n\n");
    }

    std::string current_input;
    std::string ghost_text;
    bool running = true;

    auto get_prediction = [&](const std::string& input_str) -> std::string {
        if (input_str.empty()) return "";
        for (const auto& s : cmd_suggestions) { if (s.size() > input_str.size() && s.substr(0, input_str.size()) == input_str) return s.substr(input_str.size()); }
        return "";
    };

    auto draw_prompt = [&]() {
        if (!is_interactive) return;
        std::cout << "\r\033[K" << term::bold(tokyo_cyan(" ➜  ")) << tokyo_text(current_input);
        ghost_text = get_prediction(current_input);
        if (!ghost_text.empty()) { std::cout << tokyo_dim(ghost_text) << "\033[" << ghost_text.size() << "D"; }
        std::cout.flush();
    };

    auto run_fleet_browser = [&]() {
        auto agents = registry.list_agents();
        if (agents.empty()) return;
        
        int selected = 0;
        int scroll_top = 0;
        bool choosing = true;
        term::TerminalScreen browser_screen;

        while (choosing) {
            int w, h;
            term::get_terminal_size(w, h);
            browser_screen.resize(w, h);
            browser_screen.clear();

            int view_h = h - 6;
            if (selected < scroll_top) scroll_top = selected;
            if (selected >= scroll_top + view_h) scroll_top = selected - view_h + 1;

            browser_screen.write_gradient(0, 0, " EUXIS FLEET EXPLORER │ Use Up/Down to navigate, Enter to select, Esc to cancel ", 113, 153, 238, 164, 133, 221);
            
            for (int i = 0; i < view_h && (scroll_top + i) < (int)agents.size(); ++i) {
                int idx = scroll_top + i;
                bool is_sel = (idx == selected);
                int y = 2 + i;
                
                if (is_sel) {
                    browser_screen.write_text(2, y, "➜ " + agents[idx].id, 56, 168, 157, 26, 27, 42, true);
                    browser_screen.write_text(20, y, synthesize_description(agents[idx]), 169, 177, 214, 26, 27, 42);
                } else {
                    browser_screen.write_text(4, y, agents[idx].id, 113, 153, 238);
                    browser_screen.write_text(20, y, synthesize_description(agents[idx]), 68, 75, 106);
                }
            }

            // Scrollbar
            if ((int)agents.size() > view_h) {
                int bar_h = std::max(1, (view_h * view_h) / (int)agents.size());
                int bar_y = 2 + (scroll_top * (view_h - bar_h)) / std::max(1, (int)agents.size() - view_h);
                for (int i = 0; i < view_h; ++i) browser_screen.set_cell(w - 1, 2 + i, U'│', 68, 75, 106);
                for (int i = 0; i < bar_h; ++i) browser_screen.set_cell(w - 1, bar_y + i, U'┃', 164, 133, 221);
            }

            browser_screen.render();
            
            int k = term::read_key();
            if (k == 1001) { if (selected > 0) selected--; } // Up
            else if (k == 1002) { if (selected < (int)agents.size() - 1) selected++; } // Down
            else if (k == '\r' || k == '\n') {
                active_agent = agents[selected].id;
                memory_ctx = session.get_memory_context(active_agent);
                choosing = false;
                std::cout << "\033[H\033[2J\r  " << tokyo_dim("Switched to ") << term::bold(tokyo_cyan(active_agent)) << "\n\n";
            } else if (k == 27 || k == 'q') choosing = false; // Esc or q
        }
    };

    auto process_command = [&](const std::string& trimmed) {
        if (trimmed == "exit" || trimmed == "quit" || trimmed == "/exit" || trimmed == "/quit") { running = false; return true; }
        if (trimmed == "clear" || trimmed == "/clear") { memory_ctx.clear(); history.clear(); std::cout << "\r\033[K  " << tokyo_dim("History wiped.\n\n"); return true; }
        
        if (trimmed == "/fleet") { run_fleet_browser(); return true; }
        
        std::string out;
        if (trimmed == "/about") { out = "EUXIS ADE v0.0.7\nTokyo Dark Edition\nAgentic Orchestrator [C++23]"; }
        else if (trimmed == "/auth") { out = "Profiles:\n - claude (Anthropic)\n - gemini (Google)\n - local (Ollama)"; }
        else if (trimmed.starts_with("/model")) {
            if (trimmed == "/model") out = "Tiers: routine, data, code, reason. Use /model <tier>";
            else { model_info = router.route(trimmed.substr(7), "switch"); out = "Switched to " + model_info.provider + " (" + model_info.model + ")"; }
        }
        else if (trimmed == "/help") out = "Commands:\n /model <tier>  Switch AI\n /agent <id>    Switch Role\n /fleet         Fleet Browser\n /history       Session Turns\n /clear         Wipe history\n /exit          Quit";
        else if (trimmed == "/history") out = "History: " + std::to_string(history.size()) + " turns.";

        if (!out.empty()) { history.push_back({trimmed, out}); std::cout << "\r\033[K " << term::bold(tokyo_blue("◆ " + trimmed)) << "\n"; std::istringstream s(out); std::string l; while(std::getline(s, l)) std::cout << tokyo_dim(" ┃  ") << tokyo_text(l) << "\n"; std::cout << "\n"; return true; }

        if (trimmed.starts_with("/agent ") || trimmed.starts_with("@")) {
            size_t start = trimmed.starts_with("@") ? 1 : 7;
            std::string target = trimmed.substr(start);
            if (registry.get_agent(target)) {
                active_agent = target; memory_ctx = session.get_memory_context(active_agent);
                std::cout << "\r\033[K  " << tokyo_dim("Switched to ") << term::bold(tokyo_cyan(active_agent)) << "\n\n";
                return true;
            }
        }
        return false;
    };

    while (running) {
        draw_prompt();
        if (is_interactive) {
            int c = term::read_key();
            if (c > 0) {
                if (c == 3 || c == 4) running = false;
                else if (c == 9 || c == 1003) { if (!ghost_text.empty()) current_input += ghost_text; }
                else if (c == 127 || c == 8 || c == 1000) { if (!current_input.empty()) current_input.pop_back(); }
                else if (c == '\r' || c == '\n') {
                    if (current_input.empty()) continue;
                    std::string user_msg = current_input; current_input.clear();
                    if (process_command(user_msg)) continue;
                    
                    std::cout << "\r\033[K " << term::bold(tokyo_cyan("● You")) << "  " << tokyo_dim(active_agent) << "\n";
                    std::istringstream u_stream(user_msg); std::string u_line; while (std::getline(u_stream, u_line)) std::cout << tokyo_dim(" ┃  ") << tokyo_text(u_line) << "\n";
                    std::cout << "\n";

                    std::atomic<bool> is_thinking{true};
                    std::thread spinner_thread([&]() {
                        static const std::vector<std::string> f = {"⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"};
                        int i = 0; while (is_thinking) {
                            std::cout << "\r\033[K  " << tokyo_magenta(f[(i++) % 10]) << " " << tokyo_dim("researching codebase...");
                            std::cout.flush(); std::this_thread::sleep_for(std::chrono::milliseconds(80));
                        }
                    });

                    auto ai = registry.get_agent(active_agent);
                    std::string sys_p = (ai && !ai->prompt_path.empty()) ? ProviderExecutor::load_agent_prompt(ctx.euxis_home, ai->prompt_path) : "You are Euxis.";
                    
                    bool first = true; bool need_p = false;
                    auto response = executor.execute(model_info, ProviderExecutor::build_prompt(sys_p, PiiFilter::redact(user_msg), memory_ctx), 120, std::nullopt, [&](const std::string& chunk){
                        if (first) { is_thinking = false; spinner_thread.join(); std::cout << "\r\033[K" << " " << term::bold(tokyo_magenta("◆ Euxis")) << "  " << tokyo_dim(model_info.model) << "\n"; first = false; need_p = true; }
                        for (char ch : chunk) { if (need_p) { std::cout << tokyo_magenta(" ┃  "); need_p = false; } std::cout << tokyo_text(std::string(1, ch)); if (ch == '\n') need_p = true; }
                        std::cout.flush(); std::this_thread::sleep_for(std::chrono::milliseconds(2));
                    });

                    if (first) { is_thinking = false; spinner_thread.join(); std::cout << "\r\033[K" << " " << term::bold(tokyo_magenta("◆ Euxis")) << "  " << tokyo_dim(model_info.model) << "\n"; }
                    if (response.success) { history.push_back({user_msg, response.output}); memory_ctx += "\nUser: " + user_msg + "\nEuxis: " + response.output + "\n"; std::cout << "\n\n"; }
                    else std::cout << tokyo_error(" ┃  Error: " + response.error) << "\n\n";
                } else if (c >= 32 && c <= 126) current_input += (char)c;
            } else std::this_thread::sleep_for(std::chrono::milliseconds(5));
        } else {
            std::string l; if (!std::getline(input, l)) break;
            auto res = executor.execute(model_info, l); if (res.success) std::cout << res.output << "\n";
        }
    }
    if (is_interactive) { std::cout << "\r\033[K\n"; term::disable_raw_mode(); }
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
