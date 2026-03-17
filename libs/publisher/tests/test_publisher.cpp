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

        // Write data.yaml with mixed types for coverage
        std::ofstream data(tmp_root_ / "data" / "test.yaml");
        data << R"(
title: 'The Modern Way'
author: 'C++23 Agent'
version: 1
rating: 4.5
active: true
tags: [fast, safe]
)";
        data.close();

        // Write template with custom delimiters
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

TEST_F(PublisherTest, LoadYamlDataMixedTypes) {
    Publisher pub(tmp_root_);
    auto res = pub.load_data(tmp_root_ / "data" / "test.yaml");
    ASSERT_TRUE(res.has_value());
    EXPECT_EQ((*res)["title"], "The Modern Way");
    EXPECT_EQ((*res)["version"], 1);
    EXPECT_DOUBLE_EQ((*res)["rating"], 4.5);
    EXPECT_TRUE((*res)["active"]);
    EXPECT_TRUE((*res)["tags"].is_array());
}

TEST_F(PublisherTest, LoadYamlFailure) {
    Publisher pub(tmp_root_);
    auto res = pub.load_data(tmp_root_ / "nonexistent.yaml");
    EXPECT_FALSE(res.has_value());
    EXPECT_TRUE(res.error().find("YAML I/O failure") != std::string::npos);
}

TEST_F(PublisherTest, RenderLatex) {
    Publisher pub(tmp_root_);
    auto res = pub.render("test-doc", OutputFormat::LaTeX, BuildMode::Draft);
    ASSERT_TRUE(res.has_value()) << (res.has_value() ? "" : res.error());
    EXPECT_TRUE(res->find(R"(\title{The Modern Way})") != std::string::npos);
    EXPECT_TRUE(res->find(R"(\date{\today})") != std::string::npos);
}

TEST_F(PublisherTest, RenderBuildModes) {
    Publisher pub(tmp_root_);
    
    auto res_sub = pub.render("test-doc", OutputFormat::LaTeX, BuildMode::Submission);
    EXPECT_TRUE(res_sub->find(R"(\date{\today})") == std::string::npos);

    auto res_cam = pub.render("test-doc", OutputFormat::LaTeX, BuildMode::CameraReady);
    EXPECT_TRUE(res_cam->find(R"(\date{\today})") == std::string::npos);
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
    EXPECT_TRUE(res.error().find("Missing metadata entry") != std::string::npos);
}

TEST_F(PublisherTest, MetaYamlMissing) {
    std::filesystem::remove(tmp_root_ / "data" / "meta.yaml");
    Publisher pub(tmp_root_);
    auto res = pub.render("test-doc");
    EXPECT_FALSE(res.has_value());
}

TEST_F(PublisherTest, TemplatesKeyMissing) {
    std::ofstream meta(tmp_root_ / "data" / "meta.yaml");
    meta << "something_else: true\n";
    meta.close();
    Publisher pub(tmp_root_);
    auto res = pub.render("test-doc");
    EXPECT_FALSE(res.has_value());
}

TEST_F(PublisherTest, InjaRenderingFailure) {
    // Malformed template (unclosed tag)
    std::ofstream tmpl(tmp_root_ / "templates" / "test.tex.j2");
    tmpl << "<< title";
    tmpl.close();
    
    Publisher pub(tmp_root_);
    auto res = pub.render("test-doc");
    EXPECT_FALSE(res.has_value());
    EXPECT_TRUE(res.error().find("Inja Engine Error") != std::string::npos);
}

TEST_F(PublisherTest, BuildPdfSuccess) {
    Publisher pub(tmp_root_);
    auto res = pub.build_pdf("test-doc");
    ASSERT_TRUE(res.has_value()) << res.error();
    EXPECT_EQ(res->extension(), ".pdf");
}

TEST_F(PublisherTest, BuildPdfFailure) {
    Publisher pub(tmp_root_);
    auto res = pub.build_pdf("nonexistent");
    EXPECT_FALSE(res.has_value());
}

TEST_F(PublisherTest, UnsupportedFormat) {
    Publisher pub(tmp_root_);
    auto res = pub.render("test-doc", static_cast<OutputFormat>(99));
    EXPECT_FALSE(res.has_value());
    EXPECT_TRUE(res.error().find("Unsupported output format") != std::string::npos);
}

} // namespace
} // namespace euxis::publisher
