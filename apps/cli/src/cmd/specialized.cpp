#include "euxis/cli/cmd/specialized.hpp"
#include "euxis/cli/cmd/fleet.hpp"
#include "euxis/cli/box.hpp"
#include "euxis/cli/config_loader.hpp"
#include "euxis/cli/i18n.hpp"
#include "euxis/cli/lsp_bridge.hpp"
#include "euxis/cli/pii_filter.hpp"
#include "euxis/cli/process.hpp"
#include "euxis/cli/provider_executor.hpp"
#include "euxis/cli/provider_router.hpp"
#include "euxis/cli/registry_client.hpp"
#include "euxis/cli/sarif.hpp"
#include "euxis/cli/session.hpp"
#include "euxis/cli/terminal.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <chrono>
#include <sstream>
#include <unistd.h>

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
auto scan_directory_listing(const fs::path& dir, int max_depth = 3) -> std::string {
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
        // Limit depth
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

    // Check for audio tools
    bool has_sox = Process::available("sox");
    bool has_ffmpeg = Process::available("ffmpeg");
    std::cout << "  sox:    " << (has_sox ? tr("available") : tr("not found")) << "\n"
              << "  ffmpeg: " << (has_ffmpeg ? tr("available") : tr("not found")) << "\n";

    // Determine model selection
    std::string tier = "reason";
    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "--tier" && i + 1 < args.size()) {
            tier = args[++i];
        }
    }

    std::cout << "  " << tr("Mode:") << "   " << term::cyan(tr("text fallback")) << " (" << tr("type 'exit' or 'quit' to stop") << ")\n\n";

    ProviderRouter router(ctx.data_dir);
    ProviderExecutor executor(ctx.data_dir);
    auto model = router.route(tier, "voice conversation");

    std::cout << "  " << tr("Model:") << "  " << model.model << "\n\n";

    // Conversation loop: read from provided input stream line by line
    std::string line;
    int turn = 0;
    while (true) {
        if (&input == &std::cin) {
            std::cout << term::bold(tr("you> "));
            std::cout.flush();
        }

        if (!std::getline(input, line)) {
            // EOF
            if (&input == &std::cin) std::cout << "\n";
            break;
        }

        // Trim whitespace
        auto trimmed = line;
        while (!trimmed.empty() && (trimmed.front() == ' ' || trimmed.front() == '\t' || trimmed.front() == '\r')) {
            trimmed.erase(trimmed.begin());
        }
        while (!trimmed.empty() && (trimmed.back() == ' ' || trimmed.back() == '\t' || trimmed.back() == '\r' || trimmed.back() == '\n')) {
            trimmed.pop_back();
        }

        if (trimmed.empty()) continue;

        // Exit commands
        if (trimmed == "exit" || trimmed == "quit" || trimmed == "/exit" || trimmed == "/quit") {
            std::cout << "\n" << term::icon_ok() << " " << tr("Voice session ended.") << "\n";
            break;
        }

        // PII filter the input before sending to provider
        auto safe_input = PiiFilter::redact(trimmed);

        auto prompt = ProviderExecutor::build_prompt(
            "You are Euxis, a helpful AI assistant in a voice-like text conversation. "
            "Keep responses concise and conversational.",
            safe_input);

        ++turn;
        int frame = 0;
        if (&input == &std::cin) {
            term::spinner_frame(frame++, tr("Thinking..."));
        }
        auto response = executor.execute(model, prompt);
        if (&input == &std::cin) {
            term::spinner_clear();
        }

        if (response.success) {
            std::cout << term::cyan(tr("euxis> ")) << response.output << "\n\n";
        } else {
            std::cerr << term::icon_fail() << " " << tr("Error:") << " " << response.error << "\n\n";
        }
    }

    std::cout << turn << " " << tr("turn(s) completed.") << "\n";
    return 0;
}

// --- tui ---

int cmd_tui(Context& ctx, const std::vector<std::string>& args) {
    return cmd_tui_ex(ctx, args, std::cin);
}

