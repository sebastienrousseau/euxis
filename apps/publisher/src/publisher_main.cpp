#include "euxis/publisher/main_entry.hpp"

#include "euxis/publisher/publisher.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

namespace euxis::publisher {

namespace {

void print_usage(std::ostream& out) {
    out << R"(Euxis Publisher (C++23)
Usage: euxis-publisher <command> [options]

Commands:
  render --doc <id> [--format <latex|json>]
  build  --doc <id> [--mode <draft|submission|camera-ready>]
)";
}

} // namespace

int publisher_main(int argc, char* argv[], std::ostream& out, std::ostream& err) {
    if (argc < 2) {
        print_usage(out);
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
        err << "ERROR: --doc <id> is required\n";
        return 1;
    }

    const char* home = std::getenv("HOME");
    std::filesystem::path content_root;
    if (home) {
        content_root = std::filesystem::path(home) / "Code/Private/Python/euxis-publisher";
    }
    if (content_root.empty() || !std::filesystem::exists(content_root)) {
        content_root = std::filesystem::current_path();
    }

    Publisher publisher(content_root);

    if (command == "render") {
        auto fmt = (format_str == "json" ? OutputFormat::JSON : OutputFormat::LaTeX);
        auto res = publisher.render(doc_id, fmt);
        if (res) {
            out << *res << std::endl;
        } else {
            err << "ERROR: " << res.error() << std::endl;
            return 1;
        }
    } else if (command == "build") {
        auto mode = (mode_str == "camera-ready" ? BuildMode::CameraReady :
                     (mode_str == "submission" ? BuildMode::Submission : BuildMode::Draft));
        auto res = publisher.build_pdf(doc_id, mode);
        if (res) {
            out << "SUCCESS: PDF built at " << res->string() << std::endl;
        } else {
            err << "ERROR: " << res.error() << std::endl;
            return 1;
        }
    } else {
        print_usage(out);
        return 1;
    }

    return 0;
}

int publisher_main(int argc, char* argv[]) {
    return publisher_main(argc, argv, std::cout, std::cerr);
}

} // namespace euxis::publisher
