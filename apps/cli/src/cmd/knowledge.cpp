#include "euxis/cli/cmd/knowledge.hpp"
#include "euxis/cli/config_loader.hpp"
#include "euxis/cli/engine.hpp"
#include "euxis/cli/i18n.hpp"
#include "euxis/cli/process.hpp"
#include "euxis/cli/provider_executor.hpp"
#include "euxis/cli/terminal.hpp"
#include "euxis/cli/omnigraph.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>

namespace euxis::cli::cmd {

using euxis::cli::i18n::tr;
namespace {

namespace fs = std::filesystem;
namespace term = terminal;

/// Return the path to the cortex entries.json file, creating parent dirs if needed.
auto cortex_entries_path(const std::string& euxis_home) -> fs::path {
    auto dir = fs::path(euxis_home) / "data/runtime" / "memory" / "cortex";
    if (!fs::is_directory(dir)) {
        fs::create_directories(dir);
    }
    return dir / "entries.json";
}

/// Load the cortex entries.json as a JSON object. Returns empty object if missing.
auto load_cortex_entries(const fs::path& path) -> nlohmann::json {
    if (!fs::exists(path)) {
        return nlohmann::json::object();
    }
    std::ifstream f(path);
    if (!f.is_open()) {
        return nlohmann::json::object();
    }
    try {
        auto j = nlohmann::json::parse(f);
        if (j.is_object()) {
            return j;
        }
    } catch (const std::exception&) {
        // Corrupt file; start fresh
    }
    return nlohmann::json::object();
}

/// Save cortex entries to disk.
void save_cortex_entries(const fs::path& path, const nlohmann::json& entries) {
    std::ofstream f(path);
    f << entries.dump(2) << "\n";
}

} // namespace

// --- cortex ---

int cmd_cortex(Context& ctx, const std::vector<std::string>& args) {
    if (args.empty()) {
        std::cerr << tr("Usage: euxis cortex <remember|recall|stats|forget> [args]") << "\n";
        return 2;
    }

    auto entries_path = cortex_entries_path(ctx.euxis_home);

    if (args[0] == "stats") {
        auto db_dir = fs::path(ctx.euxis_home) / "data/runtime" / "memory" / "cortex" / "db";
        auto entries = load_cortex_entries(entries_path);
        auto entry_count = static_cast<int>(entries.size());

        size_t total_size = 0;
        int file_count = 0;
        if (fs::is_directory(db_dir)) {
            for (const auto& entry : fs::recursive_directory_iterator(db_dir)) {
                if (entry.is_regular_file()) {
                    total_size += entry.file_size();
                    ++file_count;
                }
            }
        }

        if (ctx.json_output) {
            nlohmann::json j;
            j["db_path"] = db_dir.string();
            j["files"] = file_count;
            j["size_bytes"] = total_size;
            j["entries"] = entry_count;
            std::cout << j.dump(2) << "\n";
        } else {
            std::cout << tr("Cortex Stats") << "\n"
                      << "  " << tr("DB path:") << "  " << db_dir.string() << "\n"
                      << "  " << tr("Files:") << "    " << file_count << "\n"
                      << "  " << tr("Size:") << "     " << (total_size / 1024) << " KB\n"
                      << "  " << tr("Entries:") << "  " << entry_count << "\n";
        }
        return 0;
    }

    if (args[0] == "remember") {
        if (args.size() < 3) {
            std::cerr << tr("Usage: euxis cortex remember <key> <value...>") << "\n";
            return 2;
        }
        auto key = args[1];
        // Join remaining args as the value
        std::string value;
        for (size_t i = 2; i < args.size(); ++i) {
            if (!value.empty()) value += ' ';
            value += args[i];
        }

        auto entries = load_cortex_entries(entries_path);
        entries[key] = value;
        save_cortex_entries(entries_path, entries);

        if (ctx.json_output) {
            nlohmann::json j;
            j["action"] = "remember";
            j["key"] = key;
            j["value"] = value;
            std::cout << j.dump(2) << "\n";
        } else {
            std::cout << term::icon_ok() << " " << tr("Remembered:") << " " << term::cyan(key)
                      << " = " << value << "\n";
        }
        return 0;
    }

    if (args[0] == "recall") {
        if (args.size() < 2) {
            std::cerr << tr("Usage: euxis cortex recall <key>") << "\n";
            return 2;
        }
        auto key = args[1];
        auto entries = load_cortex_entries(entries_path);

        // Exact match
        if (entries.contains(key)) {
            auto value = entries[key].get<std::string>();
            if (ctx.json_output) {
                nlohmann::json j;
                j["key"] = key;
                j["value"] = value;
                j["match"] = "exact";
                std::cout << j.dump(2) << "\n";
            } else {
                std::cout << term::cyan(key) << " = " << value << "\n";
            }
            return 0;
        }

        // Partial match: search all keys and values for the term
        nlohmann::json matches = nlohmann::json::array();
        for (const auto& [k, v] : entries.items()) {
            auto val = v.get<std::string>();
            if (k.find(key) != std::string::npos || val.find(key) != std::string::npos) {
                matches.push_back({{"key", k}, {"value", val}});
            }
        }

        if (!matches.empty()) {
            if (ctx.json_output) {
                nlohmann::json j;
                j["query"] = key;
                j["match"] = "partial";
                j["results"] = matches;
                std::cout << j.dump(2) << "\n";
            } else {
                std::cout << term::yellow(tr("No exact match. Partial matches:")) << "\n";
                for (const auto& m : matches) {
                    std::cout << "  " << term::cyan(m["key"].get<std::string>())
                              << " = " << m["value"].get<std::string>() << "\n";
                }
            }
            return 0;
        }

        if (ctx.json_output) {
            nlohmann::json j;
            j["query"] = key;
            j["match"] = "none";
            j["results"] = nlohmann::json::array();
            std::cout << j.dump(2) << "\n";
        } else {
            std::cerr << term::icon_fail() << " " << tr("Not found:") << " " << key << "\n";
        }
        return 1;
    }

    if (args[0] == "forget") {
        if (args.size() < 2) {
            std::cerr << tr("Usage: euxis cortex forget <key>") << "\n";
            return 2;
        }
        auto key = args[1];
        auto entries = load_cortex_entries(entries_path);

        if (!entries.contains(key)) {
            if (ctx.json_output) {
                nlohmann::json j;
                j["action"] = "forget";
                j["key"] = key;
                j["found"] = false;
                std::cout << j.dump(2) << "\n";
            } else {
                std::cerr << term::icon_fail() << " " << tr("Key not found:") << " " << key << "\n";
            }
            return 1;
        }

        entries.erase(key);
        save_cortex_entries(entries_path, entries);

        if (ctx.json_output) {
            nlohmann::json j;
            j["action"] = "forget";
            j["key"] = key;
            j["found"] = true;
            std::cout << j.dump(2) << "\n";
        } else {
            std::cout << term::icon_ok() << " " << tr("Forgot:") << " " << key << "\n";
        }
        return 0;
    }

    std::cerr << tr("Unknown cortex command:") << " " << args[0] << "\n"
              << tr("Usage: euxis cortex <remember|recall|stats|forget> [args]") << "\n";
    return 2;
}

// --- graph ---

int cmd_graph(Context& ctx, const std::vector<std::string>& args) {
    if (args.empty()) {
        std::cerr << tr("Usage: euxis graph <show|query|export> [args]") << "\n";
        return 2;
    }

    auto graph_dir = fs::path(ctx.euxis_home) / "data/runtime" / "memory" / "cortex";

    if (args[0] == "show") {
        std::cout << term::bold(tr("Knowledge Graph")) << "\n";
        if (fs::is_directory(graph_dir)) {
            int count = 0;
            for (const auto& entry : fs::directory_iterator(graph_dir)) {
                if (entry.is_regular_file()) {
                    std::cout << "  " << entry.path().filename().string() << "\n";
                    ++count;
                }
            }
            std::cout << "\n" << count << " " << tr("file(s)") << "\n";
        } else {
            std::cout << "  " << tr("Graph directory not found") << "\n";
        }
        return 0;
    }

    if (args[0] == "query") {
        if (args.size() < 2) {
            std::cerr << tr("Usage: euxis graph query <search-term>") << "\n";
            return 2;
        }
        auto term_str = args[1];

        if (!fs::is_directory(graph_dir)) {
            std::cerr << tr("Graph directory not found:") << " " << graph_dir.string() << "\n";
            return 1;
        }

        int match_count = 0;
        nlohmann::json results = nlohmann::json::array();

        for (const auto& entry : fs::recursive_directory_iterator(graph_dir)) {
            if (!entry.is_regular_file()) continue;

            std::ifstream f(entry.path());
            if (!f.is_open()) continue;

            std::string line;
            int line_num = 0;
            while (std::getline(f, line)) {
                ++line_num;
                if (line.find(term_str) != std::string::npos) {
                    ++match_count;
                    if (ctx.json_output) {
                        results.push_back({
                            {"file", entry.path().string()},
                            {"line", line_num},
                            {"content", line}
                        });
                    } else {
                        std::cout << term::cyan(entry.path().string())
                                  << ":" << line_num << ": " << line << "\n";
                    }
                }
            }
        }

        if (ctx.json_output) {
            nlohmann::json j;
            j["query"] = term_str;
            j["matches"] = match_count;
            j["results"] = results;
            std::cout << j.dump(2) << "\n";
        } else if (match_count == 0) {
            std::cout << tr("No matches for:") << " " << term_str << "\n";
        } else {
            std::cout << "\n" << match_count << " " << tr("match(es)") << "\n";
        }
        return 0;
    }

    if (args[0] == "export" && args.size() >= 2) {
        auto output = args[1];
        if (fs::is_directory(graph_dir)) {
            // Export graph data as JSON
            nlohmann::json j;
            j["nodes"] = nlohmann::json::array();
            for (const auto& entry : fs::directory_iterator(graph_dir)) {
                if (entry.is_regular_file()) {
                    j["nodes"].push_back({{"file", entry.path().filename().string()},
                                          {"size", entry.file_size()}});
                }
            }
            std::ofstream f(output);
            f << j.dump(2);
            std::cout << term::icon_ok() << " " << tr("Exported to:") << " " << output << "\n";
            return 0;
        }
        std::cerr << tr("No graph data to export") << "\n";
        return 1;
    }

    std::cerr << tr("Unknown graph command:") << " " << args[0] << "\n";
    return 2;
}

// --- codex ---

int cmd_codex(Context& ctx, const std::vector<std::string>& args) {
    ConfigLoader loader(ctx.data_dir);

    if (args.empty() || args[0] == "list") {
        auto codex = loader.load("config/codex.json");
        if (!codex) {
            std::cout << tr("No codex.json found") << "\n";
            return 0;
        }

        std::cout << term::bold(tr("Template Codex")) << "\n\n";
        if (codex->contains("templates") && (*codex)["templates"].is_array()) {
            for (const auto& t : (*codex)["templates"]) {
                std::string name = t.value("name", "?");
                std::string desc = t.value("description", "");
                std::cout << "  " << term::cyan(name) << "  " << desc << "\n";
            }
        }
        return 0;
    }

    if (args[0] == "validate") {
        auto codex = loader.load("config/codex.json");
        if (!codex) {
            std::cerr << tr("codex.json not found") << "\n";
            return 1;
        }
        int issues = 0;
        if (codex->contains("templates") && (*codex)["templates"].is_array()) {
            for (const auto& t : (*codex)["templates"]) {
                std::string file = t.value("file", "");
                if (!file.empty()) {
                    auto path = fs::path(ctx.euxis_home) / file;
                    if (!fs::exists(path)) {
                        std::cout << term::icon_fail() << " " << tr("Missing:") << " " << file << "\n";
                        ++issues;
                    }
                }
            }
        }
        if (issues == 0) std::cout << term::icon_ok() << " " << tr("Codex valid") << "\n";
        return issues > 0 ? 1 : 0;
    }

    if (args[0] == "render") {
        if (args.size() < 2) {
            std::cerr << tr("Usage: euxis codex render <template-name> [--var KEY=VALUE ...]") << "\n";
            return 2;
        }
        auto template_name = args[1];

        // Parse --var KEY=VALUE pairs
        std::map<std::string, std::string> vars;
        for (size_t i = 2; i < args.size(); ++i) {
            if (args[i] == "--var" && i + 1 < args.size()) {
                auto kv = args[++i];
                auto eq_pos = kv.find('=');
                if (eq_pos != std::string::npos) {
                    vars[kv.substr(0, eq_pos)] = kv.substr(eq_pos + 1);
                }
            }
        }

        // Load codex.json and find the template
        auto codex = loader.load("config/codex.json");
        if (!codex) {
            std::cerr << tr("codex.json not found") << "\n";
            return 1;
        }

        if (!codex->contains("templates") || !(*codex)["templates"].is_array()) {
            std::cerr << tr("No templates defined in codex.json") << "\n";
            return 1;
        }

        std::string template_file;
        for (const auto& t : (*codex)["templates"]) {
            if (t.value("name", "") == template_name) {
                template_file = t.value("file", "");
                break;
            }
        }

        if (template_file.empty()) {
            std::cerr << term::icon_fail() << " " << tr("Template not found:") << " " << template_name << "\n";
            return 1;
        }

        // Resolve template file path
        auto tpl_path = fs::path(ctx.euxis_home) / template_file;
        if (!fs::exists(tpl_path)) {
            std::cerr << term::icon_fail() << " " << tr("Template file missing:") << " "
                      << tpl_path.string() << "\n";
            return 1;
        }

        // Read the template file
        std::ifstream f(tpl_path);
        std::ostringstream buf;
        buf << f.rdbuf();
        std::string content = buf.str();

        // Substitute {{KEY}} placeholders with provided values
        for (const auto& [key, value] : vars) {
            std::string placeholder = "{{" + key + "}}";
            std::string::size_type pos = 0;
            while ((pos = content.find(placeholder, pos)) != std::string::npos) {
                content.replace(pos, placeholder.size(), value);
                pos += value.size();
            }
        }

        std::cout << content;
        return 0;
    }

    std::cerr << tr("Usage: euxis codex <list|validate|render> [args]") << "\n";
    return 2;
}

// --- omnigraph ---

int cmd_omnigraph(Context& ctx, const std::vector<std::string>& args) {
    std::string provider = "claude";
    int budget = 100000;

    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "--provider" && i + 1 < args.size()) {
            provider = args[++i];
        } else if (args[i] == "--budget" && i + 1 < args.size()) {
            budget = std::stoi(args[++i]);
        }
    }

    // Build workspace graph from current directory
    nlohmann::json graph;
    graph["workspace"] = fs::current_path().string();
    graph["nodes"] = nlohmann::json::array();

    // Scan for key files
    for (const auto& entry : fs::directory_iterator(fs::current_path())) {
        if (entry.is_regular_file()) {
            graph["nodes"].push_back({
                {"path", entry.path().filename().string()},
                {"size", entry.file_size()}
            });
        }
    }

    // Format for provider using existing GraphAdapter
    auto formatted = GraphAdapter::format_for_provider(graph, provider);

    // Apply token budget
    TokenBudgeter budgeter(budget);
    auto optimized = budgeter.optimize_graph(graph, provider);

    if (ctx.json_output) {
        std::cout << optimized.dump(2) << "\n";
    } else {
        std::cout << formatted << "\n";
    }

    return 0;
}