int cmd_tui_ex(Context& ctx, const std::vector<std::string>& args, std::istream& input) {
    // TUI is now the dedicated Terminal User Interface (interactive REPL).
    
    // Check if we are in a non-interactive environment (only check STDIN if it's the real cin)
    if (&input == &std::cin && (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO))) {
        std::cout << term::icon_info() << " " << tr("Non-interactive mode detected. TUI requires a terminal.") << "\n";
        return 1;
    }

    term::print_banner();
    
    RegistryClient registry(ctx.data_dir);
    ProviderRouter router(ctx.data_dir);
    
    auto agents = registry.list_agents();
    auto squads = registry.list_squads();
    
    // --- System Status Box ---
    std::ostringstream sys_body;
    sys_body << term::bold("Workspace: ") << term::cyan(ctx.euxis_home) << "\n"
             << term::bold("Data Dir:  ") << term::cyan(ctx.data_dir) << "\n\n"
             << term::bold("Active Provider: ") << term::rgb_fg(255, 215, 0, router.detect_provider()) << "\n";
             
    auto providers = router.available_providers();
    sys_body << term::bold("Available:       ");
    for (size_t i = 0; i < providers.size(); ++i) {
        if (i > 0) sys_body << ", ";
        sys_body << term::dim(providers[i]);
    }
    
    term::print_box("EUXIS TERMINAL UI (v0.1.2)", sys_body.str());
    std::cout << "\n";

    // Determine model selection & initial message
    std::string tier = "code";
    std::string initial_msg;
    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "--tier" && i + 1 < args.size()) {
            tier = args[++i];
        } else if (args[i].front() != '-') {
            if (!initial_msg.empty()) initial_msg += " ";
            initial_msg += args[i];
        }
    }

    ProviderExecutor executor(ctx.data_dir);
    auto model = router.route(tier, initial_msg.empty() ? "tui conversation" : initial_msg);

    std::cout << term::dim("Connected to ") << term::bold(term::cyan(model.model)) 
              << term::dim(" via ") << term::bold(model.provider) << "\n"
              << term::dim("Commands: 'exit' to quit, 'clear' to reset context, 'history' to show log") << "\n\n";

    // Interactive Chat Loop
    std::string line;
    std::string memory_ctx;
    std::vector<std::pair<std::string, std::string>> history;
    std::string active_agent = "code-agent";
    
    bool processed_initial = false;

    while (true) {
        std::string trimmed;

        if (!initial_msg.empty() && !processed_initial) {
            trimmed = initial_msg;
            processed_initial = true;
            std::cout << term::rgb_fg(100, 255, 100, "╭─ Initial Message\n╰─> ") << trimmed << "\n";
        } else {
            if (&input == &std::cin) {
                std::cout << term::rgb_fg(100, 255, 100, "╭─ " + active_agent + " (You)\n╰─> ");
                std::cout.flush();
            }

            if (!std::getline(input, line)) {
                if (&input == &std::cin) std::cout << "\n";
                break;
            }

            trimmed = line;
            // Trim whitespace
            while (!trimmed.empty() && (trimmed.front() == ' ' || trimmed.front() == '\t' || trimmed.front() == '\r')) {
                trimmed.erase(trimmed.begin());
            }
            while (!trimmed.empty() && (trimmed.back() == ' ' || trimmed.back() == '\t' || trimmed.back() == '\r' || trimmed.back() == '\n')) {
                trimmed.pop_back();
            }
        }

        if (trimmed.empty()) continue;

        // Commands
        if (trimmed == "exit" || trimmed == "quit" || trimmed == "/exit" || trimmed == "/quit") {
            std::cout << "\n" << term::icon_ok() << " " << term::dim("Session ended.") << "\n";
            break;
        }
        if (trimmed == "clear") {
            memory_ctx.clear();
            history.clear();
            std::cout << "\n" << term::icon_ok() << " " << term::dim("Context memory cleared.") << "\n\n";
            continue;
        }
        if (trimmed == "history") {
            std::cout << "\n" << term::bold("--- Session History ---") << "\n";
            for (size_t i = 0; i < history.size(); ++i) {
                std::cout << term::dim("Turn " + std::to_string(i+1) + ":\n")
                          << term::green("  User:  ") << history[i].first << "\n"
                          << term::cyan("  Euxis: ") << (history[i].second.size() > 100 ? 
                                history[i].second.substr(0, 97) + "..." : history[i].second) << "\n\n";
            }
            std::cout << term::bold("-----------------------") << "\n\n";
            continue;
        }

        // Check for agent switch @agent
        if (trimmed.starts_with("@")) {
            auto space = trimmed.find(' ');
            std::string target = (space == std::string::npos) ? trimmed.substr(1) : trimmed.substr(1, space - 1);
            
            auto agent_ptr = registry.get_agent(target);
            if (agent_ptr) {
                active_agent = target;
                if (space == std::string::npos) {
                    std::cout << term::icon_ok() << " " << term::dim("Switched to ") << term::bold(target) << "\n\n";
                    continue;
                }
                trimmed = trimmed.substr(space + 1);
                while (!trimmed.empty() && (trimmed.front() == ' ' || trimmed.front() == '\t')) trimmed.erase(trimmed.begin());
            } else {
                std::cout << term::icon_warn() << " " << term::dim("Unknown agent: ") << target << "\n\n";
                if (space == std::string::npos) continue;
            }
        }

        // PII filter the input
        auto safe_input = PiiFilter::redact(trimmed);

        // --- Autonomous Task Decomposition (Swarm Interception) ---
        // If the request is complex (review, research, audit), switch to Swarm mode
        std::string lower_input = safe_input;
        std::transform(lower_input.begin(), lower_input.end(), lower_input.begin(), ::tolower);
        
        bool is_complex = lower_input.find("review") != std::string::npos || 
                          lower_input.find("research") != std::string::npos ||
                          lower_input.find("audit") != std::string::npos;

        if (is_complex && active_agent == "code-agent") {
            std::cout << term::dim("  \xe2\xa0\x8b Complex task detected. Activating Euxis Swarm Orchestrator...");
            std::cout.flush();
            
            // Use the base name; cmd_playbook will resolve it via data/config/playbooks/
            std::vector<std::string> swarm_args = {"verify-everything", safe_input};
            std::cout << "\r\033[K"; // Clear line
            
            int swarm_res = cmd_playbook(ctx, swarm_args);
            if (swarm_res == 0) {
                std::cout << term::icon_ok() << " " << term::dim("Swarm analysis complete. Results saved to session memory.") << "\n\n";
                continue;
            } else {
                std::cout << term::icon_fail() << " " << term::dim("Swarm orchestration failed. Falling back to monolithic reasoning.") << "\n\n";
            }
        }

        // Load agent system prompt
        std::string system_prompt;
        auto agent_info = registry.get_agent(active_agent);
        if (agent_info && !agent_info->prompt_path.empty()) {
            system_prompt = ProviderExecutor::load_agent_prompt(ctx.euxis_home, agent_info->prompt_path);
        }
        
        if (system_prompt.empty()) {
            system_prompt = "You are Euxis, an advanced AI engineering assistant. Be precise, technical, and use markdown.";
        }
            
        std::string full_prompt = ProviderExecutor::build_prompt(system_prompt, safe_input, memory_ctx);

        if (&input == &std::cin) {
            std::cout << term::dim("  \xe2\xa0\x8b Thinking...");
            std::cout.flush();
        }
        
        auto start = std::chrono::steady_clock::now();
        auto response = executor.execute(model, full_prompt);
        auto end = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        // Clear the thinking line
        if (&input == &std::cin) {
            std::cout << "\r\033[K"; 
        }

        if (response.success) {
            std::cout << term::rgb_fg(100, 200, 255, "╭─ " + active_agent + " ") 
                      << term::dim(std::format("({}ms | {})", duration, model.model)) << "\n";
            
            if (response.output.empty()) {
                std::cout << term::rgb_fg(100, 200, 255, "│ ") << term::dim("<empty response>") << "\n";
            } else {
                std::istringstream stream{response.output};
                std::string out_line;
                while (std::getline(stream, out_line)) {
                    std::cout << term::rgb_fg(100, 200, 255, "│ ") << out_line << "\n";
                }
            }
            std::cout << term::rgb_fg(100, 200, 255, "╰─\n\n");
            
            // Add turn to context memory & history
            history.push_back({trimmed, response.output});
            memory_ctx += "\nUser: " + safe_input + "\nEuxis: " + response.output + "\n";
            if (memory_ctx.size() > 12000) {
                memory_ctx = memory_ctx.substr(memory_ctx.size() - 12000);
            }
        } else {
            std::cerr << term::rgb_fg(255, 100, 100, "╭─ Error\n│ ") << (response.error.empty() ? "Unknown execution failure" : response.error) << "\n"
                      << term::rgb_fg(255, 100, 100, "│ Exit Code: ") << response.exit_code << "\n"
                      << term::rgb_fg(255, 100, 100, "╰─\n\n");
        }
    }

    return 0;
}

