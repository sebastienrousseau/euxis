#include <gtest/gtest.h>
#include <QApplication>
#include <QLabel>
#include <QSignalSpy>
#include <euxis/etx/collapsible_section.hpp>

namespace euxis::etx {
namespace {

class CollapsibleSectionTest : public ::testing::Test {
protected:
    void SetUp() override {
        section_ = new CollapsibleSection("Test Section");
    }
    void TearDown() override {
        delete section_;
    }
    CollapsibleSection* section_{nullptr};
};

TEST_F(CollapsibleSectionTest, DefaultExpanded) {
    EXPECT_TRUE(section_->is_expanded());
}

TEST_F(CollapsibleSectionTest, ToggleChangesState) {
    section_->toggle();
    EXPECT_FALSE(section_->is_expanded());
    section_->toggle();
    EXPECT_TRUE(section_->is_expanded());
}

TEST_F(CollapsibleSectionTest, SignalEmitted) {
    QSignalSpy spy(section_, &CollapsibleSection::toggled);
    section_->toggle();
    EXPECT_EQ(spy.count(), 1);
    auto args = spy.takeFirst();
    EXPECT_FALSE(args.at(0).toBool());
}

TEST_F(CollapsibleSectionTest, SetContent) {
    auto* content = new QLabel("Inner content");
    section_->set_content(content);
    // Should not crash
    section_->toggle();
    section_->toggle();
}

} // namespace
} // namespace euxis::etx
