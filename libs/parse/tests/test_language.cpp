#include <gtest/gtest.h>

#include "euxis/parse/types.hpp"

namespace euxis::parse {
namespace {

TEST(Language, StringifyCoversEveryEnumerator) {
    EXPECT_STREQ(language_str(Language::C),   "c");
    EXPECT_STREQ(language_str(Language::Cpp), "cpp");
}

TEST(LanguageDetect, RecognisesCExtensions) {
    EXPECT_EQ(detect_language_by_extension(".c"), Language::C);
    EXPECT_EQ(detect_language_by_extension("c"),  Language::C);
    EXPECT_EQ(detect_language_by_extension(".h"), Language::C);
}

TEST(LanguageDetect, RecognisesCppExtensions) {
    EXPECT_EQ(detect_language_by_extension(".cpp"), Language::Cpp);
    EXPECT_EQ(detect_language_by_extension(".cxx"), Language::Cpp);
    EXPECT_EQ(detect_language_by_extension(".cc"),  Language::Cpp);
    EXPECT_EQ(detect_language_by_extension(".hpp"), Language::Cpp);
    EXPECT_EQ(detect_language_by_extension(".hxx"), Language::Cpp);
    EXPECT_EQ(detect_language_by_extension(".hh"),  Language::Cpp);
    EXPECT_EQ(detect_language_by_extension(".tpp"), Language::Cpp);
    EXPECT_EQ(detect_language_by_extension(".inl"), Language::Cpp);
    EXPECT_EQ(detect_language_by_extension(".cppm"), Language::Cpp);
}

TEST(LanguageDetect, CaseInsensitive) {
    EXPECT_EQ(detect_language_by_extension(".CPP"), Language::Cpp);
    EXPECT_EQ(detect_language_by_extension(".Hpp"), Language::Cpp);
}

TEST(LanguageDetect, UnknownReturnsNullopt) {
    EXPECT_FALSE(detect_language_by_extension(".rs").has_value());
    EXPECT_FALSE(detect_language_by_extension(".py").has_value());
    EXPECT_FALSE(detect_language_by_extension("").has_value());
    EXPECT_FALSE(detect_language_by_extension("not-an-ext").has_value());
}

TEST(LanguageDetect, FromPath) {
    EXPECT_EQ(detect_language(std::filesystem::path{"src/foo.c"}),   Language::C);
    EXPECT_EQ(detect_language(std::filesystem::path{"src/foo.cpp"}), Language::Cpp);
    EXPECT_EQ(detect_language(std::filesystem::path{"src/foo.hpp"}), Language::Cpp);
    EXPECT_FALSE(detect_language(std::filesystem::path{"README.md"}).has_value());
}

} // namespace
} // namespace euxis::parse
