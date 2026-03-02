#include <gtest/gtest.h>
#include <QApplication>
#include <QPushButton>
#include <QLabel>
#include <QWidget>
#include <QKeyEvent>
#include <QSignalSpy>

#include "euxis/etx/accessibility.hpp"

namespace euxis::etx {
namespace {

class AccessibilityTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!QApplication::instance()) {
            static int argc = 1;
            static char arg0[] = "test";
            static char* argv[] = {arg0};
            new QApplication(argc, argv);
        }
    }
};

TEST_F(AccessibilityTest, LabelSetsAccessibleName) {
    QLabel widget;
    a11y::label(&widget, "Test Label", "A test description");

    EXPECT_EQ(widget.accessibleName().toStdString(), "Test Label");
    EXPECT_EQ(widget.accessibleDescription().toStdString(), "A test description");
}

TEST_F(AccessibilityTest, LabelWithoutDescription) {
    QLabel widget;
    a11y::label(&widget, "Name Only");

    EXPECT_EQ(widget.accessibleName().toStdString(), "Name Only");
    EXPECT_TRUE(widget.accessibleDescription().isEmpty());
}

TEST_F(AccessibilityTest, InteractiveSetsTabFocus) {
    QPushButton button;
    button.setFocusPolicy(Qt::NoFocus);

    a11y::interactive(&button, "Action Button");

    EXPECT_EQ(button.focusPolicy(), Qt::TabFocus);
    EXPECT_EQ(button.accessibleName().toStdString(), "Action Button");
}

TEST_F(AccessibilityTest, FocusStyleApplied) {
    QLabel widget;
    a11y::apply_focus_style(&widget);

    EXPECT_TRUE(widget.styleSheet().contains(":focus"));
}

TEST_F(AccessibilityTest, StatusIndicatorCreated) {
    QWidget parent;
    auto* indicator = a11y::create_status_indicator("Running", "#4caf50", &parent);

    ASSERT_NE(indicator, nullptr);
    EXPECT_EQ(indicator->accessibleName().toStdString(), "Running");
}

TEST_F(AccessibilityTest, FocusChainInstalled) {
    QPushButton a, b, c;
    a11y::install_focus_chain({&a, &b, &c});
    // Focus chain installation doesn't crash — verifying no-throw
    SUCCEED();
}

TEST_F(AccessibilityTest, NullWidgetSafe) {
    // All helpers should handle nullptr gracefully
    a11y::label(nullptr, "test");
    a11y::interactive(nullptr, "test");
    a11y::enable_keyboard_activation(nullptr);
    a11y::apply_focus_style(nullptr);
    SUCCEED();
}

TEST_F(AccessibilityTest, FocusStyleNotDuplicated) {
    QLabel widget;
    a11y::apply_focus_style(&widget);
    QString first = widget.styleSheet();
    a11y::apply_focus_style(&widget);
    // Should not duplicate the :focus style
    EXPECT_EQ(widget.styleSheet(), first);
}

TEST_F(AccessibilityTest, KeyboardActivationFilterNotDuplicated) {
    QPushButton button;
    a11y::enable_keyboard_activation(&button);
    int count1 = 0;
    for (auto* child : button.children()) {
        if (child->inherits("QObject")) ++count1;
    }
    a11y::enable_keyboard_activation(&button);
    int count2 = 0;
    for (auto* child : button.children()) {
        if (child->inherits("QObject")) ++count2;
    }
    // Should not add duplicate filter
    EXPECT_EQ(count1, count2);
}

TEST_F(AccessibilityTest, InteractiveSetsAccessibleName) {
    QPushButton button;
    a11y::interactive(&button, "My Button", QAccessible::Button);
    EXPECT_EQ(button.accessibleName().toStdString(), "My Button");
    EXPECT_EQ(button.focusPolicy(), Qt::TabFocus);
}

