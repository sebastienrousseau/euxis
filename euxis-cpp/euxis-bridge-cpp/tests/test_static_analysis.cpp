#include <gtest/gtest.h>
#include <euxis/bridge/static_analysis.hpp>

#include <filesystem>
#include <fstream>

namespace euxis::bridge {

class StaticAnalysisTest : public ::testing::Test {
protected:
    std::filesystem::path tmp_dir_;

    void SetUp() override {
        tmp_dir_ = std::filesystem::temp_directory_path() / "euxis_test_sa";
        std::filesystem::create_directories(tmp_dir_);
    }

    void TearDown() override {
        std::filesystem::remove_all(tmp_dir_);
    }

    void write_file(const std::string& name, const std::string& content) {
        std::ofstream f(tmp_dir_ / name);
        f << content;
        f.close();
    }
};

TEST_F(StaticAnalysisTest, DetectEval) {
    write_file("bad.js", "const result = eval('1+1');\n");

    SkillStaticAnalyzer analyzer;
    auto findings = analyzer.analyze_file(tmp_dir_ / "bad.js");

    EXPECT_FALSE(findings.empty());
    EXPECT_EQ(findings[0].severity, Severity::Critical);
    EXPECT_EQ(findings[0].pattern, "eval");
}

TEST_F(StaticAnalysisTest, DetectSubprocess) {
    write_file("cmd.py", "import subprocess\nsubprocess.run(['ls'])\n");

    SkillStaticAnalyzer analyzer;
    auto findings = analyzer.analyze_file(tmp_dir_ / "cmd.py");

    bool found = false;
    for (const auto& f : findings) {
        if (f.pattern == "subprocess") found = true;
    }
    EXPECT_TRUE(found);
}

TEST_F(StaticAnalysisTest, DetectOsSystem) {
    write_file("os_cmd.py", "import os\nos.system('rm -rf /')\n");

    SkillStaticAnalyzer analyzer;
    auto findings = analyzer.analyze_file(tmp_dir_ / "os_cmd.py");

    bool found = false;
    for (const auto& f : findings) {
        if (f.pattern == "os.system") found = true;
    }
    EXPECT_TRUE(found);
}

TEST_F(StaticAnalysisTest, CleanFile) {
    write_file("clean.js", "function add(a, b) { return a + b; }\n");

    SkillStaticAnalyzer analyzer;
    auto findings = analyzer.analyze_file(tmp_dir_ / "clean.js");
    EXPECT_TRUE(findings.empty());
}

TEST_F(StaticAnalysisTest, AnalyzeDirectory) {
    write_file("safe.js", "console.log('hello');\n");
    write_file("danger.js", "eval('code');\nos.system('hack');\n");

    SkillStaticAnalyzer analyzer;
    auto report = analyzer.analyze_directory(tmp_dir_);

    EXPECT_EQ(report.scanned_files, 2u);
    EXPECT_FALSE(report.passed);  // Has critical findings
    EXPECT_FALSE(report.findings.empty());
}

TEST_F(StaticAnalysisTest, AnalyzeCleanDirectory) {
    write_file("a.js", "const x = 1;\n");
    write_file("b.py", "x = 1\n");

    SkillStaticAnalyzer analyzer;
    auto report = analyzer.analyze_directory(tmp_dir_);

    EXPECT_EQ(report.scanned_files, 2u);
    EXPECT_TRUE(report.passed);
}

TEST_F(StaticAnalysisTest, SkipNonAnalyzableFiles) {
    write_file("readme.md", "# Hello\n");
    write_file("data.json", "{}\n");

    SkillStaticAnalyzer analyzer;
    auto report = analyzer.analyze_directory(tmp_dir_);
    EXPECT_EQ(report.scanned_files, 0u);
}

TEST_F(StaticAnalysisTest, DetectChildProcess) {
    write_file("cp.js", "const cp = require('child_process');\n");

    SkillStaticAnalyzer analyzer;
    auto findings = analyzer.analyze_file(tmp_dir_ / "cp.js");

    bool found = false;
    for (const auto& f : findings) {
        if (f.severity == Severity::Critical) found = true;
    }
    EXPECT_TRUE(found);
}

}  // namespace euxis::bridge
