#include "euxis/cli/tui/focus_manager.hpp"

#include <algorithm>

namespace euxis::cli::tui {

void FocusManager::add(std::shared_ptr<Widget> widget) {
    widgets_.push_back(std::move(widget));
}

void FocusManager::remove(const std::shared_ptr<Widget>& widget) {
    auto it = std::find(widgets_.begin(), widgets_.end(), widget);
    if (it == widgets_.end()) return;

    int idx = static_cast<int>(std::distance(widgets_.begin(), it));
    widgets_.erase(it);

    // Adjust current focus index: empty list and "removed the focused
    // widget" both collapse to "no focus".
    if (widgets_.empty() || idx == current_) {
        current_ = -1;
    } else if (idx < current_) {
        current_--;
    }
}

void FocusManager::clear() {
    for (auto& w : widgets_) w->set_focused(false);
    widgets_.clear();
    current_ = -1;
}

bool FocusManager::focus_next() {
    if (has_modal()) return false; // Modal steals focus

    if (widgets_.empty()) return false;

    int start = (current_ < 0) ? 0 : current_ + 1;
    int n = static_cast<int>(widgets_.size());

    for (int i = 0; i < n; ++i) {
        int idx = (start + i) % n;
        if (widgets_[idx]->focusable() && widgets_[idx]->visible()) {
            if (current_ >= 0 && current_ < n) widgets_[current_]->set_focused(false);
            current_ = idx;
            widgets_[current_]->set_focused(true);
            return true;
        }
    }
    return false;
}

bool FocusManager::focus_prev() {
    if (has_modal()) return false;

    if (widgets_.empty()) return false;

    int n = static_cast<int>(widgets_.size());
    int start = (current_ <= 0) ? n - 1 : current_ - 1;

    for (int i = 0; i < n; ++i) {
        int idx = (start - i + n) % n;
        if (widgets_[idx]->focusable() && widgets_[idx]->visible()) {
            if (current_ >= 0 && current_ < n) widgets_[current_]->set_focused(false);
            current_ = idx;
            widgets_[current_]->set_focused(true);
            return true;
        }
    }
    return false;
}

bool FocusManager::set_focus(const std::shared_ptr<Widget>& widget) {
    auto it = std::find(widgets_.begin(), widgets_.end(), widget);
    if (it == widgets_.end()) return false;
    if (!widget->focusable() || !widget->visible()) return false;

    int n = static_cast<int>(widgets_.size());
    if (current_ >= 0 && current_ < n) widgets_[current_]->set_focused(false);

    current_ = static_cast<int>(std::distance(widgets_.begin(), it));
    widgets_[current_]->set_focused(true);
    return true;
}

std::shared_ptr<Widget> FocusManager::focused() const {
    int n = static_cast<int>(widgets_.size());
    if (has_modal()) return modal_stack_.back();
    if (current_ >= 0 && current_ < n) return widgets_[current_];
    return nullptr;
}

void FocusManager::push_modal(std::shared_ptr<Widget> modal) {
    modal->set_focused(true);
    modal_stack_.push_back(std::move(modal));
}

void FocusManager::pop_modal() {
    if (modal_stack_.empty()) return;
    modal_stack_.back()->set_focused(false);
    modal_stack_.pop_back();

    // Restore focus to the previously focused widget
    int n = static_cast<int>(widgets_.size());
    if (current_ >= 0 && current_ < n) {
        widgets_[current_]->set_focused(true);
    }
}

std::shared_ptr<Widget> FocusManager::modal() const {
    if (modal_stack_.empty()) return nullptr;
    return modal_stack_.back();
}

} // namespace euxis::cli::tui
