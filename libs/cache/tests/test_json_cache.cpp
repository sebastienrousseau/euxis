#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>

#include "euxis/cache/json_cache.hpp"

namespace euxis::cache {
namespace {

namespace fs = std::filesystem;

struct TmpDir {
    fs::path path;
    TmpDir() {
        path = fs::temp_directory_path() /
            ("euxis-jsoncache-test-" +
             std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        fs::create_directories(path);
    }
    ~TmpDir() { std::error_code ec; fs::remove_all(path, ec); }
};

void write_json(const fs::path& p, const nlohmann::json& content) {
    fs::create_directories(p.parent_path());
    std::ofstream f(p);
    f << content.dump();
}

TEST(JsonCache, OpenCreatesParentDirectory) {
    TmpDir d;
    auto db = d.path / "nested" / "deeper" / "cache.sqlite";
    auto cache = JsonCache::open(db);
    ASSERT_TRUE(cache.has_value()) << (cache ? "" : cache.error().message);
    EXPECT_TRUE(fs::exists(db));
}

TEST(JsonCache, MissReturnsNullopt) {
    TmpDir d;
    auto cache = JsonCache::open(d.path / "c.sqlite").value();
    auto got = cache.get(d.path / "does-not-exist.json");
    ASSERT_TRUE(got.has_value());
    EXPECT_FALSE(got->has_value());
}

TEST(JsonCache, PutThenGetRoundTripsParsedJson) {
    TmpDir d;
    auto cache = JsonCache::open(d.path / "c.sqlite").value();
    auto file = d.path / "data.json";
    nlohmann::json doc = {{"a", 1}, {"b", "x"}, {"c", {1, 2, 3}}};
    write_json(file, doc);

    auto put = cache.put(file, doc);
    ASSERT_TRUE(put.has_value()) << (put ? "" : put.error().message);

    auto got = cache.get(file);
    ASSERT_TRUE(got.has_value());
    ASSERT_TRUE(got->has_value());
    EXPECT_EQ(**got, doc);
}

TEST(JsonCache, ContentEditInvalidatesEntry) {
    TmpDir d;
    auto cache = JsonCache::open(d.path / "c.sqlite").value();
    auto file = d.path / "data.json";

    nlohmann::json v1 = {{"v", 1}};
    write_json(file, v1);
    cache.put(file, v1).value();

    nlohmann::json v2 = {{"v", 2}};
    write_json(file, v2);

    // Same path, different content → cache miss (hash mismatch).
    auto got = cache.get(file);
    ASSERT_TRUE(got.has_value());
    EXPECT_FALSE(got->has_value());
}

TEST(JsonCache, GetOrLoadParsesOnMissAndCachesOnHit) {
    TmpDir d;
    auto cache = JsonCache::open(d.path / "c.sqlite").value();
    auto file = d.path / "data.json";

    nlohmann::json doc = {{"k", "v"}};
    write_json(file, doc);

    auto miss = cache.get_or_load(file);
    ASSERT_TRUE(miss.has_value());
    EXPECT_EQ(*miss, doc);

    auto hit = cache.get_or_load(file);
    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ(*hit, doc);

    // Second call should have been a hit.
    EXPECT_GE(cache.stats().hits,   1U);
    EXPECT_GE(cache.stats().misses, 1U);
}

TEST(JsonCache, PurgeRemovesAllEntries) {
    TmpDir d;
    auto cache = JsonCache::open(d.path / "c.sqlite").value();
    auto a = d.path / "a.json";
    auto b = d.path / "b.json";
    write_json(a, nlohmann::json::object());
    write_json(b, nlohmann::json::object());
    cache.put(a, nlohmann::json::object()).value();
    cache.put(b, nlohmann::json::object()).value();

    auto removed = cache.purge();
    ASSERT_TRUE(removed.has_value());
    EXPECT_GE(*removed, 2U);

    auto got = cache.get(a);
    ASSERT_TRUE(got.has_value());
    EXPECT_FALSE(got->has_value());
}

TEST(JsonCache, NonexistentSourceFileIsMiss) {
    TmpDir d;
    auto cache = JsonCache::open(d.path / "c.sqlite").value();
    auto got = cache.get(d.path / "never-existed.json");
    ASSERT_TRUE(got.has_value());
    EXPECT_FALSE(got->has_value());
}

TEST(JsonCache, MoveConstructorPreservesHandle) {
    TmpDir d;
    auto cache = JsonCache::open(d.path / "c.sqlite").value();
    JsonCache moved = std::move(cache);
    auto got = moved.get(d.path / "missing");
    ASSERT_TRUE(got.has_value());
    EXPECT_FALSE(got->has_value());
}

} // namespace
} // namespace euxis::cache