// --- gui ---

int cmd_gui(Context& ctx, const std::vector<std::string>& args) {
    // Try to launch the full Qt GUI (euxis-etx)
    // Search order: build dir, installed binary, PATH
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

    std::cerr << term::icon_fail() << " " << tr("ETX GUI binary not found.") << "\n"
              << term::dim(tr("Please build 'euxis-etx' or ensure it is in your PATH.")) << "\n";
    return 1;
}

// --- polish ---

int cmd_polish(Context& ctx, const std::vector<std::string>& args) {
    if (args.empty()) {
        std::cerr << tr("Usage: euxis polish <file> [--style formal|casual] [--dry-run]") << "\n";
        return 2;
    }

    const auto& file_path = args[0];
    std::string style = "formal";
    bool dry_run = false;
    for (size_t i = 1; i < args.size(); ++i) {
        if (args[i] == "--style" && i + 1 < args.size()) {
            style = args[++i];
        } else if (args[i] == "--dry-run") {
            dry_run = true;
        }
    }

    if (!fs::exists(file_path)) {
        std::cerr << tr("File not found:") << " " << file_path << "\n";
        return 1;
    }

    std::ifstream f(file_path);
    std::string content((std::istreambuf_iterator<char>(f)),
                         std::istreambuf_iterator<char>());

    std::cout << term::bold(tr("Polish:")) << " " << file_path << "\n"
              << "  " << tr("Style:") << "    " << style << "\n"
              << "  " << tr("Size:") << "     " << content.size() << " " << tr("bytes") << "\n"
              << "  " << tr("Dry-run:") << "  " << (dry_run ? tr("yes") : tr("no")) << "\n";

    // PII check
    auto redacted = PiiFilter::redact(content);
    if (redacted != content) {
        std::cout << "  " << term::icon_warn() << " " << tr("PII detected and redacted before sending to provider") << "\n";
        content = redacted;
    }

    // Build the polishing prompt
    std::string system_prompt =
        "You are a professional editor. Polish and reformat the following text. "
        "Style: " + style + ". "
        "Preserve the original meaning and structure. "
        "If the content is code, improve comments, naming, and formatting. "
        "If the content is prose, improve clarity, grammar, and flow. "
        "Return ONLY the polished content, no explanations or commentary.";

    auto prompt = ProviderExecutor::build_prompt(system_prompt, "Polish this content:", content);

    ProviderRouter router(ctx.data_dir);
    ProviderExecutor executor(ctx.data_dir);
    auto model = router.route("reason", "polish text");

    int frame = 0;
    term::spinner_frame(frame++, tr("Polishing..."));
    auto response = executor.execute(model, prompt);
    term::spinner_clear();

    if (!response.success) {
        std::cerr << term::icon_fail() << " " << tr("Polish failed:") << " " << response.error << "\n";
        return 1;
    }

    std::cout << "  " << tr("Output:") << "   " << response.output.size() << " " << tr("bytes") << " ("
              << response.duration_ms << "ms)\n";

    if (dry_run) {
        // Print to stdout only
        std::cout << "\n" << term::bold(tr("--- Polished output ---")) << "\n";
        std::cout << response.output << "\n";
        std::cout << term::bold(tr("--- End ---")) << "\n";
    } else {
        // Write back to the file
        std::ofstream out(file_path);
        out << response.output;
        std::cout << "  " << term::icon_ok() << " " << tr("Written back to:") << " " << file_path << "\n";
    }

    std::cout << term::icon_ok() << " " << tr("Polish complete") << "\n";
    return 0;
}

// --- kaizen ---

