#include <gtest/gtest.h>

#include <string>
#include <unordered_set>

#include "euxis/taint/builtin_specs.hpp"
#include "euxis/taint/spec.hpp"

namespace euxis::taint {
namespace {

using L = euxis::parse::Language;

TEST(AppliesTo, EmptyVectorIsUniversal) {
    EXPECT_TRUE(applies_to({}, L::C));
    EXPECT_TRUE(applies_to({}, L::Python));
}

TEST(AppliesTo, MatchesListedLanguage) {
    EXPECT_TRUE(applies_to({L::C, L::Cpp}, L::C));
    EXPECT_TRUE(applies_to({L::C, L::Cpp}, L::Cpp));
    EXPECT_FALSE(applies_to({L::C, L::Cpp}, L::Rust));
}

TEST(Builtin, ContainsBalancedSourceSinkSets) {
    auto t = builtin_specs();
    EXPECT_FALSE(t.sources.empty());
    EXPECT_FALSE(t.sinks.empty());
    EXPECT_FALSE(t.sanitizers.empty());
    EXPECT_EQ(t.name, "euxis-builtin");
}

TEST(Builtin, EveryWiredLanguageHasAtLeastOneSink) {
    auto t = builtin_specs();
    for (auto lang : {L::C, L::Cpp, L::Rust, L::Go, L::Python,
                      L::JavaScript, L::TypeScript, L::Java}) {
        bool saw_sink = false;
        for (const auto& sink : t.sinks) {
            if (applies_to(sink.languages, lang)) {
                saw_sink = true;
                break;
            }
        }
        EXPECT_TRUE(saw_sink)
            << "no sink spec covers language enum=" << static_cast<int>(lang);
    }
}

TEST(Builtin, CriticalSinksAreLabelled) {
    auto t = builtin_specs();
    bool saw_critical = false;
    for (const auto& s : t.sinks) {
        if (s.severity == euxis::security::Severity::Critical) {
            saw_critical = true;
            EXPECT_TRUE(s.cwe.has_value())
                << "Critical sink '" << s.id << "' should carry a CWE";
        }
    }
    EXPECT_TRUE(saw_critical);
}

TEST(Builtin, CweFormatIsCanonical) {
    auto t = builtin_specs();
    for (const auto& s : t.sinks) {
        if (!s.cwe) continue;
        // Every CWE must match the CWE-<digits> form.
        EXPECT_TRUE(s.cwe->starts_with("CWE-"))
            << "sink '" << s.id << "' has non-canonical CWE '"
            << *s.cwe << "'";
    }
}

TEST(Builtin, SourceSinkIdsAreUnique) {
    auto t = builtin_specs();
    std::unordered_set<std::string> ids;
    for (const auto& s : t.sources) {
        EXPECT_TRUE(ids.insert(s.id).second)
            << "duplicate source id: " << s.id;
    }
    for (const auto& s : t.sinks) {
        EXPECT_TRUE(ids.insert(s.id).second)
            << "duplicate sink id: " << s.id;
    }
    for (const auto& s : t.sanitizers) {
        EXPECT_TRUE(ids.insert(s.id).second)
            << "duplicate sanitizer id: " << s.id;
    }
}

} // namespace
} // namespace euxis::taint
