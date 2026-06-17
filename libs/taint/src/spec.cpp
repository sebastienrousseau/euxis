#include "euxis/taint/spec.hpp"

#include <algorithm>

namespace euxis::taint {

auto applies_to(const std::vector<euxis::parse::Language>& langs,
                euxis::parse::Language lang) noexcept -> bool {
    if (langs.empty()) return true;
    return std::find(langs.begin(), langs.end(), lang) != langs.end();
}

} // namespace euxis::taint
