#include <gtest/gtest.h>
#include <QApplication>
#include <euxis/etx/breadcrumb_widget.hpp>
#include <QSignalSpy>

namespace euxis::etx {
namespace {

class BreadcrumbTest : public ::testing::Test {
protected:
    void SetUp() override {
        widget_ = new BreadcrumbWidget();
    }
    void TearDown() override {
        delete widget_;
    }
    BreadcrumbWidget* widget_{nullptr};
};

TEST_F(BreadcrumbTest, InitEmpty) {
    EXPECT_EQ(widget_->count(), 0);
}

TEST_F(BreadcrumbTest, NavigateSingle) {
    widget_->navigate_to(0, "Dashboard");
    EXPECT_EQ(widget_->count(), 1);
}

TEST_F(BreadcrumbTest, NavigateSequence) {
    widget_->navigate_to(0, "Dashboard");
    widget_->navigate_to(1, "Agent");
    widget_->navigate_to(2, "Logs");
    EXPECT_EQ(widget_->count(), 3);
}

TEST_F(BreadcrumbTest, TruncateOnRevisit) {
    widget_->navigate_to(0, "Dashboard");
    widget_->navigate_to(1, "Agent");
    widget_->navigate_to(2, "Logs");
    widget_->navigate_to(0, "Dashboard");  // revisiting Dashboard
    EXPECT_EQ(widget_->count(), 1);
}

TEST_F(BreadcrumbTest, Reset) {
    widget_->navigate_to(0, "Dashboard");
    widget_->navigate_to(1, "Agent");
    widget_->reset(5, "Metrics");
    EXPECT_EQ(widget_->count(), 1);
}

TEST_F(BreadcrumbTest, SignalEmitted) {
    QSignalSpy spy(widget_, &BreadcrumbWidget::crumb_clicked);
    // Just verify signal can be connected — actual click test needs QTest
    EXPECT_TRUE(spy.isValid());
}

} // namespace
} // namespace euxis::etx