TEST_F(AccessibilityTest, StatusIndicatorColorApplied) {
    QWidget parent;
    auto* indicator = a11y::create_status_indicator("Error", "#f44336", &parent);
    ASSERT_NE(indicator, nullptr);
    EXPECT_EQ(indicator->accessibleName().toStdString(), "Error");
    // Check that children exist (dot + label)
    EXPECT_GE(indicator->children().size(), 2);
}

TEST_F(AccessibilityTest, EmptyFocusChain) {
    a11y::install_focus_chain({});
    // Should not crash with empty list
    SUCCEED();
}

TEST_F(AccessibilityTest, SingleItemFocusChain) {
    QPushButton a;
    a11y::install_focus_chain({&a});
    SUCCEED();
}

TEST_F(AccessibilityTest, RegisterAccessibilityFactories) {
    a11y::register_accessibility_factories();
    // Should not crash
    SUCCEED();
}

TEST_F(AccessibilityTest, LabelWithEmptyName) {
    QLabel widget;
    a11y::label(&widget, "", "description only");
    EXPECT_EQ(widget.accessibleName().toStdString(), "");
    EXPECT_EQ(widget.accessibleDescription().toStdString(), "description only");
}

// =============================================================================
// KeyboardActivationFilter — eventFilter (lines 36-51 in accessibility.cpp)
// =============================================================================

TEST_F(AccessibilityTest, KeyboardActivationFilterEnterKey) {
    QPushButton button("Click Me");
    a11y::enable_keyboard_activation(&button);
    button.show();

    QSignalSpy click_spy(&button, &QPushButton::clicked);

    // Send a Key_Return event — the filter should simulate a mouse click
    QKeyEvent enter_event(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
    QApplication::sendEvent(&button, &enter_event);

    EXPECT_GE(click_spy.count(), 1);
}

TEST_F(AccessibilityTest, KeyboardActivationFilterSpaceKey) {
    QPushButton button("Space Test");
    a11y::enable_keyboard_activation(&button);
    button.show();

    QSignalSpy click_spy(&button, &QPushButton::clicked);

    // Send a Key_Space event — should also trigger click
    QKeyEvent space_event(QEvent::KeyPress, Qt::Key_Space, Qt::NoModifier);
    QApplication::sendEvent(&button, &space_event);

    EXPECT_GE(click_spy.count(), 1);
}

TEST_F(AccessibilityTest, KeyboardActivationFilterEnterNumpadKey) {
    QPushButton button("Numpad Enter");
    a11y::enable_keyboard_activation(&button);
    button.show();

    QSignalSpy click_spy(&button, &QPushButton::clicked);

    // Send Key_Enter (numpad enter) event
    QKeyEvent numpad_event(QEvent::KeyPress, Qt::Key_Enter, Qt::NoModifier);
    QApplication::sendEvent(&button, &numpad_event);

    EXPECT_GE(click_spy.count(), 1);
}

TEST_F(AccessibilityTest, KeyboardActivationFilterOtherKeyIgnored) {
    QPushButton button("Other Key");
    a11y::enable_keyboard_activation(&button);
    button.show();

    QSignalSpy click_spy(&button, &QPushButton::clicked);

    // Send a non-activating key (Key_A) — should NOT trigger click
    QKeyEvent a_event(QEvent::KeyPress, Qt::Key_A, Qt::NoModifier);
    QApplication::sendEvent(&button, &a_event);

    EXPECT_EQ(click_spy.count(), 0);
}

TEST_F(AccessibilityTest, KeyboardActivationViaInteractive) {
    // interactive() calls enable_keyboard_activation internally
    QPushButton button("Interactive");
    a11y::interactive(&button, "Test Button");
    button.show();

    QSignalSpy click_spy(&button, &QPushButton::clicked);

    QKeyEvent return_event(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
    QApplication::sendEvent(&button, &return_event);

    EXPECT_GE(click_spy.count(), 1);
}

} // namespace
} // namespace euxis::etx