int cmd_kaizen(Context& ctx, const std::vector<std::string>& args) {
    std::string focus;
    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "--focus" && i + 1 < args.size()) {
            focus = args[++i];
        }
    }

    std::cout << term::bold(tr("Kaizen - Continuous Improvement")) << "\n\n";

    RegistryClient registry(ctx.data_dir);
    auto agents = registry.list_agents();
    auto squads = registry.list_squads();

    // Build fleet context for the provider
    std::ostringstream fleet_context;
    fleet_context << "## Agent Fleet Overview\n\n"
                  << "Total agents: " << agents.size() << "\n"
                  << "Total squads: " << squads.size() << "\n\n";

    // Collect agent quality metrics
    int total = static_cast<int>(agents.size());
    int with_version = 0;
    int with_tags = 0;
    int with_role = 0;
    int with_prompt = 0;

    fleet_context << "### Agents\n\n";
    for (const auto& a : agents) {
        if (!a.version.empty()) ++with_version;
        if (!a.tags.empty()) ++with_tags;
        if (!a.role.empty()) ++with_role;
        if (!a.prompt_path.empty()) ++with_prompt;

        fleet_context << "- **" << a.id << "**: role=" << (a.role.empty() ? "(none)" : a.role)
                      << ", tier=" << (a.tier.empty() ? "(none)" : a.tier)
                      << ", version=" << (a.version.empty() ? "(none)" : a.version)
                      << ", tags=" << a.tags.size()
                      << ", capabilities=" << a.capabilities.size()
                      << ", prompt=" << (a.prompt_path.empty() ? "missing" : "present")
                      << "\n";
    }

    fleet_context << "\n### Squads\n\n";
    for (const auto& s : squads) {
        fleet_context << "- **" << s.id << "** (" << s.name << "): "
                      << s.purpose << " [" << s.members.size() << " members, lead=" << s.lead << "]\n";
    }

    fleet_context << "\n### Quality Metrics\n\n"
                  << "- With version: " << with_version << "/" << total << "\n"
                  << "- With tags: " << with_tags << "/" << total << "\n"
                  << "- With role: " << with_role << "/" << total << "\n"
                  << "- With prompt: " << with_prompt << "/" << total << "\n";

    // Print local summary first
    std::cout << "  " << tr("Agents:") << "        " << total << "\n"
              << "  " << tr("Squads:") << "        " << squads.size() << "\n"
              << "  " << tr("With version:") << "  " << with_version << "/" << total << "\n"
              << "  " << tr("With tags:") << "     " << with_tags << "/" << total << "\n"
              << "  " << tr("With role:") << "     " << with_role << "/" << total << "\n"
              << "  " << tr("With prompt:") << "   " << with_prompt << "/" << total << "\n\n";

    // Build the kaizen prompt
    std::string system_prompt =
        "You are a Kaizen (continuous improvement) consultant analyzing an AI agent fleet. "
        "Analyze the fleet data below and provide actionable improvement suggestions. "
        "Focus on: agent completeness, squad composition, tier distribution, capability gaps, "
        "and operational efficiency. Format your response as a numbered list of suggestions "
        "with priority (P0/P1/P2) and expected impact.";

    std::string task = "Analyze this agent fleet and suggest improvements.";
    if (!focus.empty()) {
        task += " Focus area: " + focus;
    }

    auto prompt = ProviderExecutor::build_prompt(system_prompt, task, fleet_context.str());

    ProviderRouter router(ctx.data_dir);
    ProviderExecutor executor(ctx.data_dir);
    auto model = router.route("reason", "kaizen analysis");

    int frame = 0;
    term::spinner_frame(frame++, tr("Analyzing fleet..."));
    auto response = executor.execute(model, prompt);
    term::spinner_clear();

    if (!response.success) {
        std::cerr << term::icon_fail() << " " << tr("Kaizen analysis failed:") << " " << response.error << "\n";

        // Fall back to local heuristic suggestions
        std::cout << term::bold(tr("Local Suggestions (provider unavailable):")) << "\n";
        if (with_version < total) {
            std::cout << "  " << term::icon_warn() << " [P1] " << (total - with_version)
                      << " " << tr("agent(s) missing version") << "\n";
        }
        if (with_tags < total) {
            std::cout << "  " << term::icon_warn() << " [P1] " << (total - with_tags)
                      << " " << tr("agent(s) missing tags") << "\n";
        }
        if (with_role < total) {
            std::cout << "  " << term::icon_warn() << " [P0] " << (total - with_role)
                      << " " << tr("agent(s) missing role definition") << "\n";
        }
        if (with_prompt < total) {
            std::cout << "  " << term::icon_warn() << " [P0] " << (total - with_prompt)
                      << " " << tr("agent(s) missing prompt file") << "\n";
        }
        if (with_version == total && with_tags == total &&
            with_role == total && with_prompt == total) {
            std::cout << "  " << term::icon_ok() << " " << tr("All agents meet quality baseline") << "\n";
        }
        return 0;
    }

    // Display the AI-generated suggestions
    std::cout << term::bold(tr("Kaizen Suggestions:")) << "\n\n";
    std::cout << response.output << "\n\n";
    std::cout << "  (" << response.duration_ms << "ms)\n";

    // Save analysis to session
    Session session(ctx.euxis_home);
    auto project_dir = session.ensure_project_dirs("kaizen");
    auto output_path = fs::path(project_dir) / "output" / "kaizen-analysis.md";
    write_output_file(output_path, response.output);
    std::cout << "  " << tr("Saved to:") << " " << output_path.string() << "\n";

    std::cout << term::icon_ok() << " " << tr("Kaizen analysis complete") << "\n";
    return 0;
}

// --- audit ---

int cmd_audit(Context& ctx, const std::vector<std::string>& args) {
    (void)args;

    std::cout << term::bold(tr("Security & Compliance Audit")) << "\n\n";

    int issues = 0;

    // 1. Check for sensitive files
    std::cout << "  " << term::bold(tr("Sensitive file scan:")) << "\n";
    for (const auto& pattern : {".env", "credentials.json", "secrets.yaml", ".key"}) {
        auto path = fs::path(ctx.euxis_home) / pattern;
        if (fs::exists(path)) {
            std::cout << "    " << term::icon_warn() << " " << tr("Found:") << " " << pattern << "\n";
            ++issues;
        }
    }
    if (issues == 0) {
        std::cout << "    " << term::icon_ok() << " " << tr("No sensitive files in EUXIS_HOME root") << "\n";
    }

    // 2. Check file permissions
    std::cout << "  " << term::bold(tr("Permissions:")) << "\n";
    auto config_dir = fs::path(ctx.data_dir) / "config";
    if (fs::is_directory(config_dir)) {
        auto perms = fs::status(config_dir).permissions();
        bool world_readable = (perms & fs::perms::others_read) != fs::perms::none;
        if (world_readable) {
            std::cout << "    " << term::icon_warn() << " " << tr("config/ is world-readable") << "\n";
            ++issues;
        } else {
            std::cout << "    " << term::icon_ok() << " " << tr("config/ permissions OK") << "\n";
        }
    }

    // 3. PII filter status
    std::cout << "  " << term::bold(tr("PII protection:")) << "\n";
    std::cout << "    " << (PiiFilter::enabled() ? term::icon_ok() : term::icon_warn())
              << " " << tr("Log sanitization:") << " " << (PiiFilter::enabled() ? tr("enabled") : tr("disabled")) << "\n";

    std::cout << "\n" << (issues == 0 ? term::green(tr("Audit passed")) :
                          term::yellow(std::to_string(issues) + " " + tr("finding(s)"))) << "\n";
    return 0;
}

// --- audit-run ---

