#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>

#include "euxis/scripts/tech_debt.hpp"

namespace euxis::scripts {
namespace {

class TechDebtTest : public ::testing::Test {
protected:
    std::filesystem::path tmp_dir_;

    void SetUp() override {
        tmp_dir_ = std::filesystem::temp_directory_path() / "euxis_debt_test";
        std::filesystem::create_directories(tmp_dir_ / "euxis-core" / "src");
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

TEST_F(TechDebtTest, CountsPragmasAndTodos) {
    write_file(tmp_dir_ / "euxis-core" / "src" / "main.py",
               "x = 1  # pragma: no cover\n# TODO fix this\n");
    auto report = check_tech_debt(tmp_dir_);
    EXPECT_EQ(report.pragma_count, 1);
    EXPECT_EQ(report.todo_count, 1);
    EXPECT_TRUE(report.ok);
}

TEST_F(TechDebtTest, EmptyRepoReportsZero) {
    auto empty = std::filesystem::temp_directory_path() / "euxis_empty_test";
    std::filesystem::create_directories(empty);
    auto report = check_tech_debt(empty);
    EXPECT_EQ(report.pragma_count, 0);
    EXPECT_EQ(report.todo_count, 0);
    EXPECT_TRUE(report.ok);
    std::filesystem::remove_all(empty);
}

TEST_F(TechDebtTest, ReportToJson) {
    DebtReport r{.pragma_count = 5, .todo_count = 2, .ok = true};
    auto j = r.to_json();
    EXPECT_EQ(j["pragma_count"], 5);
    EXPECT_EQ(j["todo_count"], 2);
    EXPECT_TRUE(j["ok"].get<bool>());
}

} // namespace
} // namespace euxis::scripts
