#include <gtest/gtest.h>
#include <QApplication>
#include <QLabel>
#include <euxis/etx/bento_layout.hpp>

namespace euxis::etx {
namespace {

class BentoLayoutTest : public ::testing::Test {
protected:
    void SetUp() override {
        layout_ = new BentoLayout();
    }
    void TearDown() override {
        delete layout_;
    }
    BentoLayout* layout_{nullptr};
};

TEST_F(BentoLayoutTest, DefaultsEmpty) {
    EXPECT_EQ(layout_->card_count(), 0);
    EXPECT_EQ(layout_->column_count(), 1);
}

TEST_F(BentoLayoutTest, AddCards) {
    auto* card1 = new QLabel("A");
    auto* card2 = new QLabel("B");
    layout_->add_card(card1);
    layout_->add_card(card2);
    EXPECT_EQ(layout_->card_count(), 2);
}

TEST_F(BentoLayoutTest, CustomBreakpoints) {
    QVector<QPair<int, int>> bp = {{400, 2}, {800, 4}};
    layout_->set_breakpoints(bp);
    // Just verify no crash
    auto* card = new QLabel("C");
    layout_->add_card(card);
    EXPECT_EQ(layout_->card_count(), 1);
}

TEST_F(BentoLayoutTest, RelayoutOnResize) {
    auto* card1 = new QLabel("X");
    auto* card2 = new QLabel("Y");
    layout_->add_card(card1);
    layout_->add_card(card2);
    // Resize to trigger relayout — should not crash
    layout_->resize(1200, 600);
    EXPECT_GE(layout_->column_count(), 1);
}

// --- Coverage: lines 41-44 (relayout with 0 cards) ---
TEST_F(BentoLayoutTest, RelayoutWithZeroCards) {
    // Trigger relayout via resize with no cards added
    layout_->resize(1200, 600);
    EXPECT_EQ(layout_->card_count(), 0);
    // column_count still gets updated based on breakpoints
    EXPECT_GE(layout_->column_count(), 1);
}

// --- Coverage: relayout triggered by set_breakpoints with no cards ---
TEST_F(BentoLayoutTest, SetBreakpointsWithNoCards) {
    QVector<QPair<int, int>> bp = {{400, 2}, {800, 4}};
    layout_->set_breakpoints(bp);
    EXPECT_EQ(layout_->card_count(), 0);
}

} // namespace
} // namespace euxis::etx
