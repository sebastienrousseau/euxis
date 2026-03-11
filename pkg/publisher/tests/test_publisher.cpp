#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include "euxis/publisher/publisher.hpp"

namespace euxis::publisher {
namespace {

class PublisherTest : public ::testing::Test {
protected:
    std::filesystem::path tmp_root_;

    void SetUp() override {
        tmp_root_ = std::filesystem::temp_directory_path() / "euxis_pub_test";
        std::filesystem::remove_all(tmp_root_);
        std::filesystem::create_directories(tmp_root_ / "data");
        std::filesystem::create_directories(tmp_root_ / "templates");

        // Write meta.yaml
        std::ofstream meta(tmp_root_ / "data" / "meta.yaml");
        meta << R"(
templates:
  test-doc:
    template: test.tex.j2
    data: test.yaml
)";
        meta.close();

        // Write data.yaml
        std::ofstream data(tmp_root_ / "data" / "test.yaml");
        data << R"(
title: 'The Modern Way'
author: 'C++23 Agent'
)";
        data.close();

        // Write template with custom delimiters
        // Correcting Inja syntax for the 'if' block
        std::ofstream tmpl(tmp_root_ / "templates" / "test.tex.j2");
        tmpl << R"(
\title{<< title >>}
\author{<< author >>}
<% if build_mode == "draft" %>
\date{\today}
<% endif %>
)";
        tmpl.close();
    }

    void TearDown() override {
        std::filesystem::remove_all(tmp_root_);
    }
};

TEST_F(PublisherTest, LoadYamlData) {
    Publisher pub(tmp_root_);
    auto res = pub.load_data(tmp_root_ / "data" / "test.yaml");
    ASSERT_TRUE(res.has_value());
    EXPECT_EQ((*res)["title"], "The Modern Way");
    EXPECT_EQ((*res)["author"], "C++23 Agent");
}

TEST_F(PublisherTest, RenderLatex) {
    Publisher pub(tmp_root_);
    auto res = pub.render("test-doc", OutputFormat::LaTeX, BuildMode::Draft);
    ASSERT_TRUE(res.has_value()) << (res.has_value() ? "" : res.error());
    EXPECT_TRUE(res->find(R"(\title{The Modern Way})") != std::string::npos);
    EXPECT_TRUE(res->find(R"(\date{\today})") != std::string::npos);
}

TEST_F(PublisherTest, RenderJson) {
    Publisher pub(tmp_root_);
    auto res = pub.render("test-doc", OutputFormat::JSON);
    ASSERT_TRUE(res.has_value());
    auto j = nlohmann::json::parse(*res);
    EXPECT_EQ(j["title"], "The Modern Way");
}

TEST_F(PublisherTest, DocumentNotFound) {
    Publisher pub(tmp_root_);
    auto res = pub.render("nonexistent");
    EXPECT_FALSE(res.has_value());
}

} // namespace
} // namespace euxis::publisher
