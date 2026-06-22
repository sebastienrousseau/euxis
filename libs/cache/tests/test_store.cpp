#include <gtest/gtest.h>

#include "euxis/cache/hash.hpp"
#include "euxis/cache/store.hpp"

#include <chrono>
#include <filesystem>

namespace euxis::cache {
namespace {

namespace fs = std::filesystem;

struct TmpDir {
    fs::path path;
    TmpDir() {
        path = fs::temp_directory_path() /
            ("euxis-cache-store-" +
             std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        fs::create_directories(path);
    }
    ~TmpDir() { std::error_code ec; fs::remove_all(path, ec); }
};

KeyInputs inputs_for(const fs::path& file, std::string content,
                     std::string ruleset = "ruleset-default") {
    return KeyInputs{
        .file         = file,
        .content_hash = hash_string(content),
        .ruleset_hash = hash_string(ruleset),
        .tool_version = "0.1.0-test",
    };
}

TEST(Store, OpenCreatesDirectoryAndDatabase) {
    TmpDir dir;
    auto path = dir.path / "nested" / "cache.sqlite";
    auto cache = ScanCache::open(path);
    ASSERT_TRUE(cache.has_value());
    EXPECT_TRUE(fs::exists(path));
}

TEST(Store, GetReturnsNulloptForMissingKey) {
    TmpDir dir;
    auto cache = ScanCache::open(dir.path / "c.sqlite");
    ASSERT_TRUE(cache.has_value());
    auto got = cache->get("does-not-exist");
    ASSERT_TRUE(got.has_value());
    EXPECT_FALSE(got->has_value());
}

TEST(Store, PutThenGetRoundTrips) {
    TmpDir dir;
    auto cache = ScanCache::open(dir.path / "c.sqlite");
    ASSERT_TRUE(cache.has_value());

    auto k = inputs_for("/tmp/x.cpp", "void main(){}");
    CacheEntry e;
    e.findings_json   = R"({"findings":[],"version":1})";
    e.created_at_unix = 1700000000;
    auto put = cache->put(k, e);
    ASSERT_TRUE(put.has_value());

    auto got = cache->get(compose_key(k));
    ASSERT_TRUE(got.has_value());
    ASSERT_TRUE(got->has_value());
    EXPECT_EQ((*got)->findings_json,   e.findings_json);
    EXPECT_EQ((*got)->created_at_unix, e.created_at_unix);
    EXPECT_GT((*got)->size_bytes,      0);
}

TEST(Store, PutOverwritesSameKey) {
    TmpDir dir;
    auto cache = ScanCache::open(dir.path / "c.sqlite");
    ASSERT_TRUE(cache.has_value());

    auto k = inputs_for("/tmp/x.cpp", "v1");
    cache->put(k, CacheEntry{.findings_json = "first"});
    cache->put(k, CacheEntry{.findings_json = "second"});

    auto got = cache->get(compose_key(k));
    ASSERT_TRUE(got.has_value() && got->has_value());
    EXPECT_EQ((*got)->findings_json, "second");
}

TEST(Store, DifferentContentHashGivesDifferentKey) {
    TmpDir dir;
    auto cache = ScanCache::open(dir.path / "c.sqlite");
    ASSERT_TRUE(cache.has_value());

    auto k1 = inputs_for("/tmp/x.cpp", "v1");
    auto k2 = inputs_for("/tmp/x.cpp", "v2");
    EXPECT_NE(compose_key(k1), compose_key(k2));

    cache->put(k1, CacheEntry{.findings_json = "first"});
    cache->put(k2, CacheEntry{.findings_json = "second"});

    auto got1 = cache->get(compose_key(k1));
    auto got2 = cache->get(compose_key(k2));
    ASSERT_TRUE(got1.has_value() && got1->has_value());
    ASSERT_TRUE(got2.has_value() && got2->has_value());
    EXPECT_EQ((*got1)->findings_json, "first");
    EXPECT_EQ((*got2)->findings_json, "second");
}

TEST(Store, DifferentRulesetGivesDifferentKey) {
    TmpDir dir;
    auto cache = ScanCache::open(dir.path / "c.sqlite");
    ASSERT_TRUE(cache.has_value());

    auto k1 = inputs_for("/tmp/x.cpp", "v1", "rules-a");
    auto k2 = inputs_for("/tmp/x.cpp", "v1", "rules-b");
    EXPECT_NE(compose_key(k1), compose_key(k2));
}

TEST(Store, PurgeRemovesAllEntries) {
    TmpDir dir;
    auto cache = ScanCache::open(dir.path / "c.sqlite");
    ASSERT_TRUE(cache.has_value());

    cache->put(inputs_for("/a", "1"), CacheEntry{.findings_json = "x"});
    cache->put(inputs_for("/b", "2"), CacheEntry{.findings_json = "y"});

    auto removed = cache->purge();
    ASSERT_TRUE(removed.has_value());
    EXPECT_EQ(*removed, 2);

    auto st = cache->stats();
    ASSERT_TRUE(st.has_value());
    EXPECT_EQ(st->entry_count, 0);
}

TEST(Store, PurgeOlderThanRemovesOnlyExpired) {
    TmpDir dir;
    auto cache = ScanCache::open(dir.path / "c.sqlite");
    ASSERT_TRUE(cache.has_value());

    constexpr std::int64_t now = 2'000'000'000;
    constexpr std::int64_t old = 1'000'000'000;

    cache->put(inputs_for("/old", "1"),  CacheEntry{.findings_json = "stale", .created_at_unix = old});
    cache->put(inputs_for("/new", "2"),  CacheEntry{.findings_json = "fresh", .created_at_unix = now});

    auto purged = cache->purge_older_than(/*ttl*/ std::int64_t{30} * 24 * 3600, now);
    ASSERT_TRUE(purged.has_value());
    EXPECT_EQ(*purged, 1);

    auto st = cache->stats();
    ASSERT_TRUE(st.has_value());
    EXPECT_EQ(st->entry_count, 1);
}

TEST(Store, StatsReflectInsertedEntries) {
    TmpDir dir;
    auto cache = ScanCache::open(dir.path / "c.sqlite");
    ASSERT_TRUE(cache.has_value());

    cache->put(inputs_for("/a", "1"), CacheEntry{.findings_json = "abcd"});
    cache->put(inputs_for("/b", "2"), CacheEntry{.findings_json = "efgh"});

    auto st = cache->stats();
    ASSERT_TRUE(st.has_value());
    EXPECT_EQ(st->entry_count,      2);
    EXPECT_GE(st->total_size_bytes, 8);
}

TEST(Store, MoveConstructorPreservesHandle) {
    TmpDir dir;
    auto cache = ScanCache::open(dir.path / "c.sqlite");
    ASSERT_TRUE(cache.has_value());
    ScanCache moved = std::move(*cache);
    auto st = moved.stats();
    ASSERT_TRUE(st.has_value());
    EXPECT_EQ(st->entry_count, 0);
}

} // namespace
} // namespace euxis::cache