int cmd_audit_run(Context& ctx, const std::vector<std::string>& args) {
    std::string run_id = "audit-" + std::to_string(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());

    std::cout << term::bold(tr("Audit Run:")) << " " << run_id << "\n\n";

    auto evidence_dir = fs::path(ctx.euxis_home) / "data" / "audit" / run_id;
    fs::create_directories(evidence_dir);

    // Collect evidence
    std::cout << "  " << tr("Collecting evidence...") << "\n";

    // Save system info
    {
        auto result = Process::run("uname", {"-a"});
        std::ofstream f(evidence_dir / "system-info.txt");
        f << result.stdout_output;
    }

    // Save registry snapshot
    {
        RegistryClient registry(ctx.data_dir);
        auto agents = registry.list_agents();
        nlohmann::json j = nlohmann::json::array();
        for (const auto& a : agents) {
            j.push_back({{"id", a.id}, {"tier", a.tier}, {"version", a.version}});
        }
        std::ofstream f(evidence_dir / "registry-snapshot.json");
        f << j.dump(2);
    }

    // Save provider status
    {
        ProviderRouter router(ctx.data_dir);
        nlohmann::json j;
        j["provider"] = router.detect_provider();
        j["providers"] = router.available_providers();
        j["local"] = router.local_available();
        std::ofstream f(evidence_dir / "provider-status.json");
        f << j.dump(2);
    }

    std::cout << "  " << term::icon_ok() << " " << tr("Evidence saved to:") << " " << evidence_dir.string() << "\n";

    if (!args.empty() && args[0] == "--full") {
        // Run full audit
        std::cout << "\n  " << tr("Running full audit checks...") << "\n";
        cmd_audit(ctx, {});
    }

    return 0;
}

// --- certify ---

int cmd_certify(Context& ctx, const std::vector<std::string>& args) {
    if (args.empty()) {
        std::cerr << tr("Usage: euxis certify <agent-id>") << "\n";
        return 2;
    }

    const std::string& agent_id = args[0];
    RegistryClient registry(ctx.data_dir);
    auto agent = registry.get_agent(agent_id);

    if (!agent) {
        std::cerr << tr("Agent not found:") << " " << agent_id << "\n";
        return 1;
    }

    std::cout << term::bold(tr("Certify:")) << " " << agent_id << "\n\n";
    int score = 0;
    int max_score = 5;

    // 1. Has version
    bool has_version = !agent->version.empty();
    std::cout << "  " << (has_version ? term::icon_ok() : term::icon_fail())
              << " " << tr("Version defined") << "\n";
    if (has_version) ++score;

    // 2. Has tags
    bool has_tags = !agent->tags.empty();
    std::cout << "  " << (has_tags ? term::icon_ok() : term::icon_fail())
              << " " << tr("Tags defined") << "\n";
    if (has_tags) ++score;

    // 3. Has role
    bool has_role = !agent->role.empty();
    std::cout << "  " << (has_role ? term::icon_ok() : term::icon_fail())
              << " " << tr("Role defined") << "\n";
    if (has_role) ++score;

    // 4. Prompt file exists
    bool has_prompt = false;
    if (!agent->prompt_path.empty()) {
        has_prompt = fs::exists(fs::path(ctx.euxis_home) / agent->prompt_path);
    }
    std::cout << "  " << (has_prompt ? term::icon_ok() : term::icon_fail())
              << " " << tr("Prompt file exists") << "\n";
    if (has_prompt) ++score;

    // 5. Has tier
    bool has_tier = !agent->tier.empty();
    std::cout << "  " << (has_tier ? term::icon_ok() : term::icon_fail())
              << " " << tr("Tier assigned") << "\n";
    if (has_tier) ++score;

    std::cout << "\n  " << tr("Score:") << " " << score << "/" << max_score << "\n";
    if (score == max_score) {
        std::cout << "  " << term::green(tr("CERTIFIED")) << "\n";
    } else {
        std::cout << "  " << term::yellow(tr("NOT CERTIFIED — fix issues above")) << "\n";
    }

    return score == max_score ? 0 : 1;
}

// --- evidence-verify ---

int cmd_evidence_verify(Context& ctx, const std::vector<std::string>& args) {
    auto audit_dir = fs::path(ctx.euxis_home) / "data" / "audit";

    if (args.empty() || args[0] == "list") {
        std::cout << term::bold(tr("Audit Evidence")) << "\n\n";
        if (!fs::is_directory(audit_dir)) {
            std::cout << "  " << tr("No audit evidence found") << "\n";
            return 0;
        }
        for (const auto& entry : fs::directory_iterator(audit_dir)) {
            if (entry.is_directory()) {
                int file_count = 0;
                for (const auto& f : fs::directory_iterator(entry.path())) {
                    if (f.is_regular_file()) ++file_count;
                    (void)f;
                }
                std::cout << "  " << entry.path().filename().string()
                          << " (" << file_count << " " << tr("files") << ")\n";
            }
        }
        return 0;
    }

    if (args[0] == "verify" && args.size() >= 2) {
        auto run_dir = audit_dir / args[1];
        if (!fs::is_directory(run_dir)) {
            std::cerr << tr("Audit run not found:") << " " << args[1] << "\n";
            return 1;
        }

        std::cout << term::bold(tr("Verifying:")) << " " << args[1] << "\n";
        int files = 0;
        for (const auto& entry : fs::directory_iterator(run_dir)) {
            if (entry.is_regular_file()) {
                ++files;
                std::cout << "  " << term::icon_ok() << " " << entry.path().filename().string()
                          << " (" << entry.file_size() << " " << tr("bytes") << ")\n";
            }
        }
        std::cout << "\n" << files << " " << tr("evidence file(s) verified") << "\n";
        return 0;
    }

    std::cerr << tr("Usage: euxis evidence-verify <list|verify> [run-id]") << "\n";
    return 2;
}

// --- gym ---

