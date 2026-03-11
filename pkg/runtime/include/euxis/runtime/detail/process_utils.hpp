#pragma once

#include <string>
#include <string_view>
#include <array>
#include <cstdio>

namespace euxis::runtime::detail {

// Internal helper for capturing command output
inline std::string run_command(const std::string& cmd) {
    std::array<char, 4096> buffer{};
    std::string result;
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return {};
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe)) {
        result += buffer.data();
    }
    pclose(pipe);
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r'))
        result.pop_back();
    return result;
}

// Internal helper for piping to a command's stdin
inline bool run_pipe_command(const std::string& cmd, std::string_view input) {
    FILE* pipe = popen(cmd.c_str(), "w");
    if (!pipe) return false;
    fwrite(input.data(), 1, input.size(), pipe);
    return pclose(pipe) == 0;
}

} // namespace euxis::runtime::detail
