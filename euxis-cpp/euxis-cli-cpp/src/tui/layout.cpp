#include "euxis/cli/tui/layout.hpp"

#include <algorithm>
#include <cmath>

namespace euxis::cli::tui {

// ---------------------------------------------------------------------------
// VBox
// ---------------------------------------------------------------------------

void VBox::add(std::shared_ptr<Widget> child) {
    children_.push_back(std::move(child));
}

void VBox::clear() {
    children_.clear();
}

Size VBox::preferred_size() const {
    int w = 0, h = 0;
    for (const auto& child : children_) {
        if (!child->visible()) continue;
        auto sz = child->preferred_size();
        w = std::max(w, sz.width);
        h += sz.height;
    }
    return {w, h};
}

void VBox::render(terminal::TerminalScreen& screen, Rect area) {
    if (children_.empty() || area.empty()) return;

    // Count visible children
    int visible_count = 0;
    for (const auto& c : children_) {
        if (c->visible()) visible_count++;
    }
    if (visible_count == 0) return;

    // Distribute height equally among visible children
    int each_h = area.h / visible_count;
    int remainder = area.h % visible_count;
    int y = area.y;

    for (const auto& child : children_) {
        if (!child->visible()) continue;
        int h = each_h + (remainder > 0 ? 1 : 0);
        if (remainder > 0) remainder--;
        child->render(screen, {area.x, y, area.w, h});
        y += h;
    }
}

bool VBox::handle_event(const Event& event) {
    for (const auto& child : children_) {
        if (child->visible() && child->handle_event(event)) return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// HBox
// ---------------------------------------------------------------------------

void HBox::add(std::shared_ptr<Widget> child) {
    children_.push_back(std::move(child));
}

void HBox::clear() {
    children_.clear();
}

Size HBox::preferred_size() const {
    int w = 0, h = 0;
    for (const auto& child : children_) {
        if (!child->visible()) continue;
        auto sz = child->preferred_size();
        w += sz.width;
        h = std::max(h, sz.height);
    }
    return {w, h};
}

void HBox::render(terminal::TerminalScreen& screen, Rect area) {
    if (children_.empty() || area.empty()) return;

    int visible_count = 0;
    for (const auto& c : children_) {
        if (c->visible()) visible_count++;
    }
    if (visible_count == 0) return;

    int each_w = area.w / visible_count;
    int remainder = area.w % visible_count;
    int x = area.x;

    for (const auto& child : children_) {
        if (!child->visible()) continue;
        int w = each_w + (remainder > 0 ? 1 : 0);
        if (remainder > 0) remainder--;
        child->render(screen, {x, area.y, w, area.h});
        x += w;
    }
}

bool HBox::handle_event(const Event& event) {
    for (const auto& child : children_) {
        if (child->visible() && child->handle_event(event)) return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// HSplit
// ---------------------------------------------------------------------------

HSplit::HSplit(std::shared_ptr<Widget> left, std::shared_ptr<Widget> right, float ratio)
    : left_(std::move(left)), right_(std::move(right)), ratio_(ratio) {}

Size HSplit::preferred_size() const {
    auto ls = left_ ? left_->preferred_size() : Size{0, 0};
    auto rs = right_ ? right_->preferred_size() : Size{0, 0};
    return {ls.width + rs.width, std::max(ls.height, rs.height)};
}

void HSplit::render(terminal::TerminalScreen& screen, Rect area) {
    if (area.empty()) return;

    int left_w = static_cast<int>(std::round(area.w * ratio_));
    int right_w = area.w - left_w;

    if (left_ && left_->visible()) {
        left_->render(screen, {area.x, area.y, left_w, area.h});
    }
    if (right_ && right_->visible()) {
        right_->render(screen, {area.x + left_w, area.y, right_w, area.h});
    }
}

bool HSplit::handle_event(const Event& event) {
    if (left_ && left_->handle_event(event)) return true;
    if (right_ && right_->handle_event(event)) return true;
    return false;
}

// ---------------------------------------------------------------------------
// VSplit
// ---------------------------------------------------------------------------

VSplit::VSplit(std::shared_ptr<Widget> top, std::shared_ptr<Widget> bottom, float ratio)
    : top_(std::move(top)), bottom_(std::move(bottom)), ratio_(ratio) {}

Size VSplit::preferred_size() const {
    auto ts = top_ ? top_->preferred_size() : Size{0, 0};
    auto bs = bottom_ ? bottom_->preferred_size() : Size{0, 0};
    return {std::max(ts.width, bs.width), ts.height + bs.height};
}

void VSplit::render(terminal::TerminalScreen& screen, Rect area) {
    if (area.empty()) return;

    int top_h = static_cast<int>(std::round(area.h * ratio_));
    int bottom_h = area.h - top_h;

    if (top_ && top_->visible()) {
        top_->render(screen, {area.x, area.y, area.w, top_h});
    }
    if (bottom_ && bottom_->visible()) {
        bottom_->render(screen, {area.x, area.y + top_h, area.w, bottom_h});
    }
}

bool VSplit::handle_event(const Event& event) {
    if (top_ && top_->handle_event(event)) return true;
    if (bottom_ && bottom_->handle_event(event)) return true;
    return false;
}

// ---------------------------------------------------------------------------
// Float
// ---------------------------------------------------------------------------

Float::Float(std::shared_ptr<Widget> content, float width_pct, int max_rows)
    : content_(std::move(content)), width_pct_(width_pct), max_rows_(max_rows) {}

Size Float::preferred_size() const {
    if (content_) return content_->preferred_size();
    return {40, 12};
}

void Float::render(terminal::TerminalScreen& screen, Rect area) {
    if (!content_ || area.empty()) return;

    int float_w = static_cast<int>(area.w * width_pct_);
    int float_h = std::min(max_rows_, area.h - 2);
    if (float_w < 10 || float_h < 3) return;

    int float_x = area.x + (area.w - float_w) / 2;
    int float_y = area.y + (area.h - float_h) / 2;

    // Draw border
    screen.draw_box(float_x, float_y, float_w, float_h);

    // Render content inside the border
    Rect inner{float_x + 1, float_y + 1, float_w - 2, float_h - 2};
    content_->render(screen, inner);
}

bool Float::handle_event(const Event& event) {
    if (content_) return content_->handle_event(event);
    return false;
}

} // namespace euxis::cli::tui
