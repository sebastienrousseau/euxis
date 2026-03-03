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
    std::vector<std::string> cmd_suggestions = {
        "/help", "/clear", "/exit", "/agent", "/history", 
        "tell me a joke", "refactor code", "analyze project", "check security"
    };

    bool is_interactive = (&input == &std::cin);
    if (is_interactive) {
        term::enable_raw_mode();
        // Cinematic Header via standard stdout
        std::cout << "\n  " << term::bold(term::rgb_fg(139, 233, 253, "EUXIS ADE v0.0.6")) 
                  << term::dim("  │  Interactive Session  │  " + model_info.model) << "\n";
        std::cout << "  " << term::dim("Type /help for commands. Press Tab to autocomplete.") << "\n\n";
    }

    std::string current_input;
    std::string ghost_text;
    bool running = true;

    auto get_prediction = [&](const std::string& input_str) -> std::string {
        if (input_str.empty()) return "";
        for (const auto& s : cmd_suggestions) {
            if (s.size() > input_str.size() && s.substr(0, input_str.size()) == input_str) {
                return s.substr(input_str.size());
            }
        }
        return "";
    };

    auto draw_prompt = [&]() {
        if (!is_interactive) return;
        std::cout << "\r\033[K"; // Clear current line
        std::cout << term::bold(term::rgb_fg(245, 189, 230, "  ➜ ")) << current_input;
        ghost_text = get_prediction(current_input);
        if (!ghost_text.empty()) {
            std::cout << term::dim(ghost_text);
            std::cout << "\033[" << ghost_text.size() << "D"; // Move cursor back
        }
        std::cout.flush();
    };

    auto process_command = [&](const std::string& trimmed) {
        if (trimmed == "exit" || trimmed == "quit" || trimmed == "/exit" || trimmed == "/quit") {
            running = false; return true;
        }
        if (trimmed == "clear" || trimmed == "/clear") {
            memory_ctx.clear(); 
            std::cout << "  " << term::dim("History wiped.") << "\n\n";
            return true;
        }
        if (trimmed == "?" || trimmed == "/help") {
            std::cout << "  " << term::bold("COMMAND PALETTE") << "\n"
                      << "  " << term::rgb_fg(139, 233, 253, "/help") << "    Show this menu\n"
                      << "  " << term::rgb_fg(139, 233, 253, "/clear") << "   Clear memory context\n"
                      << "  " << term::rgb_fg(139, 233, 253, "/agent") << "   Switch active agent\n"
                      << "  " << term::rgb_fg(139, 233, 253, "/exit") << "    End session\n\n";
            return true;
        }
        if (trimmed.starts_with("@")) {
            auto space = trimmed.find(' ');
            std::string target = (space == std::string::npos) ? trimmed.substr(1) : trimmed.substr(1, space - 1);
            if (registry.get_agent(target)) {
                active_agent = target;
                memory_ctx = session.get_memory_context(active_agent);
                std::cout << "  " << term::dim("Switched agent to: ") << term::bold(term::rgb_fg(189, 147, 249, active_agent)) << "\n\n";
                if (space == std::string::npos) return true;
            } else {
                std::cout << "  " << term::rgb_fg(237, 135, 150, "Unknown agent: " + target) << "\n\n";
                return true;
            }
        }
        return false;
    };

    auto execute_turn = [&](const std::string& user_msg) {
        if (!is_interactive) return;
        
        // Print User Message
        std::cout << "\r\033[K";
        std::cout << "  " << term::bold(term::rgb_fg(139, 233, 253, "👤 You")) << "\n";
        std::cout << "  " << user_msg << "\n\n";
        
        // Print AI Header
        std::cout << "  " << term::bold(term::rgb_fg(189, 147, 249, "🤖 " + active_agent)) << " " << term::dim("via " + model_info.model) << "\n";
        
        std::atomic<bool> is_thinking{true};
        
        // Background thinking spinner
        std::thread spinner_thread([&]() {
            static const std::vector<std::string> frames = {"⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"};
            int i = 0;
            while (is_thinking) {
                std::cout << "\r\033[K  " << term::rgb_fg(245, 189, 230, frames[(i++) % frames.size()]) << " Analyzing...";
                std::cout.flush();
                std::this_thread::sleep_for(std::chrono::milliseconds(80));
            }
        });

        std::string sys_p = "You are Euxis. Be concise and elegant.";
        auto ai = registry.get_agent(active_agent);
        if (ai && !ai->prompt_path.empty()) sys_p = ProviderExecutor::load_agent_prompt(ctx.euxis_home, ai->prompt_path);
        
        bool first_chunk = true;
        auto response = executor.execute(model_info, ProviderExecutor::build_prompt(sys_p, PiiFilter::redact(user_msg), memory_ctx), 120, std::nullopt, [&](const std::string& chunk){
            if (first_chunk) {
                is_thinking = false;
                spinner_thread.join();
                std::cout << "\r\033[K  "; // Clear spinner line
                first_chunk = false;
            }
            
            // Format chunks with left padding
            for (char c : chunk) {
                if (c == '\n') std::cout << "\n  ";
                else std::cout << c;
            }
            std::cout.flush();
        });

        if (first_chunk) { // Edge case: immediate return (e.g. error)
            is_thinking = false;
            spinner_thread.join();
            std::cout << "\r\033[K";
        }

        if (response.success) {
            std::cout << "\n\n";
            session.save_memory(active_agent, user_msg, response.output);
            memory_ctx += "\nUser: " + user_msg + "\nEuxis: " + response.output + "\n";
        } else {
            std::cout << "  " << term::rgb_fg(237, 135, 150, "Error: " + response.error) << "\n\n";
        }
    };

    if (!initial_msg.empty()) {
        if (!process_command(initial_msg)) {
            execute_turn(initial_msg);
        }
    }

    while (running) {
        draw_prompt();
        if (is_interactive) {
            int c = term::read_key();
            if (c > 0) {
                if (c == 3 || c == 4) {
                    running = false;
                } else if (c == 9) { // Tab
                    if (!ghost_text.empty()) current_input += ghost_text;
                } else if (c == 127 || c == 8 || c == 1000) { // Backspace
                    if (!current_input.empty()) current_input.pop_back();
                } else if (c == 1001 || c == 1002 || c == 1003 || c == 1004) {
                    // Ignore arrows for now
                } else if (c == '\r' || c == '\n') { // Enter
                    if (current_input.empty()) continue;
                    std::string user_msg = current_input;
                    current_input.clear();
                    
                    std::cout << "\r\033[K"; // Clear prompt before processing
                    if (process_command(user_msg)) continue;
                    
                    execute_turn(user_msg);
                } else if (c >= 32 && c <= 126) {
                    current_input += static_cast<char>(c);
                }
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        } else {
            std::string l; if (!std::getline(input, l)) break;
            if (process_command(l)) continue;
            auto res = executor.execute(model_info, l);
            if (res.success) std::cout << res.output << "\n";
        }
    }

    if (is_interactive) {
        std::cout << "\n";
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