int cmd_gym(Context& ctx, const std::vector<std::string>& args) {
    if (args.empty()) {
        std::cerr << tr("Usage: euxis gym <agent-id> [--drills N]") << "\n";
        return 2;
    }

    std::string agent_id = args[0];
    int drills = 3;
    for (size_t i = 1; i < args.size(); ++i) {
        if (args[i] == "--drills" && i + 1 < args.size()) {
            drills = std::stoi(args[++i]);
        }
    }

    RegistryClient registry(ctx.data_dir);
    auto agent = registry.get_agent(agent_id);
    if (!agent) {
        std::cerr << tr("Agent not found:") << " " << agent_id << "\n";
        return 1;
    }

    std::cout << term::bold(tr("Agent Gym:")) << " " << agent_id << "\n"
              << "  " << tr("Tier:") << "   " << agent->tier << "\n"
              << "  " << tr("Role:") << "   " << agent->role << "\n"
              << "  " << tr("Drills:") << " " << drills << "\n\n";

    // Load agent system prompt for context
    std::string agent_system_prompt;
    if (!agent->prompt_path.empty()) {
        agent_system_prompt = ProviderExecutor::load_agent_prompt(ctx.euxis_home, agent->prompt_path);
    }

    // Generate tier-appropriate drill prompts
    struct Drill {
        std::string name;
        std::string prompt;
        int min_length;
    };

    std::vector<Drill> drill_bank;

    if (agent->tier == "reason" || agent->tier == "code") {
        drill_bank = {
            {"Analysis",       "Analyze the trade-offs between WASM sandboxing and container isolation for multi-agent systems. Provide a structured comparison.", 200},
            {"Problem Solve",  "Given an agent fleet with 40+ agents, describe a strategy to minimize LLM costs while maintaining quality. Include tier-based routing.", 150},
            {"Architecture",   "Design a fault-tolerant message passing system for agent-to-agent communication. Cover at least 3 failure modes.", 200},
            {"Code Review",    "Review this pseudocode for security issues: `exec(user_input); db.query('SELECT * FROM agents WHERE id=' + agent_id)`", 100},
            {"Edge Cases",     "List 5 edge cases that could break an LLM provider fallback chain, and how to handle each.", 150},
        };
    } else if (agent->tier == "data") {
        drill_bank = {
            {"Data Quality",   "Describe 3 methods to validate agent registry data integrity, including handling of missing fields.", 100},
            {"Summarization",  "Summarize the key differences between MCP, A2A, and ACP protocols in 3 bullet points each.", 120},
            {"Extraction",     "From the text 'Agent alpha-1 v2.3 (tier:code, tags:[security,audit]) deployed on 2026-01-15', extract all structured fields as JSON.", 80},
            {"Classification", "Classify these agent tasks by tier: code review, log summarization, strategic planning, data parsing, security audit.", 80},
            {"Aggregation",    "Given 10 agents with varying response times (50ms-5000ms), describe how to compute meaningful fleet health metrics.", 100},
        };
    } else {
        // routine tier or unknown
        drill_bank = {
            {"Format",         "Reformat this list into a markdown table: 'agent-1: code tier, agent-2: reason tier, agent-3: data tier'", 50},
            {"Template",       "Generate a JSON manifest template for registering a new agent with fields: id, role, tier, version, tags.", 60},
            {"Status Check",   "Write a one-paragraph status report for an agent fleet with 42 agents, 8 squads, and 3 providers.", 50},
            {"Instruction",    "Write clear step-by-step instructions for adding a new agent to the Euxis registry.", 80},
            {"Validation",     "List the 5 most important fields to validate when registering a new AI agent.", 50},
        };
    }

    // Limit to the requested number of drills
    int actual_drills = std::min(drills, static_cast<int>(drill_bank.size()));

    ProviderRouter router(ctx.data_dir);
    ProviderExecutor executor(ctx.data_dir);
    Session session(ctx.euxis_home);
    auto model = router.route(agent->tier, "training drill");

    int total_score = 0;
    int max_score = actual_drills * 3; // 3 points max per drill

    for (int i = 0; i < actual_drills; ++i) {
        const auto& drill = drill_bank[static_cast<size_t>(i)];

        std::cout << "  " << tr("Drill") << " " << (i + 1) << "/" << actual_drills
                  << ": " << term::cyan(drill.name) << "\n";

        // Build drill prompt using the agent's own system prompt for role context
        std::string drill_system =
            "You are being evaluated on a training drill. Respond thoroughly and accurately. "
            "Your role context: " + (agent_system_prompt.empty() ?
                ("Agent '" + agent_id + "' with role '" + agent->role + "'") :
                agent_system_prompt);

        auto combined_prompt = ProviderExecutor::build_prompt(drill_system, drill.prompt);

        int frame = 0;
        term::spinner_frame(frame++, tr("Running drill") + " " + std::to_string(i + 1) + "...");
        auto response = executor.execute(model, combined_prompt);
        term::spinner_clear();

        if (!response.success) {
            std::cout << "    " << term::icon_fail() << " " << tr("Failed:") << " " << response.error << "\n";
            continue;
        }

        // Evaluate the response
        int drill_score = 0;

        // Criterion 1: Minimum length (response is substantive)
        bool length_ok = static_cast<int>(response.output.size()) >= drill.min_length;
        if (length_ok) ++drill_score;

        // Criterion 2: Quality markers (structured thinking)
        int quality_markers = 0;
        // Check for structured content indicators
        if (response.output.find('\n') != std::string::npos) ++quality_markers;
        if (response.output.find("- ") != std::string::npos ||
            response.output.find("1.") != std::string::npos ||
            response.output.find("* ") != std::string::npos) ++quality_markers;
        if (response.output.find("##") != std::string::npos ||
            response.output.find("**") != std::string::npos) ++quality_markers;
        bool quality_ok = quality_markers >= 2;
        if (quality_ok) ++drill_score;

        // Criterion 3: No obvious failure patterns
        bool no_refusal = response.output.find("I cannot") == std::string::npos &&
                          response.output.find("I'm unable") == std::string::npos &&
                          response.output.find("As an AI") == std::string::npos;
        if (no_refusal) ++drill_score;

        total_score += drill_score;

        std::string score_str = std::to_string(drill_score) + "/3";
        auto score_color = drill_score == 3 ? term::green(score_str) :
                           drill_score >= 2 ? term::yellow(score_str) :
                           term::red(score_str);

        std::cout << "    " << (drill_score >= 2 ? term::icon_ok() : term::icon_warn())
                  << " " << tr("Score:") << " " << score_color
                  << " | " << response.output.size() << " " << tr("chars")
                  << " | " << response.duration_ms << "ms"
                  << " | len:" << (length_ok ? tr("pass") : tr("FAIL"))
                  << " qual:" << (quality_ok ? tr("pass") : tr("FAIL"))
                  << " tone:" << (no_refusal ? tr("pass") : tr("FAIL"))
                  << "\n";
    }

    // Summary
    std::cout << "\n" << term::bold(tr("Results:")) << "\n"
              << "  " << tr("Total Score:") << " " << total_score << "/" << max_score << "\n";

    double pct = max_score > 0 ? (100.0 * total_score / max_score) : 0.0;
    if (pct >= 80.0) {
        std::cout << "  " << tr("Grade:") << " " << term::green(tr("EXCELLENT")) << "\n";
    } else if (pct >= 60.0) {
        std::cout << "  " << tr("Grade:") << " " << term::yellow(tr("GOOD")) << "\n";
    } else {
        std::cout << "  " << tr("Grade:") << " " << term::red(tr("NEEDS IMPROVEMENT")) << "\n";
    }

    // Save results
    auto project_dir = session.ensure_project_dirs(agent_id);
    auto output_path = fs::path(project_dir) / "output" / "gym-results.json";
    nlohmann::json results;
    results["agent_id"] = agent_id;
    results["tier"] = agent->tier;
    results["drills"] = actual_drills;
    results["score"] = total_score;
    results["max_score"] = max_score;
    results["percentage"] = pct;
    write_output_file(output_path, results.dump(2));

    std::cout << "  " << tr("Results saved to:") << " " << output_path.string() << "\n";
    std::cout << term::icon_ok() << " " << tr("Training complete") << "\n";
    return 0;
}

