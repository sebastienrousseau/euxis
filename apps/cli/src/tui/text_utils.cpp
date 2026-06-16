#include "euxis/cli/tui/text_utils.hpp"
#include <sstream>
#include <algorithm>

namespace euxis::cli::tui {

std::vector<std::string> wrap_text(std::string_view text, int width) {
    if (width <= 0) return {std::string(text)};
    
    std::vector<std::string> result;
    std::string content(text);
    std::stringstream ss(content);
    std::string raw_line;

    while (std::getline(ss, raw_line, '\n')) {
        if (raw_line.empty()) {
            result.push_back("");
            continue;
        }

        std::string current_line;
        std::string current_word;
        
        for (char c : raw_line) {
            if (c == ' ' || c == '\t') {
                // Word ended. Check if it fits.
                if (!current_word.empty()) {
                    if (current_line.empty()) {
                        current_line = current_word;
                    } else if ((int)(current_line.size() + 1 + current_word.size()) <= width) {
                        current_line += " " + current_word;
                    } else {
                        result.push_back(current_line);
                        current_line = current_word;
                    }
                    current_word.clear();
                    // NOLINTBEGIN(bugprone-branch-clone) — distinct cases (mid-line
                    // continuation vs leading-spaces) kept apart for documentation,
                    // even though both append c.
                } else if (!current_line.empty() && (int)current_line.size() < width) {
                    // Preserve consecutive spaces
                    current_line += c;
                } else if (current_line.empty()) {
                    // Leading spaces
                    current_line += c;
                    // NOLINTEND(bugprone-branch-clone)
                }
            } else {
                current_word += c;
                // Hard wrap very long words
                if ((int)current_word.size() >= width) {
                    if (!current_line.empty()) result.push_back(current_line);
                    result.push_back(current_word.substr(0, width));
                    current_word = current_word.substr(width);
                    current_line.clear();
                }
            }
        }
        
        // Finalize the last word
        if (!current_word.empty()) {
            if (current_line.empty()) {
                current_line = current_word;
            } else if ((int)(current_line.size() + 1 + current_word.size()) <= width) {
                current_line += " " + current_word;
            } else {
                result.push_back(current_line);
                current_line = current_word;
            }
        }
        
        if (!current_line.empty() || raw_line.empty()) {
            result.push_back(current_line);
        }
    }

    if (text.ends_with('\n')) result.push_back("");
    if (result.empty()) result.push_back("");
    
    return result;
}

} // namespace euxis::cli::tui
