#include <gtest/gtest.h>
#include "euxis/cli/tui/layout.hpp"
#include "euxis/cli/tui/focus_manager.hpp"

namespace euxis::cli::tui {
namespace {

// --- Test Widget ---

class TestWidget : public Widget {
public:
    explicit TestWidget(int w = 10, int h = 5, bool can_focus = false)
        : w_(w), h_(h), can_focus_(can_focus) {}

    Size preferred_size() const override { return {w_, h_}; }
    void render(terminal::TerminalScreen&, Rect area) override { last_area_ = area; render_count_++; }
    bool focusable() const override { return can_focus_; }

    Rect last_area_{};
    int render_count_{0};

private:
    int w_, h_;
    bool can_focus_;
};

// --- Rect Tests ---

TEST(RectTest, ContainsInside) {
    Rect r{10, 20, 30, 40};
    EXPECT_TRUE(r.contains(10, 20));
    EXPECT_TRUE(r.contains(39, 59));
    EXPECT_TRUE(r.contains(25, 40));
}

TEST(RectTest, ContainsOutside) {
    Rect r{10, 20, 30, 40};
    EXPECT_FALSE(r.contains(9, 20));
    EXPECT_FALSE(r.contains(40, 20));
    EXPECT_FALSE(r.contains(10, 60));
}

TEST(RectTest, EmptyRect) {
    Rect r1{0, 0, 0, 0};
    EXPECT_TRUE(r1.empty());
    Rect r2{0, 0, -1, 5};
    EXPECT_TRUE(r2.empty());
    Rect r3{0, 0, 1, 1};
    EXPECT_FALSE(r3.empty());
}

// --- LayoutMode Tests ---

TEST(LayoutModeTest, SinglePaneUnder80) {
    EXPECT_EQ(layout_mode_for_width(79), LayoutMode::SinglePane);
    EXPECT_EQ(layout_mode_for_width(40), LayoutMode::SinglePane);
}

TEST(LayoutModeTest, DualPane80To120) {
    EXPECT_EQ(layout_mode_for_width(80), LayoutMode::DualPane);
    EXPECT_EQ(layout_mode_for_width(100), LayoutMode::DualPane);
    EXPECT_EQ(layout_mode_for_width(120), LayoutMode::DualPane);
}

TEST(LayoutModeTest, TriplePaneOver120) {
    EXPECT_EQ(layout_mode_for_width(121), LayoutMode::TriplePane);
    EXPECT_EQ(layout_mode_for_width(200), LayoutMode::TriplePane);
}

// --- VBox Tests ---

TEST(VBoxTest, PreferredSize) {
    VBox vbox;
    vbox.add(std::make_shared<TestWidget>(20, 5));
    vbox.add(std::make_shared<TestWidget>(30, 10));
    auto sz = vbox.preferred_size();
    EXPECT_EQ(sz.width, 30);
    EXPECT_EQ(sz.height, 15);
}

TEST(VBoxTest, RenderDistributesHeight) {
    VBox vbox;
    auto w1 = std::make_shared<TestWidget>();
    auto w2 = std::make_shared<TestWidget>();
    vbox.add(w1);
    vbox.add(w2);

    terminal::TerminalScreen screen;
    vbox.render(screen, {0, 0, 80, 20});

    EXPECT_EQ(w1->last_area_.y, 0);
    EXPECT_EQ(w1->last_area_.h, 10);
    EXPECT_EQ(w2->last_area_.y, 10);
    EXPECT_EQ(w2->last_area_.h, 10);
}

TEST(VBoxTest, SkipsInvisibleChildren) {
    VBox vbox;
    auto w1 = std::make_shared<TestWidget>();
    auto w2 = std::make_shared<TestWidget>();
    w1->set_visible(false);
    vbox.add(w1);
    vbox.add(w2);

    terminal::TerminalScreen screen;
    vbox.render(screen, {0, 0, 80, 20});

    EXPECT_EQ(w1->render_count_, 0);
    EXPECT_EQ(w2->last_area_.h, 20); // Gets all height
}

TEST(VBoxTest, EmptyVBox) {
    VBox vbox;
    EXPECT_EQ(vbox.preferred_size().width, 0);
    EXPECT_EQ(vbox.preferred_size().height, 0);
}

TEST(VBoxTest, ClearChildren) {
    VBox vbox;
    vbox.add(std::make_shared<TestWidget>());
    vbox.clear();
    EXPECT_TRUE(vbox.children().empty());
}

// --- HBox Tests ---

TEST(HBoxTest, PreferredSize) {
    HBox hbox;
    hbox.add(std::make_shared<TestWidget>(20, 5));
    hbox.add(std::make_shared<TestWidget>(30, 10));
    auto sz = hbox.preferred_size();
    EXPECT_EQ(sz.width, 50);
    EXPECT_EQ(sz.height, 10);
}

TEST(HBoxTest, RenderDistributesWidth) {
    HBox hbox;
    auto w1 = std::make_shared<TestWidget>();
    auto w2 = std::make_shared<TestWidget>();
    hbox.add(w1);
    hbox.add(w2);

    terminal::TerminalScreen screen;
    hbox.render(screen, {0, 0, 80, 20});

    EXPECT_EQ(w1->last_area_.x, 0);
    EXPECT_EQ(w1->last_area_.w, 40);
    EXPECT_EQ(w2->last_area_.x, 40);
    EXPECT_EQ(w2->last_area_.w, 40);
}

// --- HSplit Tests ---

TEST(HSplitTest, DefaultRatio) {
    auto left = std::make_shared<TestWidget>();
    auto right = std::make_shared<TestWidget>();
    HSplit split(left, right);

    EXPECT_FLOAT_EQ(split.ratio(), 0.5f);
}

TEST(HSplitTest, RenderWithRatio) {
    auto left = std::make_shared<TestWidget>();
    auto right = std::make_shared<TestWidget>();
    HSplit split(left, right, 0.3f);

    terminal::TerminalScreen screen;
    split.render(screen, {0, 0, 100, 20});

    EXPECT_EQ(left->last_area_.x, 0);
    EXPECT_EQ(left->last_area_.w, 30);
    EXPECT_EQ(right->last_area_.x, 30);
    EXPECT_EQ(right->last_area_.w, 70);
}

TEST(HSplitTest, SetRatio) {
    auto left = std::make_shared<TestWidget>();
    auto right = std::make_shared<TestWidget>();
    HSplit split(left, right);
    split.set_ratio(0.7f);
    EXPECT_FLOAT_EQ(split.ratio(), 0.7f);
}

// --- VSplit Tests ---

TEST(VSplitTest, RenderWithRatio) {
    auto top = std::make_shared<TestWidget>();
    auto bottom = std::make_shared<TestWidget>();
    VSplit split(top, bottom, 0.4f);

    terminal::TerminalScreen screen;
    split.render(screen, {0, 0, 80, 50});

    EXPECT_EQ(top->last_area_.y, 0);
    EXPECT_EQ(top->last_area_.h, 20);
    EXPECT_EQ(bottom->last_area_.y, 20);
    EXPECT_EQ(bottom->last_area_.h, 30);
}

// --- Float Tests ---

TEST(FloatTest, RendersCentered) {
    auto content = std::make_shared<TestWidget>();
    Float f(content, 0.6f, 12);

    terminal::TerminalScreen screen;
    f.render(screen, {0, 0, 100, 30});

    // Content should be rendered inside the float box
    EXPECT_GT(content->render_count_, 0);
    EXPECT_GT(content->last_area_.x, 0); // Centered, not at edge
}

TEST(FloatTest, TooSmallAreaSkipsRender) {
    auto content = std::make_shared<TestWidget>();
    Float f(content);

    terminal::TerminalScreen screen;
    f.render(screen, {0, 0, 5, 2}); // Too small

    EXPECT_EQ(content->render_count_, 0);
}

TEST(FloatTest, IsFocusable) {
    auto content = std::make_shared<TestWidget>();
    Float f(content);
    EXPECT_TRUE(f.focusable());
}

// --- FocusManager Tests ---

TEST(FocusManagerTest, EmptyManager) {
    FocusManager fm;
    EXPECT_EQ(fm.focused(), nullptr);
    EXPECT_EQ(fm.focused_index(), -1);
    EXPECT_EQ(fm.size(), 0u);
}

TEST(FocusManagerTest, FocusNext) {
    FocusManager fm;
    auto w1 = std::make_shared<TestWidget>(10, 5, true);
    auto w2 = std::make_shared<TestWidget>(10, 5, true);
    fm.add(w1);
    fm.add(w2);

    EXPECT_TRUE(fm.focus_next());
    EXPECT_EQ(fm.focused(), w1);
    EXPECT_TRUE(w1->focused());

    EXPECT_TRUE(fm.focus_next());
    EXPECT_EQ(fm.focused(), w2);
    EXPECT_FALSE(w1->focused());
    EXPECT_TRUE(w2->focused());
}

TEST(FocusManagerTest, FocusPrev) {
    FocusManager fm;
    auto w1 = std::make_shared<TestWidget>(10, 5, true);
    auto w2 = std::make_shared<TestWidget>(10, 5, true);
    fm.add(w1);
    fm.add(w2);

    EXPECT_TRUE(fm.focus_prev());
    EXPECT_EQ(fm.focused(), w2); // Wraps to end

    EXPECT_TRUE(fm.focus_prev());
    EXPECT_EQ(fm.focused(), w1);
}

TEST(FocusManagerTest, SkipsNonFocusable) {
    FocusManager fm;
    auto w1 = std::make_shared<TestWidget>(10, 5, false); // Not focusable
    auto w2 = std::make_shared<TestWidget>(10, 5, true);
    fm.add(w1);
    fm.add(w2);

    EXPECT_TRUE(fm.focus_next());
    EXPECT_EQ(fm.focused(), w2); // Skips w1
}

TEST(FocusManagerTest, SkipsInvisible) {
    FocusManager fm;
    auto w1 = std::make_shared<TestWidget>(10, 5, true);
    auto w2 = std::make_shared<TestWidget>(10, 5, true);
    w1->set_visible(false);
    fm.add(w1);
    fm.add(w2);

    EXPECT_TRUE(fm.focus_next());
    EXPECT_EQ(fm.focused(), w2); // Skips invisible w1
}

TEST(FocusManagerTest, SetFocus) {
    FocusManager fm;
    auto w1 = std::make_shared<TestWidget>(10, 5, true);
    auto w2 = std::make_shared<TestWidget>(10, 5, true);
    fm.add(w1);
    fm.add(w2);

    EXPECT_TRUE(fm.set_focus(w2));
    EXPECT_EQ(fm.focused(), w2);
}

TEST(FocusManagerTest, SetFocusNonFocusableFails) {
    FocusManager fm;
    auto w = std::make_shared<TestWidget>(10, 5, false);
    fm.add(w);

    EXPECT_FALSE(fm.set_focus(w));
}

TEST(FocusManagerTest, ModalPushPop) {
    FocusManager fm;
    auto w1 = std::make_shared<TestWidget>(10, 5, true);
    auto modal = std::make_shared<TestWidget>(10, 5, true);
    fm.add(w1);
    fm.focus_next();

    EXPECT_FALSE(fm.has_modal());

    fm.push_modal(modal);
    EXPECT_TRUE(fm.has_modal());
    EXPECT_EQ(fm.modal(), modal);
    EXPECT_EQ(fm.focused(), modal);

    // Focus operations should be blocked during modal
    EXPECT_FALSE(fm.focus_next());

    fm.pop_modal();
    EXPECT_FALSE(fm.has_modal());
    EXPECT_EQ(fm.focused(), w1); // Restored
}

TEST(FocusManagerTest, RemoveWidget) {
    FocusManager fm;
    auto w1 = std::make_shared<TestWidget>(10, 5, true);
    auto w2 = std::make_shared<TestWidget>(10, 5, true);
    fm.add(w1);
    fm.add(w2);
    fm.focus_next(); // Focus on w1

    fm.remove(w1);
    EXPECT_EQ(fm.size(), 1u);
}

TEST(FocusManagerTest, ClearAllWidgets) {
    FocusManager fm;
    fm.add(std::make_shared<TestWidget>(10, 5, true));
    fm.add(std::make_shared<TestWidget>(10, 5, true));
    fm.clear();

    EXPECT_EQ(fm.size(), 0u);
    EXPECT_EQ(fm.focused(), nullptr);
}

TEST(FocusManagerTest, FocusWrapAround) {
    FocusManager fm;
    auto w1 = std::make_shared<TestWidget>(10, 5, true);
    auto w2 = std::make_shared<TestWidget>(10, 5, true);
    fm.add(w1);
    fm.add(w2);

    fm.focus_next(); // w1
    fm.focus_next(); // w2
    fm.focus_next(); // wraps to w1
    EXPECT_EQ(fm.focused(), w1);
}

} // namespace
} // namespace euxis::cli::tui
