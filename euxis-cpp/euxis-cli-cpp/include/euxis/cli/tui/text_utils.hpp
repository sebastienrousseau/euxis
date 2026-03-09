#pragma once

#include <string>
#include <vector>
#include <string_view>

namespace euxis::cli::tui {

/**
 * @brief High-performance word wrapping utility for terminal interfaces.
 * 
 * Splits a text string into a list of lines that fit within the specified width.
 * Faithfully preserves original newlines and multiple consecutive spaces to 
 * maintain Markdown formatting.
 * 
 * @param text The input string to wrap.
 * @param width Maximum number of columns per line.
 * @return std::vector<std::string> List of wrapped lines.
 */
std::vector<std::string> wrap_text(std::string_view text, int width);

} // namespace euxis::cli::tui
