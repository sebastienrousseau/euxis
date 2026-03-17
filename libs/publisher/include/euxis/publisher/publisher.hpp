/// @file
/// @brief High-performance C++23 Publication Engine (Premium Implementation).
#pragma once

#include <expected>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>
#include <concepts>

#include <nlohmann/json.hpp>

namespace euxis::publisher {

// --- 3-Word Definitions ---
// RAII: Resource-bound lifetime management.
// Concepts: Compile-time template constraints.
// Monadic: Functional error chaining.

/**
 * @brief Constraint for valid YAML/JSON data nodes.
 */
template <typename T>
concept DataNode = requires(T t) {
    { t.IsMap() } -> std::convertible_to<bool>;
    { t.IsSequence() } -> std::convertible_to<bool>;
};

enum class OutputFormat { LaTeX, Markdown, JSON };
enum class BuildMode { Draft, Submission, CameraReady };

/**
 * @brief High-performance Publication engine.
 */
class Publisher {
public:
    explicit Publisher(std::filesystem::path content_root);

    /**
     * @brief Render a document using monadic error handling.
     * @pre content_root must contain data/meta.yaml.
     * @post Returns std::expected with rendered string.
     */
    auto render(std::string_view doc_id, 
                OutputFormat format = OutputFormat::LaTeX,
                BuildMode mode = BuildMode::Draft) 
        -> std::expected<std::string, std::string>;

    /**
     * @brief Orchestrate binary PDF generation via latexmk.
     */
    auto build_pdf(std::string_view doc_id, 
                   BuildMode mode = BuildMode::Draft) 
        -> std::expected<std::filesystem::path, std::string>;

    /**
     * @brief Load structured data into hardware-friendly JSON.
     */
    auto load_data(const std::filesystem::path& path) 
        -> std::expected<nlohmann::json, std::string>;

private:
    std::filesystem::path root_;
    std::filesystem::path template_dir_;
    std::filesystem::path data_dir_;
    std::filesystem::path build_dir_;

    auto get_template_entry(const nlohmann::json& meta, std::string_view doc_id) 
        -> std::expected<nlohmann::json, std::string>;
};

} // namespace euxis::publisher
