#include <iostream>
#include <string>
#include <vector>
#include <filesystem>

#include "euxis/publisher/publisher.hpp"
#include <spdlog/spdlog.h>

void print_usage() {
    std::cout << R"(Euxis Publisher (C++23)
Usage: euxis-publisher <command> [options]

Commands:
  render --doc <id> [--format <latex|json>]
  build  --doc <id> [--mode <draft|submission|camera-ready>]
)";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage();
        return 1;
    }

    std::string command = argv[1];
    std::string doc_id;
    std::string format_str = "latex";
    std::string mode_str = "draft";

    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--doc" && i + 1 < argc) doc_id = argv[++i];
        else if (arg == "--format" && i + 1 < argc) format_str = argv[++i];
        else if (arg == "--mode" && i + 1 < argc) mode_str = argv[++i];
    }

    if (doc_id.empty()) {
        std::cerr << "ERROR: --doc <id> is required\n";
        return 1;
    }

    // Default content root (assuming we are in ~/.euxis)
    const char* home = std::getenv("HOME");
    std::filesystem::path content_root = std::filesystem::path(home) / "Code/Private/Python/euxis-publisher";
    
    // Check if path exists
    if (!std::filesystem::exists(content_root)) {
        // Fallback to current dir if porting complete
        content_root = std::filesystem::current_path();
    }

    euxis::publisher::Publisher publisher(content_root);

    if (command == "render") {
        auto fmt = (format_str == "json" ? euxis::publisher::OutputFormat::JSON : euxis::publisher::OutputFormat::LaTeX);
        auto res = publisher.render(doc_id, fmt);
        if (res) {
            std::cout << *res << std::endl;
        } else {
            std::cerr << "ERROR: " << res.error() << std::endl;
            return 1;
        }
    } else if (command == "build") {
        auto mode = (mode_str == "camera-ready" ? euxis::publisher::BuildMode::CameraReady : 
                     (mode_str == "submission" ? euxis::publisher::BuildMode::Submission : euxis::publisher::BuildMode::Draft));
        auto res = publisher.build_pdf(doc_id, mode);
        if (res) {
            std::cout << "SUCCESS: PDF built at " << res->string() << std::endl;
        } else {
            std::cerr << "ERROR: " << res.error() << std::endl;
            return 1;
        }
    } else {
        print_usage();
        return 1;
    }

    return 0;
}