// --- slash ---

int cmd_slash(Context& ctx, const std::vector<std::string>& args) {
    ConfigLoader loader(ctx.data_dir);

    if (args.empty() || args[0] == "list") {
        auto config = loader.load("config/slash-commands.json");
        if (!config) {
            // Built-in slash commands
            std::cout << term::bold(tr("Slash Commands")) << "\n\n"
                      << "  /help       " << tr("Show help") << "\n"
                      << "  /agents     " << tr("List agents") << "\n"
                      << "  /squad      " << tr("Squad operations") << "\n"
                      << "  /cortex     " << tr("Memory operations") << "\n"
                      << "  /status     " << tr("System status") << "\n";
            return 0;
        }

        if (config->contains("commands") && (*config)["commands"].is_array()) {
            std::cout << term::bold(tr("Slash Commands")) << "\n\n";
            for (const auto& c : (*config)["commands"]) {
                std::string name = c.value("name", "?");
                std::string desc = c.value("description", "");
                std::cout << "  /" << term::cyan(name) << "  " << desc << "\n";
            }
        }
        return 0;
    }

    if (args[0] == "run") {
        if (args.size() < 2) {
            std::cerr << tr("Usage: euxis slash run <command-name> [args...]") << "\n";
            return 2;
        }
        auto cmd_name = args[1];

        auto config = loader.load("config/slash-commands.json");
        if (!config) {
            std::cerr << term::icon_fail() << " " << tr("No slash-commands.json found") << "\n";
            return 1;
        }

        if (!config->contains("commands") || !(*config)["commands"].is_array()) {
            std::cerr << term::icon_fail() << " " << tr("No commands defined in slash-commands.json") << "\n";
            return 1;
        }

        // Find the command definition
        nlohmann::json cmd_def;
        bool found = false;
        for (const auto& c : (*config)["commands"]) {
            if (c.value("name", "") == cmd_name) {
                cmd_def = c;
                found = true;
                break;
            }
        }

        if (!found) {
            std::cerr << term::icon_fail() << " " << tr("Unknown slash command:") << " /" << cmd_name << "\n";
            return 1;
        }

        // Collect extra args passed after the command name
        std::vector<std::string> extra_args(args.begin() + 2, args.end());

        // If the command has an "action" field, dispatch it through the Engine
        if (cmd_def.contains("action") && cmd_def["action"].is_string()) {
            auto action = cmd_def["action"].get<std::string>();

            // Build command line: the action is an euxis subcommand (e.g. "cortex stats")
            // Parse action into tokens and append extra args
            std::vector<std::string> engine_args;
            std::istringstream iss(action);
            std::string token;
            while (iss >> token) {
                engine_args.push_back(token);
            }
            for (const auto& a : extra_args) {
                engine_args.push_back(a);
            }

            // Re-invoke via the Engine
            Engine engine(ctx.euxis_home);
            return engine.run(engine_args);
        }

        // If the command has a "script" field, run it as a shell command
        if (cmd_def.contains("script") && cmd_def["script"].is_string()) {
            auto script = cmd_def["script"].get<std::string>();
            // Append extra args to the script
            for (const auto& a : extra_args) {
                script += " " + a;
            }
            auto result = Process::shell(script);
            std::cout << result.stdout_output;
            if (!result.stderr_output.empty()) std::cerr << result.stderr_output;
            return result.exit_code;
        }

        std::cerr << term::icon_fail() << " " << tr("Command") << " /" << cmd_name
                  << " " << tr("has no action or script defined") << "\n";
        return 1;
    }

    std::cerr << tr("Usage: euxis slash <list|run> [args]") << "\n";
    return 2;
}

} // namespace euxis::cli::cmd
