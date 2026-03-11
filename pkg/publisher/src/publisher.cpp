#include "euxis/publisher/publisher.hpp"

#include <fstream>
#include <iostream>
#include <sstream>

#include <inja/inja.hpp>
#include <yaml-cpp/yaml.h>
#include <spdlog/spdlog.h>

namespace euxis::publisher {

namespace {

/**
 * @brief Helper to convert YAML::Node to nlohmann::json.
 */
nlohmann::json yaml_to_json(const YAML::Node& node) {
    if (node.IsScalar()) {
        try { return node.as<bool>(); } catch (...) {}
        try { return node.as<int64_t>(); } catch (...) {}
        try { return node.as<double>(); } catch (...) {}
        return node.as<std::string>();
    }
    if (node.IsSequence()) {
        auto j = nlohmann::json::array();
        for (auto const& item : node) {
            j.push_back(yaml_to_json(item));
        }
        return j;
    }
    if (node.IsMap()) {
        auto j = nlohmann::json::object();
        for (auto it = node.begin(); it != node.end(); ++it) {
            j[it->first.as<std::string>()] = yaml_to_json(it->second);
        }
        return j;
    }
    return nullptr;
}

} // namespace

Publisher::Publisher(std::filesystem::path content_root)
    : root_(std::move(content_root)) {
    template_dir_ = root_ / "templates";
    data_dir_ = root_ / "data";
    build_dir_ = root_ / "build";
}

auto Publisher::load_data(const std::filesystem::path& path) 
    -> std::expected<nlohmann::json, std::string> {
    try {
        YAML::Node config = YAML::LoadFile(path.string());
        return yaml_to_json(config);
    } catch (const std::exception& e) {
        return std::unexpected(std::format("Failed to load YAML data from {}: {}", path.string(), e.what()));
    }
}

auto Publisher::render(std::string_view doc_id, OutputFormat format, BuildMode mode) 
    -> std::expected<std::string, std::string> {
    
    // 1. Load metadata
    auto meta_res = load_data(data_dir_ / "meta.yaml");
    if (!meta_res) return std::unexpected(meta_res.error());
    const auto& meta = *meta_res;

    // 2. Find document entry
    if (!meta.contains("templates") || !meta["templates"].contains(std::string(doc_id))) {
        return std::unexpected(std::format("Document ID '{}' not found in meta.yaml", doc_id));
    }
    const auto& entry = meta["templates"][std::string(doc_id)];
    std::string template_name = entry.value("template", "");
    std::string data_filename = entry.value("data", "");

    // 3. Load document data
    auto data_res = load_data(data_dir_ / data_filename);
    if (!data_res) return std::unexpected(data_res.error());
    auto data = *data_res;

    // 4. Inject build mode
    data["build_mode"] = (mode == BuildMode::CameraReady ? "camera-ready" : 
                          (mode == BuildMode::Submission ? "submission" : "draft"));

    // 5. Render
    if (format == OutputFormat::LaTeX) {
        inja::Environment env;
        // Port custom delimiters from Python to avoid LaTeX conflicts
        // inja API: set_expression(open, close), set_statement(open, close), set_comment(open, close)
        env.set_expression("<<", ">>"); // variable equivalent in inja
        env.set_statement("<%", "%>"); // block equivalent in inja
        env.set_comment("<#", "#>");
        
        try {
            return env.render_file((template_dir_ / template_name).string(), data);
        } catch (const std::exception& e) {
            return std::unexpected(std::format("Inja rendering failed: {}", e.what()));
        }
    } else if (format == OutputFormat::JSON) {
        return data.dump(2);
    }

    return std::unexpected("Format not implemented yet in C++23 port");
}

auto Publisher::build_pdf(std::string_view doc_id, BuildMode mode) 
    -> std::expected<std::filesystem::path, std::string> {
    
    // 1. Render to LaTeX
    auto tex_res = render(doc_id, OutputFormat::LaTeX, mode);
    if (!tex_res) return std::unexpected(tex_res.error());

    // 2. Save to build/.cache/rendered
    auto out_dir = build_dir_ / ".cache" / "rendered";
    std::filesystem::create_directories(out_dir);
    auto tex_path = out_dir / std::format("{}.tex", doc_id);
    
    std::ofstream out(tex_path);
    out << *tex_res;
    out.close();

    // 3. Execute latexmk (simulated for now)
    spdlog::info("Building PDF for {} via latexmk...", doc_id);
    
    return tex_path.replace_extension(".pdf");
}

} // namespace euxis::publisher
