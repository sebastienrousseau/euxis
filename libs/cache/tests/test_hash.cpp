#include <gtest/gtest.h>

#include "euxis/cache/hash.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <span>

namespace euxis::cache {
namespace {

namespace fs = std::filesystem;

struct TmpFile {
    fs::path path;
    TmpFile(std::string_view name, std::string_view content) {
        path = fs::temp_directory_path() /
            ("euxis-cache-test-" +
             std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
             "-" + std::string{name});
        std::ofstream f(path, std::ios::binary);
        f.write(content.data(), static_cast<std::streamsize>(content.size()));
    }
    ~TmpFile() { std::error_code ec; fs::remove(path, ec); }
};

TEST(Hash, EmptyStringHasKnownLength) {
    auto h = hash_string("");
    EXPECT_EQ(h.size(), kDigestHexLen);
    EXPECT_TRUE(is_valid_digest(h));
}

TEST(Hash, IsDeterministic) {
    EXPECT_EQ(hash_string("hello"), hash_string("hello"));
    EXPECT_NE(hash_string("hello"), hash_string("world"));
}

TEST(Hash, IsValidDigestRejectsMalformed) {
    EXPECT_FALSE(is_valid_digest(""));
    EXPECT_FALSE(is_valid_digest("abc"));
    EXPECT_FALSE(is_valid_digest(std::string(63, 'a')));
    EXPECT_FALSE(is_valid_digest(std::string(64, 'z')));
    EXPECT_TRUE(is_valid_digest(std::string(64, 'a')));
    EXPECT_TRUE(is_valid_digest(std::string(64, '0')));
}

TEST(Hash, HashBytesMatchesHashString) {
    std::string s = "the quick brown fox";
    auto via_str   = hash_string(s);
    auto via_bytes = hash_bytes(std::span<const std::byte>(
        reinterpret_cast<const std::byte*>(s.data()), s.size()));
    EXPECT_EQ(via_str, via_bytes);
}

TEST(Hash, FileMatchesString) {
    TmpFile f{"x.txt", "the quick brown fox"};
    auto h_file = hash_file(f.path);
    ASSERT_TRUE(h_file.has_value()) << (h_file ? "" : h_file.error().message);
    EXPECT_EQ(*h_file, hash_string("the quick brown fox"));
}

TEST(Hash, FileMissingReturnsError) {
    auto h = hash_file("/tmp/euxis-this-cannot-exist-1234567");
    EXPECT_FALSE(h.has_value());
}

TEST(Hash, FileLargerThanChunkBuffer) {
    std::string big(200 * 1024, 'A');  // 200 KiB, > 64 KiB chunk
    TmpFile f{"big.bin", big};
    auto h_file = hash_file(f.path);
    ASSERT_TRUE(h_file.has_value());
    EXPECT_EQ(*h_file, hash_string(big));
}

TEST(Hasher, IncrementalEqualsOneShot) {
    Hasher h;
    h.update(std::string_view{"the "});
    h.update(std::string_view{"quick "});
    h.update(std::string_view{"brown fox"});
    auto incr = h.finalize();
    EXPECT_EQ(incr, hash_string("the quick brown fox"));
}

TEST(Hasher, MoveConstructor) {
    Hasher a;
    a.update(std::string_view{"hello"});
    Hasher b{std::move(a)};
    EXPECT_EQ(b.finalize(), hash_string("hello"));
}

} // namespace
} // namespace euxis::cache
