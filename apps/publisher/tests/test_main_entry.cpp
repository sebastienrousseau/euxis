#include "euxis/publisher/main_entry.hpp"

#include <gtest/gtest.h>

#include <sstream>
#include <string>
#include <vector>

namespace euxis::publisher {

namespace {

std::vector<char*> make_argv(std::vector<std::string>& storage) {
    std::vector<char*> argv;
    argv.reserve(storage.size() + 1);
    for (auto& s : storage) argv.push_back(s.data());
    argv.push_back(nullptr);
    return argv;
}

} // namespace

TEST(PublisherMainEntry, NoArgsPrintsUsage) {
    std::vector<std::string> args{"euxis-publisher"};
    auto argv = make_argv(args);
    std::ostringstream out, err;
    int rc = publisher_main(static_cast<int>(args.size()), argv.data(), out, err);
    EXPECT_EQ(rc, 1);
    EXPECT_NE(out.str().find("Usage:"), std::string::npos);
}

TEST(PublisherMainEntry, MissingDocPrintsError) {
    std::vector<std::string> args{"euxis-publisher", "render"};
    auto argv = make_argv(args);
    std::ostringstream out, err;
    int rc = publisher_main(static_cast<int>(args.size()), argv.data(), out, err);
    EXPECT_EQ(rc, 1);
    EXPECT_NE(err.str().find("--doc"), std::string::npos);
}

TEST(PublisherMainEntry, UnknownCommandPrintsUsage) {
    std::vector<std::string> args{"euxis-publisher", "frobnicate", "--doc", "x"};
    auto argv = make_argv(args);
    std::ostringstream out, err;
    int rc = publisher_main(static_cast<int>(args.size()), argv.data(), out, err);
    EXPECT_EQ(rc, 1);
    EXPECT_NE(out.str().find("Usage:"), std::string::npos);
}

TEST(PublisherMainEntry, RenderMissingDocIdReportsError) {
    // `render --format json` without `--doc` should still hit the
    // required-arg check after parsing.
    std::vector<std::string> args{"euxis-publisher", "render", "--format", "json"};
    auto argv = make_argv(args);
    std::ostringstream out, err;
    int rc = publisher_main(static_cast<int>(args.size()), argv.data(), out, err);
    EXPECT_EQ(rc, 1);
    EXPECT_NE(err.str().find("--doc"), std::string::npos);
}

TEST(PublisherMainEntry, RenderWithMissingDocReturnsError) {
    // doc id provided but no matching content under cwd → render() fails.
    std::vector<std::string> args{"euxis-publisher", "render", "--doc", "nonexistent-doc-id-xyz"};
    auto argv = make_argv(args);
    std::ostringstream out, err;
    int rc = publisher_main(static_cast<int>(args.size()), argv.data(), out, err);
    EXPECT_EQ(rc, 1);
    EXPECT_NE(err.str().find("ERROR"), std::string::npos);
}

TEST(PublisherMainEntry, BuildModeParsing) {
    // mode parsing reachable; we just confirm we hit the build branch with
    // an error from the publisher (no content), not a parser error.
    for (const char* mode : {"draft", "submission", "camera-ready"}) {
        std::vector<std::string> args{"euxis-publisher", "build", "--doc", "no-doc", "--mode", mode};
        auto argv = make_argv(args);
        std::ostringstream out, err;
        int rc = publisher_main(static_cast<int>(args.size()), argv.data(), out, err);
        EXPECT_EQ(rc, 1);
        EXPECT_NE(err.str().find("ERROR"), std::string::npos);
    }
}

TEST(PublisherMainEntry, RenderFormatParsing) {
    for (const char* fmt : {"latex", "json"}) {
        std::vector<std::string> args{"euxis-publisher", "render", "--doc", "no-doc", "--format", fmt};
        auto argv = make_argv(args);
        std::ostringstream out, err;
        int rc = publisher_main(static_cast<int>(args.size()), argv.data(), out, err);
        EXPECT_EQ(rc, 1);
    }
}

} // namespace euxis::publisher
