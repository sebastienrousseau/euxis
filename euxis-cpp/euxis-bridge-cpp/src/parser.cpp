#include <euxis/bridge/parser.hpp>

#include <fstream>
#include <sstream>

namespace euxis::bridge {

auto Frontmatter::get(const std::string& key) const -> std::optional<std::string> {
    auto it = fields.find(key);
    if (it != fields.end()) {
        return it->second;
    }
    return std::nullopt;
}

auto parse_frontmatter(const std::string& content) -> Frontmatter {
    Frontmatter result;
    std::istringstream stream(content);
    std::string line;
    bool in_frontmatter = false;
    bool frontmatter_done = false;
    std::ostringstream body;

    while (std::getline(stream, line)) {
        // Trim trailing \r
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        if (!in_frontmatter && !frontmatter_done && line == "---") {
            in_frontmatter = true;
            continue;
        }

        if (in_frontmatter && line == "---") {
            in_frontmatter = false;
            frontmatter_done = true;
            continue;
        }

        if (in_frontmatter) {
            auto colon_pos = line.find(':');
            if (colon_pos != std::string::npos) {
                std::string key = line.substr(0, colon_pos);
                std::string value = line.substr(colon_pos + 1);

                // Trim whitespace
                auto trim = [](std::string& s) {
                    size_t start = s.find_first_not_of(" \t");
                    size_t end = s.find_last_not_of(" \t");
                    if (start == std::string::npos) {
                        s.clear();
                    } else {
                        s = s.substr(start, end - start + 1);
                    }
                };
                trim(key);
                trim(value);
                result.fields[key] = value;
            }
        } else {
            body << line << '\n';
        }
    }

    result.body = body.str();
    return result;
}

auto parse_skill_file(const std::filesystem::path& path) -> std::optional<Frontmatter> {
    std::ifstream file(path);
    if (!file.is_open()) {
        return std::nullopt;
    }
    std::ostringstream ss;
    ss << file.rdbuf();
    return parse_frontmatter(ss.str());
}

}  // namespace euxis::bridge
