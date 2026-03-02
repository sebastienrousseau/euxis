#include <euxis/etx/config.hpp>

#include <QSettings>
#include <gtest/gtest.h>

namespace euxis::etx {
namespace {

class ETXConfigTest : public ::testing::Test {
protected:
    void SetUp() override {
        // ETXConfig hardcodes QSettings("Euxis", "ETX") — clear that
        QSettings settings("Euxis", "ETX");
        settings.clear();
        settings.sync();
    }

    void TearDown() override {
        QSettings settings("Euxis", "ETX");
        settings.clear();
        settings.sync();
    }
};

TEST_F(ETXConfigTest, DefaultThemeIsLiquidGlass) {
    ETXConfig config;
    EXPECT_EQ(config.theme(), "liquid-glass");
}

TEST_F(ETXConfigTest, SetAndGetTheme) {
    ETXConfig config;
    config.set_theme("calm");
    EXPECT_EQ(config.theme(), "calm");
}

TEST_F(ETXConfigTest, ThemePersistsAcrossInstances) {
    {
        ETXConfig config;
        config.set_theme("focused");
    }
    ETXConfig config2;
    EXPECT_EQ(config2.theme(), "focused");
}

TEST_F(ETXConfigTest, DefaultRefreshIntervalIs5000) {
    ETXConfig config;
    EXPECT_EQ(config.refresh_interval(), 5000);
}

TEST_F(ETXConfigTest, SetAndGetRefreshInterval) {
    ETXConfig config;
    config.set_refresh_interval(2000);
    EXPECT_EQ(config.refresh_interval(), 2000);
}

TEST_F(ETXConfigTest, DefaultShowTipsIsTrue) {
    ETXConfig config;
    EXPECT_TRUE(config.show_tips());
}

TEST_F(ETXConfigTest, SetAndGetShowTips) {
    ETXConfig config;
    config.set_show_tips(false);
    EXPECT_FALSE(config.show_tips());
}

// --- Coverage: line 39 (euxis_home with EUXIS_HOME env var set) ---
TEST_F(ETXConfigTest, EuxisHomeFromEnvVar) {
    setenv("EUXIS_HOME", "/tmp/euxis-test-home", 1);
    EXPECT_EQ(ETXConfig::euxis_home(), "/tmp/euxis-test-home");
    unsetenv("EUXIS_HOME");
}

// --- Coverage: line 48 (data_dir derived from euxis_home) ---
TEST_F(ETXConfigTest, DataDirDerivedFromHome) {
    setenv("EUXIS_HOME", "/tmp/test-etx", 1);
    EXPECT_EQ(ETXConfig::data_dir(), "/tmp/test-etx/euxis-data");
    unsetenv("EUXIS_HOME");
}

// --- Coverage: line 49 (runtime_dir derived from euxis_home) ---
TEST_F(ETXConfigTest, RuntimeDirDerivedFromHome) {
    setenv("EUXIS_HOME", "/tmp/test-etx", 1);
    EXPECT_EQ(ETXConfig::runtime_dir(), "/tmp/test-etx/euxis-runtime");
    unsetenv("EUXIS_HOME");
}

} // namespace
} // namespace euxis::etx
