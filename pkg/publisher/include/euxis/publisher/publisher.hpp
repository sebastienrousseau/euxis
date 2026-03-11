/// @file
/// @brief High-performance C++23 Publication Engine (Ported from Python).
#pragma once

#include <expected>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

namespace euxis::publisher {

enum class OutputFormat {
    LaTeX,
    Markdown,
    JSON
};

enum class BuildMode {
    Draft,
    Submission,
    CameraReady
};

struct PublishResult {
    bool success{false};
    std::string output_path;
    std::string error_message;
    double duration_ms{0.0};
};

/**
 * @brief High-performance Publication engine for rendering documents.
 * 
 * Ported from Python to C++23 for sub-10ms rendering and hardware-native execution.
 */
class Publisher {
public:
    explicit Publisher(std::filesystem::path content_root);

    /**
     * @brief Render a document from a template and data.
     */
    auto render(std::string_view doc_id, 
                OutputFormat format = OutputFormat::LaTeX,
                BuildMode mode = BuildMode::Draft) 
        -> std::expected<std::string, std::string>;

    /**
     * @brief Build a PDF from a rendered LaTeX document.
     */
    auto build_pdf(std::string_view doc_id, 
                   BuildMode mode = BuildMode::Draft) 
        -> std::expected<std::filesystem::path, std::string>;

    /**
     * @brief Utility: Convert YAML to JSON for Inja ingestion.
     */
    auto load_data(const std::filesystem::path& path) 
        -> std::expected<nlohmann::json, std::string>;

private:
    std::filesystem::path root_;
    std::filesystem::path template_dir_;
    std::filesystem::path data_dir_;
    std::filesystem::path build_dir_;
};

} // namespace euxis::publisher
