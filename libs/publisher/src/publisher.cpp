#include "euxis/publisher/publisher.hpp"

#include <fstream>
#include <iostream>
#include <sstream>
#include <utility>

#include <inja/inja.hpp>
#include <yaml-cpp/yaml.h>
#include <spdlog/spdlog.h>

namespace euxis::publisher {

namespace {

/**
 * @brief Convert YAML::Node to JSON using recursive visitor.
 */
nlohmann::json yaml_to_json(const DataNode auto& node) {
    if (node.IsScalar()) {
        try { return node.template as<bool>(); } catch (const std::exception&) { /* swallowed: best-effort path */ (void)0; }
        try { return node.template as<int64_t>(); } catch (const std::exception&) { /* swallowed: best-effort path */ (void)0; }
        try { return node.template as<double>(); } catch (const std::exception&) { /* swallowed: best-effort path */ (void)0; }
        return node.template as<std::string>();
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
            j[it->first.template as<std::string>()] = yaml_to_json(it->second);
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
        [[unlikely]] return std::unexpected(std::format("YAML I/O failure at {}: {}", path.string(), e.what()));
    }
}

auto Publisher::get_template_entry(const nlohmann::json& meta, std::string_view doc_id) 
    -> std::expected<nlohmann::json, std::string> {
    if (!meta.contains("templates") || !meta["templates"].contains(std::string(doc_id))) [[unlikely]] {
        return std::unexpected(std::format("Missing metadata entry for: {}", doc_id));
    }
    return meta["templates"][std::string(doc_id)];
}

auto Publisher::render(std::string_view doc_id, OutputFormat format, BuildMode mode) 
    -> std::expected<std::string, std::string> {
    
    return load_data(data_dir_ / "meta.yaml")
        .and_then([&](nlohmann::json&& meta) { return get_template_entry(meta, doc_id); })
        .and_then([&](nlohmann::json&& entry) {
            std::string data_file = entry.value("data", "");
            return load_data(data_dir_ / data_file)
                .transform([&](nlohmann::json&& data) {
                    return std::make_pair(entry.value("template", ""), std::move(data));
                });
        })
        .and_then([&](auto&& pair) -> std::expected<std::string, std::string> {
            auto& [tmpl_name, data] = pair;
            data["build_mode"] = (mode == BuildMode::CameraReady ? "camera-ready" : 
                                  (mode == BuildMode::Submission ? "submission" : "draft"));

            if (format == OutputFormat::JSON) return data.dump(2);
            if (format != OutputFormat::LaTeX) return std::unexpected("Unsupported output format");

            inja::Environment env;
            env.set_expression("<<", ">>");
            env.set_statement("<%", "%>");
            env.set_comment("<#", "#>");
            
            try {
                return env.render_file((template_dir_ / tmpl_name).string(), data);
            } catch (const std::exception& e) {
                [[unlikely]] return std::unexpected(std::format("Inja Engine Error: {}", e.what()));
            }
        });
}

auto Publisher::build_pdf(std::string_view doc_id, BuildMode mode) 
    -> std::expected<std::filesystem::path, std::string> {
    
    return render(doc_id, OutputFormat::LaTeX, mode)
        .and_then([&](std::string&& tex_content) -> std::expected<std::filesystem::path, std::string> {
            auto out_dir = build_dir_ / ".cache" / "rendered";
            std::filesystem::create_directories(out_dir);
            auto tex_path = out_dir / std::format("{}.tex", doc_id);
            
            std::ofstream out(tex_path);
            if (!out) [[unlikely]] return std::unexpected("Failed to write intermediate TeX");
            out << tex_content;
            
            spdlog::info("Dispatching PDF build for: {}", doc_id);
            return tex_path.replace_extension(".pdf");
        });
}

} // namespace euxis::publisher
