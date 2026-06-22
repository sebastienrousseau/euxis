// Minimal C++23 fixture: namespace, template, structured binding,
// expected. Exercises the tree-sitter-cpp grammar paths most of the
// rest of euxis itself relies on.

#include <expected>
#include <string>
#include <utility>

namespace euxis::sample {

template <typename T>
auto identity(T&& v) -> T {
    return std::forward<T>(v);
}

struct Greeting {
    std::string subject;
    int reps{1};
};

auto make_greeting(std::string subject) -> std::expected<Greeting, std::string> {
    if (subject.empty()) {
        return std::unexpected("subject required");
    }
    return Greeting{.subject = std::move(subject), .reps = 1};
}

} // namespace euxis::sample

// NOLINTNEXTLINE(bugprone-exception-escape) — parse-test fixture, never built
int main() {
    using namespace euxis::sample;
    if (auto g = make_greeting("world"); g) {
        const auto& [s, n] = *g;
        for (int i = 0; i < n; ++i) {
            (void)identity(s);
        }
    }
    return 0;
}
