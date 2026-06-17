#include <gtest/gtest.h>

#include <fstream>

#include "euxis/slopsquatting/guard.hpp"

namespace euxis::slopsquatting {
namespace {

using euxis::sbom::PurlType;

TEST(Guard, EmptyByDefault) {
    Guard g;
    EXPECT_EQ(g.size(), 0U);
    EXPECT_FALSE(g.contains("anything", PurlType::Npm));
}

TEST(Guard, AddAndContains) {
    Guard g;
    g.add(PurlType::Npm, {"lodash-utils", "easy-fetch"});
    EXPECT_TRUE(g.contains("lodash-utils", PurlType::Npm));
    EXPECT_TRUE(g.contains("easy-fetch",   PurlType::Npm));
    EXPECT_FALSE(g.contains("lodash",      PurlType::Npm));
}

TEST(Guard, EcosystemScoping) {
    Guard g;
    g.add(PurlType::Pypi,  {"requests-utils"});
    g.add(PurlType::Cargo, {"tokio-utils"});

    EXPECT_TRUE(g.contains("requests-utils",  PurlType::Pypi));
    EXPECT_FALSE(g.contains("requests-utils", PurlType::Cargo));
    EXPECT_TRUE(g.contains("tokio-utils",     PurlType::Cargo));
    EXPECT_FALSE(g.contains("tokio-utils",    PurlType::Pypi));
}

TEST(Guard, PypiCaseInsensitiveAndDotNormalised) {
    // PEP 503 — `Discord.PY`, `discord-py`, `discord_py`, `DISCORD.PY`
    // all normalise to `discord-py`.
    Guard g;
    g.add(PurlType::Pypi, {"discord-py"});
    EXPECT_TRUE(g.contains("Discord.PY", PurlType::Pypi));
    EXPECT_TRUE(g.contains("discord_py", PurlType::Pypi));
    EXPECT_TRUE(g.contains("DISCORD.PY", PurlType::Pypi));
    EXPECT_TRUE(g.contains("discord-py", PurlType::Pypi));
}

TEST(Guard, NpmCaseSensitive) {
    // npm registry IS case-sensitive in name lookups.
    Guard g;
    g.add(PurlType::Npm, {"easy-fetch"});
    EXPECT_TRUE(g.contains("easy-fetch", PurlType::Npm));
    EXPECT_FALSE(g.contains("EASY-FETCH", PurlType::Npm));
}

TEST(Guard, LoadSeedNonEmpty) {
    Guard g;
    auto n = g.load_seed();
    EXPECT_GT(n, 50U);
    EXPECT_GT(g.size(), 50U);

    // Spot-check a handful of expected entries from the seed.
    EXPECT_TRUE(g.contains("urlib3",          PurlType::Pypi));
    EXPECT_TRUE(g.contains("python-dateutils",PurlType::Pypi));
    EXPECT_TRUE(g.contains("easy-fetch",      PurlType::Npm));
    EXPECT_TRUE(g.contains("tokio-utils",     PurlType::Cargo));
    EXPECT_TRUE(g.contains("github.com/sirupsen/logrus-v2", PurlType::Golang));
}

TEST(Guard, LoadFromStringFile) {
    // Round-trip via a temp file.
    Guard g;
    auto tmp = std::filesystem::temp_directory_path() / "euxis-slopsq-test.txt";
    {
        std::ofstream f(tmp);
        f << "# header\n"
             "npm: my-fake = real-pkg\n"
             "pypi: another-fake\n"
             "\n"
             "generic: free-floating\n";
    }
    auto n = g.load_corpus_file(tmp);
    ASSERT_TRUE(n.has_value()) << (n ? "" : n.error().message);
    EXPECT_EQ(*n, 3U);
    EXPECT_TRUE(g.contains("my-fake",        PurlType::Npm));
    EXPECT_TRUE(g.contains("another-fake",   PurlType::Pypi));
    EXPECT_TRUE(g.contains("free-floating",  PurlType::Generic));
    std::error_code ec;
    std::filesystem::remove(tmp, ec);
}

TEST(Guard, LoadCorpusMissingFile) {
    Guard g;
    auto n = g.load_corpus_file("/tmp/euxis-this-cannot-exist-zzz");
    EXPECT_FALSE(n.has_value());
}

TEST(Guard, AddWithNearestRecordsHint) {
    // The nearest hint is stored even if it isn't surfaced by
    // contains(); the integration layer reads it back through
    // its own pathway. Smoke-test the API.
    Guard g;
    g.add_with_nearest(PurlType::Npm, "easy-fetch", "node-fetch");
    EXPECT_TRUE(g.contains("easy-fetch", PurlType::Npm));
}

} // namespace
} // namespace euxis::slopsquatting
