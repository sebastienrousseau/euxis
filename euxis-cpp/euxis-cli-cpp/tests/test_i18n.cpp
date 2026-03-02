#include <gtest/gtest.h>
#include "euxis/cli/i18n.hpp"

#include <filesystem>
#include <fstream>

namespace euxis::cli::i18n {
namespace {

namespace fs = std::filesystem;

class I18nTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir_ = "/tmp/euxis_test_i18n_" + std::to_string(getpid());
        translations_dir_ = fs::path(test_dir_) / "translations";
        fs::create_directories(translations_dir_);
    }

    void TearDown() override {
        // Reset to English to avoid affecting other tests
        Catalog::instance().load("en", translations_dir_);
        fs::remove_all(test_dir_);
    }

    std::string test_dir_;
    fs::path translations_dir_;
};

// --- Coverage: line 25, 27-29 (Catalog::load with non-English locale) ---
TEST_F(I18nTest, LoadNonEnglishLocale) {
    // Create a French translation file
    nlohmann::json translations;
    translations["hello"] = "bonjour";
    translations["world"] = "monde";
    std::ofstream(translations_dir_ / "fr.json") << translations.dump();

    Catalog::instance().load("fr", translations_dir_);

    // Verify locale was set
    EXPECT_EQ(Catalog::instance().current_locale(), "fr");
}

// --- Coverage: lines 31-32 (load with English locale clears translations) ---
TEST_F(I18nTest, LoadEnglishLocaleClears) {
    // First load French
    nlohmann::json translations;
    translations["test"] = "teste";
    std::ofstream(translations_dir_ / "fr.json") << translations.dump();
    Catalog::instance().load("fr", translations_dir_);

    // Then load English (should clear)
    Catalog::instance().load("en", translations_dir_);
    EXPECT_EQ(Catalog::instance().current_locale(), "en");

    // Translation should fall back to original
    EXPECT_EQ(Catalog::instance().translate("test"), "test");
}

// --- Coverage: lines 35-37 (load with missing translation file) ---
TEST_F(I18nTest, LoadMissingTranslationFile) {
    Catalog::instance().load("nonexistent", translations_dir_);
    // Should not crash, locale is set but no translations loaded
    EXPECT_EQ(Catalog::instance().current_locale(), "nonexistent");
    EXPECT_EQ(Catalog::instance().translate("hello"), "hello");
}

// --- Coverage: lines 40-48 (load reads JSON file, parses translations) ---
TEST_F(I18nTest, LoadReadsAndParsesFile) {
    nlohmann::json translations;
    translations["Error:"] = "Erreur:";
    translations["Version:"] = "Version:";
    translations["non_string_value"] = 42; // non-string values should be skipped
    std::ofstream(translations_dir_ / "fr.json") << translations.dump();

    Catalog::instance().load("fr", translations_dir_);

    // String values should be translated
    EXPECT_EQ(Catalog::instance().translate("Error:"), "Erreur:");
    // Non-string values should fall back
    EXPECT_EQ(Catalog::instance().translate("non_string_value"), "non_string_value");
}

// --- Coverage: lines 49-50 (malformed translation file) ---
TEST_F(I18nTest, LoadMalformedJsonFile) {
    std::ofstream(translations_dir_ / "bad.json") << "not valid json{{{";

    Catalog::instance().load("bad", translations_dir_);
    // Should not crash, falls back to English
    EXPECT_EQ(Catalog::instance().translate("hello"), "hello");
}

// --- Coverage: lines 55-61 (translate with found translation) ---
TEST_F(I18nTest, TranslateReturnsTranslation) {
    nlohmann::json translations;
    translations["Agents:"] = "Agenten:";
    translations["Squads:"] = "Trupps:";
    std::ofstream(translations_dir_ / "de.json") << translations.dump();

    Catalog::instance().load("de", translations_dir_);

    EXPECT_EQ(Catalog::instance().translate("Agents:"), "Agenten:");
    EXPECT_EQ(Catalog::instance().translate("Squads:"), "Trupps:");
    // Untranslated key falls back to original
    EXPECT_EQ(Catalog::instance().translate("Unknown key"), "Unknown key");
}

// --- Coverage: line 59 (translate returns original when no match) ---
TEST_F(I18nTest, TranslateReturnsOriginalForMissing) {
    Catalog::instance().load("en", translations_dir_);
    EXPECT_EQ(Catalog::instance().translate("anything"), "anything");
}

// --- Coverage: line 64-67 (current_locale) ---
TEST_F(I18nTest, CurrentLocaleReturnsCorrectValue) {
    EXPECT_EQ(Catalog::instance().current_locale(), "en"); // default
    Catalog::instance().load("ja", translations_dir_);
    EXPECT_EQ(Catalog::instance().current_locale(), "ja");
}

// --- Coverage: line 77-78 (tr shorthand function) ---
TEST_F(I18nTest, TrShorthandFunction) {
    nlohmann::json translations;
    translations["OK"] = "D'accord";
    std::ofstream(translations_dir_ / "fr.json") << translations.dump();

    Catalog::instance().load("fr", translations_dir_);
    EXPECT_EQ(tr("OK"), "D'accord");
    EXPECT_EQ(tr("untranslated"), "untranslated");
}

// --- Coverage: empty locale string ---
TEST_F(I18nTest, LoadEmptyLocale) {
    Catalog::instance().load("", translations_dir_);
    EXPECT_EQ(Catalog::instance().current_locale(), "");
    // Empty locale treated same as English
    EXPECT_EQ(Catalog::instance().translate("test"), "test");
}

// --- Coverage: Catalog singleton ---
TEST_F(I18nTest, SingletonSameInstance) {
    auto& a = Catalog::instance();
    auto& b = Catalog::instance();
    EXPECT_EQ(&a, &b);
}

} // namespace
} // namespace euxis::cli::i18n
