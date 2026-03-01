#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>

#include "euxis/scripts/docs_quality.hpp"

namespace euxis::scripts {
namespace {

class DocsQualityTest : public ::testing::Test {
protected:
    std::filesystem::path tmp_dir_;

    void SetUp() override {
        tmp_dir_ = std::filesystem::temp_directory_path() / "euxis_docs_test";
        std::filesystem::create_directories(tmp_dir_ / "docs" / "modules");
    }

    void TearDown() override {
        std::filesystem::remove_all(tmp_dir_);
    }

    void write_file(const std::filesystem::path& path, const std::string& content) {
        std::filesystem::create_directories(path.parent_path());
        std::ofstream f(path);
        f << content;
    }
};

TEST_F(DocsQualityTest, FindsDocFiles) {
    write_file(tmp_dir_ / "docs" / "readme.md", "# Hello");
    write_file(tmp_dir_ / "docs" / "config.yaml", "key: value");
    auto files = docs_source_files(tmp_dir_);
    EXPECT_EQ(files.size(), 2);
}

TEST_F(DocsQualityTest, EmptyDocsDir) {
    auto empty = std::filesystem::temp_directory_path() / "euxis_nodocs";
    std::filesystem::create_directories(empty);
    auto files = docs_source_files(empty);
    EXPECT_TRUE(files.empty());
    std::filesystem::remove_all(empty);
}

TEST_F(DocsQualityTest, ModuleDocPath) {
    EXPECT_EQ(module_doc_path("euxis-core").string(), "docs/modules/euxis-core.md");
    EXPECT_EQ(module_doc_path("core").string(), "docs/modules/euxis-core.md");
    EXPECT_EQ(module_doc_path("  CORE  ").string(), "docs/modules/euxis-core.md");
}

TEST_F(DocsQualityTest, MissingModuleDocs) {
    write_file(tmp_dir_ / "docs" / "modules" / "euxis-core.md", "# Core");
    auto missing = missing_module_docs(tmp_dir_, {"euxis-core", "euxis-cli"});
    EXPECT_EQ(missing.size(), 1);
    EXPECT_EQ(missing[0], "euxis-cli");
}

TEST_F(DocsQualityTest, QualityReport) {
    write_file(tmp_dir_ / "README.md", "# Euxis");
    write_file(tmp_dir_ / "docs" / "modules" / "euxis-core.md", "# Core");
    auto report = build_docs_quality_report(tmp_dir_, {"euxis-core"});
    EXPECT_FALSE(report["ok"].get<bool>());  // missing index.rst etc
}

} // namespace
} // namespace euxis::scripts
