#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>

#include "euxis/runtime/validator.hpp"

namespace euxis::runtime {
namespace {

class ValidatorTest : public ::testing::Test {
protected:
    std::filesystem::path tmp_;

    void SetUp() override {
        tmp_ = std::filesystem::temp_directory_path() / "euxis_rt_test";
        std::filesystem::remove_all(tmp_);
        std::filesystem::create_directories(tmp_ / "config");
        std::filesystem::create_directories(tmp_ / "data" / "perf");
        std::filesystem::create_directories(tmp_ / "data" / "lifecycle");
        std::filesystem::create_directories(tmp_ / "data" / "manifests");

        write("README.md", "# Runtime");
        write("config/etx-settings.json", "{}");
        write("data/perf/metrics.jsonl",
              R"({"ts":"2026-01-01","op":"boot","agent":"a1","ms":42})" "\n");
        write("data/lifecycle/transitions.jsonl",
              R"({"ts":"2026-01-01","agent":"a1","state":"ready","session":"s1"})" "\n");
        write("data/lifecycle/agent.state", "running");
        write("data/manifests/performance-optimization.json",
              R"({"version": 1})");
    }

    void TearDown() override { std::filesystem::remove_all(tmp_); }

    void write(const std::string& rel, const std::string& content) {
        auto path = tmp_ / rel;
        std::filesystem::create_directories(path.parent_path());
        std::ofstream f(path);
        f << content;
    }
};

TEST_F(ValidatorTest, ValidLayoutPasses) {
    EXPECT_NO_THROW(validate_runtime_layout(tmp_));
}

TEST_F(ValidatorTest, MissingFileThrows) {
    std::filesystem::remove(tmp_ / "README.md");
    EXPECT_THROW(validate_runtime_layout(tmp_), RuntimeValidationError);
}

TEST_F(ValidatorTest, InvalidJsonlThrows) {
    write("data/perf/metrics.jsonl", "not json\n");
    EXPECT_THROW(validate_runtime_layout(tmp_), RuntimeValidationError);
}

TEST_F(ValidatorTest, MissingKeysThrows) {
    write("data/perf/metrics.jsonl", R"({"ts":"2026-01-01"})" "\n");
    EXPECT_THROW(validate_runtime_layout(tmp_), RuntimeValidationError);
}

TEST_F(ValidatorTest, NegativeMsThrows) {
    write("data/perf/metrics.jsonl",
          R"({"ts":"2026-01-01","op":"boot","agent":"a1","ms":-1})" "\n");
    EXPECT_THROW(validate_runtime_layout(tmp_), RuntimeValidationError);
}

TEST_F(ValidatorTest, EmptyStateFileThrows) {
    write("data/lifecycle/agent.state", "   ");
    EXPECT_THROW(validate_runtime_layout(tmp_), RuntimeValidationError);
}

TEST_F(ValidatorTest, InvalidManifestThrows) {
    write("data/manifests/performance-optimization.json", "not json");
    EXPECT_THROW(validate_runtime_layout(tmp_), RuntimeValidationError);
}

} // namespace
} // namespace euxis::runtime
