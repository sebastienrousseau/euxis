#include "euxis/cli/certification_detail.hpp"

#include <algorithm>
#include <system_error>
#include <unistd.h>

namespace euxis::cli::certification::detail {

namespace {

// True when `real` is `base` or below it; both must be canonical.
auto is_under(const fs::path& base, const fs::path& real) -> bool {
    return std::mismatch(base.begin(), base.end(), real.begin(), real.end()).first == base.end();
}

}  // namespace

auto resolve_target_file(const fs::path& root, const fs::path& p) -> std::optional<fs::path> {
    std::error_code ec;
    const auto base = fs::canonical(root, ec);
    if (ec)
        return std::nullopt;
    auto real = fs::canonical(root / p, ec);
    if (ec || !fs::is_regular_file(real, ec) || !is_under(base, real))
        return std::nullopt;
    return real;
}

auto read_target_file(const fs::path& root, const fs::path& p) -> std::string {
    const auto real = resolve_target_file(root, p);
    return real ? read_file_content(*real) : std::string{};
}

auto is_target_executable(const fs::path& root, const fs::path& p) -> bool {
    const auto real = resolve_target_file(root, p);
    return real && ::access(real->c_str(), X_OK) == 0;
}

}  // namespace euxis::cli::certification::detail
