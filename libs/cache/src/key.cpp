#include "euxis/cache/key.hpp"

#include "euxis/cache/hash.hpp"

namespace euxis::cache {

auto compose_key(const KeyInputs& inputs) -> std::string {
    Hasher h;
    // NUL separators so adjacent fields cannot smear into each other
    // — e.g. ruleset_hash="X" + tool_version="Y" must not hash to the
    // same key as ruleset_hash="XY" + tool_version="".
    h.update(inputs.file.string());
    h.update(std::string_view{"\0", 1});
    h.update(inputs.content_hash);
    h.update(std::string_view{"\0", 1});
    h.update(inputs.ruleset_hash);
    h.update(std::string_view{"\0", 1});
    h.update(inputs.tool_version);
    return h.finalize();
}

} // namespace euxis::cache
