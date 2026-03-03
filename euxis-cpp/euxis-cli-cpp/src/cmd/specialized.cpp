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

[[maybe_unused]] void write_output_file(const fs::path& path, const std::string& content) {
    fs::create_directories(path.parent_path());
    std::ofstream f(path);
    f << content;
}

[[maybe_unused]] auto scan_directory_listing(const fs::path& dir, int max_depth = 3) -> std::string {
    std::ostringstream out;
    if (!fs::is_directory(dir)) return {};
    int count = 0;
    constexpr int max_files = 200;
    for (const auto& entry : fs::recursive_directory_iterator(dir, fs::directory_options::skip_permission_denied)) {
        if (count >= max_files) {
            out << "  ... (" << tr("truncated at") << " " << max_files << " " << tr("entries") << ")\n";
            break;
        }
        auto rel = fs::relative(entry.path(), dir);
        int depth = 0;
        for (auto it = rel.begin(); it != rel.end(); ++it) ++depth;
        if (depth > max_depth) continue;
        if (entry.is_regular_file()) { out << "  " << rel.string() << " (" << entry.file_size() << " " << tr("bytes") << ")\n"; ++count; } 
        else if (entry.is_directory()) { out << "  " << rel.string() << "/\n"; ++count; }
    }
    return out.str();
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
    std::vector<std::string> cmd_suggestions = {
        "/help", "/model ", "/fleet", "/agent ", "/auth", "/clear", "/exit", "/quit", "/about", "/history"
    };

    bool is_interactive = (&input == &std::cin);
    
    // Tokyo Dark Theme Lambdas
    auto tokyo_cyan    = [](const std::string& s) { return term::rgb_fg(56, 168, 157, s); };
    auto tokyo_magenta = [](const std::string& s) { return term::rgb_fg(164, 133, 221, s); };
    auto tokyo_blue    = [](const std::string& s) { return term::rgb_fg(113, 153, 238, s); };
    auto tokyo_text    = [](const std::string& s) { return term::rgb_fg(169, 177, 214, s); };
    auto tokyo_dim     = [](const std::string& s) { return term::rgb_fg(68, 75, 106, s); };
    auto tokyo_error   = [](const std::string& s) { return term::rgb_fg(247, 118, 142, s); };

    if (is_interactive) {
        term::enable_raw_mode();
        std::cout << "\n";
        std::cout << "  " << term::bold(tokyo_magenta("EUXIS ADE")) << tokyo_dim(" v0.0.7") << "\n";
        std::cout << "  " << tokyo_dim("Engine: ") << tokyo_blue("C++23 Native Stream") << "\n";
        std::cout << "  " << tokyo_dim("Theme:  ") << tokyo_text("Tokyo Dark Elite") << "\n";
        std::cout << "  " << tokyo_dim("Press Tab or Right Arrow to autocomplete. /help for commands.") << "\n\n";
    }

    std::string current_input;
    std::string ghost_text;
    bool running = true;

    auto get_prediction = [&](const std::string& input_str) -> std::string {
        if (input_str.empty()) return "";
        for (const auto& s : cmd_suggestions) {
            if (s.size() > input_str.size() && s.substr(0, input_str.size()) == input_str) return s.substr(input_str.size());
        }
        return "";
    };

    auto draw_prompt = [&]() {
        if (!is_interactive) return;
        std::cout << "\r\033[K"; // Clear line
        std::cout << term::bold(tokyo_cyan(" ➜  ")) << tokyo_text(current_input);
        ghost_text = get_prediction(current_input);
        if (!ghost_text.empty()) {
            std::cout << tokyo_dim(ghost_text);
            std::cout << "\033[" << ghost_text.size() << "D"; // Move cursor back
        }
        std::cout.flush();
    };

    auto process_command = [&](const std::string& trimmed) {
        if (trimmed == "exit" || trimmed == "quit" || trimmed == "/exit" || trimmed == "/quit") { running = false; return true; }
        if (trimmed == "clear" || trimmed == "/clear") { memory_ctx.clear(); history.clear(); std::cout << "\r\033[K  " << tokyo_dim("Session history wiped.\n\n"); return true; }
        
        std::cout << "\r\033[K"; // Clear prompt before output

        if (trimmed == "/about") {
            std::cout << " " << term::bold(tokyo_blue("◆ About EUXIS")) << "\n"
                      << tokyo_dim(" ┃  ") << tokyo_text("Version:   0.0.7") << "\n"
                      << tokyo_dim(" ┃  ") << tokyo_text("Engine:    C++23 Agentic Orchestrator") << "\n"
                      << tokyo_dim(" ┃  ") << tokyo_text("Theme:     Tokyo Dark Elite") << "\n\n";
            return true;
        }
        if (trimmed == "/auth") {
            std::cout << " " << term::bold(tokyo_blue("◆ Active Profiles")) << "\n"
                      << tokyo_dim(" ┃  ") << tokyo_text("- claude (Anthropic API)") << "\n"
                      << tokyo_dim(" ┃  ") << tokyo_text("- gemini (Google AI)") << "\n"
                      << tokyo_dim(" ┃  ") << tokyo_text("- local  (Llama 3)") << "\n\n";
            return true;
        }
        if (trimmed == "/fleet") {
            auto agents = registry.list_agents();
            std::cout << " " << term::bold(tokyo_blue("◆ Active Fleet")) << " " << tokyo_dim(std::to_string(agents.size()) + " agents") << "\n";
            for (size_t i = 0; i < agents.size(); ++i) {
                std::string desc = agents[i].role;
                if (agents[i].manifesto && !agents[i].manifesto->identity.description.empty()) {
                    desc = agents[i].manifesto->identity.description;
                }
                if (desc.size() > 90) desc = desc.substr(0, 87) + "...";
                std::cout << tokyo_dim(" ┃  ") << std::format("{:2d}. ", i + 1) << term::bold(tokyo_cyan(std::format("{:<16}", agents[i].id))) << " " << tokyo_text(desc) << "\n";
            }
            std::cout << "\n";
            return true;
        }
        if (trimmed.starts_with("/model")) {
            if (trimmed == "/model") {
                std::cout << " " << term::bold(tokyo_blue("◆ Available Tiers")) << "\n"
                          << tokyo_dim(" ┃  ") << tokyo_cyan(std::format("{:<10}", "routine")) << tokyo_text("Fast / Local inference") << "\n"
                          << tokyo_dim(" ┃  ") << tokyo_cyan(std::format("{:<10}", "data")) << tokyo_text("Structured / Extraction tasks") << "\n"
                          << tokyo_dim(" ┃  ") << tokyo_cyan(std::format("{:<10}", "code")) << tokyo_text("Software engineering") << "\n"
                          << tokyo_dim(" ┃  ") << tokyo_cyan(std::format("{:<10}", "reason")) << tokyo_text("Complex logic and architecture") << "\n"
                          << tokyo_dim(" ┃  ") << tokyo_text("\n")
                          << tokyo_dim(" ┃  ") << tokyo_text("Usage: /model <tier>") << "\n\n";
            } else {
                std::string new_tier = trimmed.size() > 7 ? trimmed.substr(7) : "";
                model_info = router.route(new_tier, "model switch");
                std::cout << " " << term::bold(tokyo_blue("◆ Model Switched")) << "\n"
                          << tokyo_dim(" ┃  ") << tokyo_text("Provider: ") << tokyo_cyan(model_info.provider) << "\n"
                          << tokyo_dim(" ┃  ") << tokyo_text("Model:    ") << tokyo_cyan(model_info.model) << "\n\n";
            }
            return true;
        }
        if (trimmed == "/help") {
            std::cout << " " << term::bold(tokyo_blue("◆ Commands")) << "\n"
                      << tokyo_dim(" ┃  ") << tokyo_cyan(std::format("{:<15}", "/model <tier>")) << tokyo_text("Switch AI capability tier") << "\n"
                      << tokyo_dim(" ┃  ") << tokyo_cyan(std::format("{:<15}", "/agent <id>")) << tokyo_text("Switch personality/role") << "\n"
                      << tokyo_dim(" ┃  ") << tokyo_cyan(std::format("{:<15}", "/fleet")) << tokyo_text("List all available agents") << "\n"
                      << tokyo_dim(" ┃  ") << tokyo_cyan(std::format("{:<15}", "/history")) << tokyo_text("View current session history") << "\n"
                      << tokyo_dim(" ┃  ") << tokyo_cyan(std::format("{:<15}", "/clear")) << tokyo_text("Wipe session context") << "\n"
                      << tokyo_dim(" ┃  ") << tokyo_cyan(std::format("{:<15}", "/exit")) << tokyo_text("End session") << "\n\n";
            return true;
        }
        if (trimmed == "/history") {
            std::cout << " " << term::bold(tokyo_blue("◆ Session History")) << " " << tokyo_dim(std::to_string(history.size()) + " turns") << "\n";
            for (const auto& h : history) {
                std::string summary = h.first;
                if (summary.size() > 50) summary = summary.substr(0, 47) + "...";
                std::cout << tokyo_dim(" ┃  ") << tokyo_text(summary) << "\n";
            }
            std::cout << "\n";
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
                if (idx > 0 && idx <= agents.size()) { target = agents[idx - 1].id; found = true; }
            } catch (...) {}

            if (!found && registry.get_agent(target)) found = true;

            if (found) {
                active_agent = target;
                memory_ctx = session.get_memory_context(active_agent);
                std::cout << " " << term::bold(tokyo_blue("◆ Agent Switched")) << "\n"
                          << tokyo_dim(" ┃  ") << tokyo_text("Active: ") << tokyo_cyan(active_agent) << "\n\n";
                return true;
            } else {
                std::cout << " " << term::bold(tokyo_error("◆ Error")) << "\n"
                          << tokyo_dim(" ┃  ") << tokyo_text("Unknown agent: " + target + ". Use /fleet to see available.") << "\n\n";
                return true;
            }
        }
        return false;
    };

    if (!initial_msg.empty()) {
        if (!process_command(initial_msg)) {
            // Handled directly below in input loop for interactive
        }
    }

    while (running) {
        draw_prompt();
        if (is_interactive) {
            int c = term::read_key();
            if (c > 0) {
                if (c == 3 || c == 4) running = false;
                else if (c == 9 || c == 1003) { // Tab or Right Arrow
                    if (!ghost_text.empty()) current_input += ghost_text;
                }
                else if (c == 127 || c == 8 || c == 1000) { if (!current_input.empty()) current_input.pop_back(); }
                else if (c == '\r' || c == '\n') {
                    if (current_input.empty()) continue;
                    std::string user_msg = current_input;
                    current_input.clear();
                    
                    if (process_command(user_msg)) continue;
                    
                    std::cout << "\r\033[K" << " " << term::bold(tokyo_cyan("● You")) << "  " << tokyo_dim(active_agent) << "\n";
                    std::istringstream u_stream(user_msg);
                    std::string u_line;
                    while (std::getline(u_stream, u_line)) {
                        std::cout << tokyo_dim(" ┃  ") << tokyo_text(u_line) << "\n";
                    }
                    std::cout << "\n";

                    std::atomic<bool> is_thinking{true};
                    std::thread spinner_thread([&]() {
                        static const std::vector<std::string> frames = {"⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"};
                        static const std::vector<std::string> phrases = {
                            "exploring codebase...", "analyzing patterns...", "synthesizing response...", "doing research..."
                        };
                        std::string phrase = phrases[rand() % phrases.size()];
                        int i = 0;
                        while (is_thinking) {
                            std::cout << "\r\033[K  " << tokyo_magenta(frames[(i++) % frames.size()]) << " " << tokyo_dim(phrase);
                            std::cout.flush();
                            std::this_thread::sleep_for(std::chrono::milliseconds(80));
                        }
                    });

                    auto ai = registry.get_agent(active_agent);
                    std::string sys_p = (ai && !ai->prompt_path.empty()) ? ProviderExecutor::load_agent_prompt(ctx.euxis_home, ai->prompt_path) : "You are Euxis.";
                    
                    bool first_chunk = true;
                    bool need_prefix = false;
                    auto response = executor.execute(model_info, ProviderExecutor::build_prompt(sys_p, PiiFilter::redact(user_msg), memory_ctx), 120, std::nullopt, [&](const std::string& chunk){
                        if (first_chunk) {
                            is_thinking = false;
                            spinner_thread.join();
                            std::cout << "\r\033[K" << " " << term::bold(tokyo_magenta("◆ Euxis")) << "  " << tokyo_dim(model_info.model) << "\n";
                            std::cout << tokyo_magenta(" ┃  ");
                            first_chunk = false;
                        }
                        for (char ch : chunk) {
                            if (need_prefix) { std::cout << tokyo_magenta(" ┃  "); need_prefix = false; }
                            std::cout << tokyo_text(std::string(1, ch));
                            if (ch == '\n') need_prefix = true;
                        }
                        std::cout.flush();
                        std::this_thread::sleep_for(std::chrono::milliseconds(2)); // Cinematic streaming
                    });

                    if (first_chunk) {
                        is_thinking = false;
                        spinner_thread.join();
                        std::cout << "\r\033[K" << " " << term::bold(tokyo_magenta("◆ Euxis")) << "  " << tokyo_dim(model_info.model) << "\n";
                    }

                    if (response.success) {
                        history.push_back({user_msg, response.output});
                        memory_ctx += "\nUser: " + user_msg + "\nEuxis: " + response.output + "\n";
                        std::cout << "\n\n";
                    } else {
                        std::cout << tokyo_error(" ┃  Error: " + response.error) << "\n\n";
                    }

                } else if (c >= 32 && c <= 126) current_input += (char)c;
            } else std::this_thread::sleep_for(std::chrono::milliseconds(5));
        } else {
            std::string l; if (!std::getline(input, l)) break;
            auto res = executor.execute(model_info, l);
            if (res.success) std::cout << res.output << "\n";
        }
    }
    
    if (is_interactive) {
        std::cout << "\r\033[K\n";
        term::disable_raw_mode();
    }
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