// --- replay ---

int cmd_replay(Context& ctx, const std::vector<std::string>& args) {
    if (args.empty()) {
        std::cerr << tr("Usage: euxis replay <session-log>") << "\n";
        return 2;
    }

    auto log_path = args[0];
    if (!fs::exists(log_path)) {
        // Try relative to runtime
        auto alt = fs::path(ctx.euxis_home) / "data" / "runtime" / log_path;
        if (fs::exists(alt)) {
            log_path = alt.string();
        } else {
            std::cerr << tr("Log not found:") << " " << log_path << "\n";
            return 1;
        }
    }

    std::cout << term::bold(tr("Session Replay")) << "\n"
              << "  " << tr("Log:") << " " << log_path << "\n\n";

    std::ifstream f(log_path);
    std::string line;
    int entries = 0;
    while (std::getline(f, line)) {
        if (line.empty()) continue;
        ++entries;
        // Try to parse as JSONL
        try {
            auto j = nlohmann::json::parse(line);
            std::string ts = j.value("timestamp", "?");
            std::string event = j.value("event", j.value("type", "?"));
            std::cout << "  [" << ts << "] " << event << "\n";
        } catch (const std::exception&) {
            std::cout << "  " << line << "\n";
        }
    }

    std::cout << "\n" << entries << " " << tr("entries replayed") << "\n";
    return 0;
}

// --- context-worker ---

int cmd_context_worker(Context& ctx, const std::vector<std::string>& args) {
    bool daemon_mode = false;
    std::string context_scope = "project";
    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "--daemon") {
            daemon_mode = true;
        } else if (args[i] == "--scope" && i + 1 < args.size()) {
            context_scope = args[++i];
        }
    }

    std::cout << term::bold(tr("Context Worker")) << "\n\n";

    auto runtime_dir = fs::path(ctx.euxis_home) / "data/runtime";
    auto context_dir = runtime_dir / "context";

    // Daemon mode: fork a background process
    if (daemon_mode) {
        auto pid_file = runtime_dir / "context-worker.pid";

        // Check if already running
        if (fs::exists(pid_file)) {
            std::ifstream pf(pid_file);
            std::string existing_pid;
            std::getline(pf, existing_pid);
            auto check = Process::run("kill", {"-0", existing_pid});
            if (check.exit_code == 0) {
                std::cout << "  " << term::icon_warn()
                          << " " << tr("Context worker already running (PID") << " " << existing_pid << ")\n";
                return 1;
            }
            // Stale PID file, remove it
            fs::remove(pid_file);
        }

        std::cout << "  " << tr("Forking background context worker...") << "\n";
        fs::create_directories(runtime_dir);
        fs::create_directories(context_dir);

        pid_t pid = ::fork();
        if (pid < 0) {
            std::cerr << term::icon_fail() << " " << tr("fork() failed") << "\n";
            return 1;
        }

        if (pid > 0) {
            // Parent: write PID file and exit
            std::ofstream pf(pid_file);
            pf << pid;
            std::cout << "  " << term::icon_ok() << " " << tr("Context worker started (PID") << " " << pid << ")\n";
            std::cout << "  " << tr("PID file:") << " " << pid_file.string() << "\n";
            return 0;
        }

        // Child: detach and run context gathering in a loop
        // Create new session to detach from terminal
        [[maybe_unused]] auto sid = ::setsid();

        // Redirect stdout/stderr to log file
        auto log_path = runtime_dir / "context-worker.log";
        [[maybe_unused]] auto out_fd = ::freopen(log_path.c_str(), "a", stdout);
        [[maybe_unused]] auto err_fd = ::freopen(log_path.c_str(), "a", stderr);

        // Background loop: gather context periodically
        while (true) {
            // Scan project files and build context summaries
            auto project_root = fs::path(ctx.euxis_home);

            // Scan key directories
            for (const auto& subdir : {"data", "euxis-core", "euxis-cli-cpp"}) {
                auto scan_path = project_root / subdir;
                if (!fs::is_directory(scan_path)) continue;

                auto listing = scan_directory_listing(scan_path);
                if (!listing.empty()) {
                    auto summary_file = context_dir / (std::string(subdir) + "-listing.txt");
                    std::ofstream sf(summary_file);
                    sf << "# Context listing for " << subdir << "\n"
                       << "# Generated: " << std::chrono::system_clock::now().time_since_epoch().count() << "\n\n"
                       << listing;
                }
            }

            // Write agent registry snapshot as context
            {
                RegistryClient registry(ctx.data_dir);
                auto agents = registry.list_agents();
                nlohmann::json ctx_json;
                ctx_json["agent_count"] = static_cast<int>(agents.size());
                ctx_json["agents"] = nlohmann::json::array();
                for (const auto& a : agents) {
                    ctx_json["agents"].push_back({
                        {"id", a.id}, {"role", a.role}, {"tier", a.tier}
                    });
                }
                auto agent_ctx_file = context_dir / "agents-context.json";
                std::ofstream af(agent_ctx_file);
                af << ctx_json.dump(2);
            }

            // Sleep 5 minutes between scans
            ::sleep(300);
        }
        // Child never reaches here
    }

    // One-shot mode: gather context now
    std::cout << "  " << tr("Mode:") << "  " << tr("one-shot") << "\n"
              << "  " << tr("Scope:") << " " << context_scope << "\n\n";

    fs::create_directories(context_dir);

    // 1. Scan project files
    auto project_root = fs::path(ctx.euxis_home);
    int summaries_written = 0;

    std::cout << "  " << tr("Scanning project directories...") << "\n";
    for (const auto& subdir : {"data", "data/runtime", "euxis-core", "euxis-cli-cpp"}) {
        auto scan_path = project_root / subdir;
        if (!fs::is_directory(scan_path)) continue;

        auto listing = scan_directory_listing(scan_path);
        if (!listing.empty()) {
            auto summary_file = context_dir / (std::string(subdir) + "-listing.txt");
            write_output_file(summary_file, "# Context listing for " + std::string(subdir) +
                              "\n\n" + listing);
            std::cout << "    " << term::icon_ok() << " " << subdir << "\n";
            ++summaries_written;
        } else {
            std::cout << "    " << term::icon_skip() << " " << subdir << " (" << tr("empty") << ")\n";
        }
    }

    // 2. Build agent registry context
    {
        RegistryClient registry(ctx.data_dir);
        auto agents = registry.list_agents();
        auto squads = registry.list_squads();

        nlohmann::json ctx_json;
        ctx_json["agent_count"] = static_cast<int>(agents.size());
        ctx_json["squad_count"] = static_cast<int>(squads.size());
        ctx_json["agents"] = nlohmann::json::array();
        for (const auto& a : agents) {
            ctx_json["agents"].push_back({
                {"id", a.id}, {"role", a.role}, {"tier", a.tier},
                {"version", a.version}, {"tags", a.tags}
            });
        }
        ctx_json["squads"] = nlohmann::json::array();
        for (const auto& s : squads) {
            ctx_json["squads"].push_back({
                {"id", s.id}, {"name", s.name}, {"members", s.members.size()}
            });
        }

        auto agent_ctx_file = context_dir / "agents-context.json";
        write_output_file(agent_ctx_file, ctx_json.dump(2));
        std::cout << "    " << term::icon_ok() << " " << tr("Agent registry context") << "\n";
        ++summaries_written;
    }

    // 3. Build provider context
    {
        ProviderRouter router(ctx.data_dir);
        nlohmann::json prov_json;
        prov_json["active_provider"] = router.detect_provider();
        prov_json["available"] = router.available_providers();
        prov_json["local_available"] = router.local_available();

        auto prov_ctx_file = context_dir / "provider-context.json";
        write_output_file(prov_ctx_file, prov_json.dump(2));
        std::cout << "    " << term::icon_ok() << " " << tr("Provider context") << "\n";
        ++summaries_written;
    }

    // 4. Build session/memory context
    {
        Session session(ctx.euxis_home);
        nlohmann::json session_json;
        session_json["project"] = session.project_name();
        session_json["session_id"] = session.session_id();
        session_json["euxis_home"] = ctx.euxis_home;
        session_json["data_dir"] = ctx.data_dir;

        auto session_ctx_file = context_dir / "session-context.json";
        write_output_file(session_ctx_file, session_json.dump(2));
        std::cout << "    " << term::icon_ok() << " " << tr("Session context") << "\n";
        ++summaries_written;
    }

    // Report final summary
    size_t total_size = 0;
    int file_count = 0;
    if (fs::is_directory(context_dir)) {
        for (const auto& entry : fs::recursive_directory_iterator(context_dir)) {
            if (entry.is_regular_file()) {
                ++file_count;
                total_size += entry.file_size();
            }
        }
    }

    std::cout << "\n  " << tr("Context files:") << " " << file_count << "\n"
              << "  " << tr("Total size:") << "    " << (total_size / 1024) << " KB\n"
              << "  " << tr("Written:") << "       " << summaries_written << " " << tr("summaries") << "\n"
              << "  " << tr("Location:") << "      " << context_dir.string() << "\n";

    std::cout << "\n" << term::icon_ok() << " " << tr("Context gathering complete") << "\n";
    return 0;
}

// --- lsp ---

int cmd_lsp(Context& ctx, const std::vector<std::string>& args) {
    namespace term = terminal;
    using euxis::cli::i18n::tr;

    // Read latest verdict artifact
    std::string artifact_path;
    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "--artifact" && i + 1 < args.size()) {
            artifact_path = args[++i];
        }
    }

    if (artifact_path.empty()) {
        auto sessions_dir = std::filesystem::path(ctx.data_dir) / "runtime" / "sessions";
        auto latest = sessions_dir / "latest.json";
        if (std::filesystem::exists(latest)) {
            artifact_path = latest.string();
        } else {
            std::cerr << tr("No verdict artifact found. Run 'euxis check' first.") << "\n";
            return 2;
        }
    }

    try {
        std::ifstream f(artifact_path);
        auto artifact = nlohmann::json::parse(f);

        // Extract findings from agent_status entries
        std::vector<SarifFinding> findings;
        if (artifact.contains("agent_status") && artifact["agent_status"].is_array()) {
            for (const auto& agent : artifact["agent_status"]) {
                std::string agent_id = agent.value("agent_id", "");
                std::string pillar = agent.value("pillar", "");
                std::string verdict = agent.value("verdict", "");
                if (agent.contains("key_findings") && agent["key_findings"].is_array()) {
                    int idx = 0;
                    for (const auto& finding : agent["key_findings"]) {
                        idx++;
                        std::string level = (verdict == "FAILED") ? "error" : "warning";
                        findings.push_back({
                            "euxis/" + pillar + "-" + std::to_string(idx),
                            finding.get<std::string>(),
                            level,
                            "", 0,
                            agent_id,
                            pillar
                        });
                    }
                }
            }
        }

        auto lsp_output = findings_to_lsp_diagnostics(findings);
        std::cout << lsp_output.dump(2) << "\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}

} // namespace euxis::cli::cmd
