#include "euxis/cli/tui/chat_widget.hpp"
#include "euxis/cli/tui/text_utils.hpp"
#include "euxis/cli/tui/keybindings.hpp"
#include <sstream>
#include <algorithm>
#include <format>

namespace euxis::cli::tui {

ChatWidget::ChatWidget() {
    accessible_name = "Chat Area";
}

void ChatWidget::add_message(ChatMessage msg) {
    messages_.push_back(std::move(msg));
    
    // Performance: Enforce a memory bound (Sliding Window)
    // Prevents OOM (Exit code 137) during long-lived sessions
    if (messages_.size() > 100) [[unlikely]] {
        messages_.erase(messages_.begin());
    }
    
    scroll_offset_ = 0;
}

void ChatWidget::append_to_last(const std::string& chunk) {
    if (!messages_.empty()) [[likely]] {
        messages_.back().content += chunk;
    }
}

void ChatWidget::set_thinking(bool thinking, std::string agent_id) {
    if (thinking) {
        if (messages_.empty() || !messages_.back().is_thinking) {
            add_message({agent_id, "", false, true, "", ""});
        }
    } else {
        if (!messages_.empty() && messages_.back().is_thinking) {
            messages_.back().is_thinking = false;
        }
    }
}

void ChatWidget::clear() {
    messages_.clear();
    scroll_offset_ = 0;
}

Size ChatWidget::preferred_size() const { return {80, 20}; }

void ChatWidget::render(terminal::TerminalScreen& screen, Rect area) {
    if (area.empty()) return;

    // Pre-calculate line wrapping and cache it if possible (Simplified for now)
    std::vector<int> heights;
    int total_h = 0;
    int content_width = area.w - 7;
    if (content_width <= 0) content_width = 10;

    for (const auto& msg : messages_) {
        int h = 1; // Header (Sender name)
        
        std::istringstream ss(msg.content);
        std::string raw_line;
        bool in_code_block = false;

        while (std::getline(ss, raw_line)) {
            if (raw_line.starts_with("```")) {
                in_code_block = !in_code_block;
                h++;
                continue;
            }

            if (in_code_block) {
                // Optimization: Rough line count for code blocks
                h++;
            } else if (raw_line.starts_with("#")) {
                h += 1;
            } else if (raw_line.starts_with("- ") || raw_line.starts_with("* ") || (raw_line.size() > 2 && std::isdigit(raw_line[0]) && raw_line[1] == '.')) {
                h += 1;
            } else if (raw_line.empty()) {
                h++;
            } else {
                // Heuristic: estimate wrapped height to avoid expensive string splitting on every frame
                h += static_cast<int>(std::max(1UL, (raw_line.size() + content_width - 1) / content_width));
            }
        }
        
        if (msg.is_thinking) h++;
        h += 1; // Extra spacing
        heights.push_back(h);
        total_h += h;
    }

    // Scroll offset handling
    int current_scroll = std::clamp(scroll_offset_, 0, std::max(0, total_h - area.h)); 
    
    int current_y = area.y - current_scroll;
    for (size_t i = 0; i < messages_.size(); ++i) {
        int h = heights[i];
        // Only render visible messages
        if (current_y + h > area.y && current_y < area.y + area.h) {
            render_message(screen, messages_[i], current_y, area);
        } else {
            current_y += h;
        }
    }
}

bool ChatWidget::handle_event(const Event& event) {
    if (event.type != EventType::Key) return false;

    if (event.key == KeyCode::ArrowUp || event.key == 'k') {
        scroll_offset_ = std::max(0, scroll_offset_ - 1);
        return true;
    }
    if (event.key == KeyCode::ArrowDown || event.key == 'j') {
        scroll_offset_ += 1;
        return true;
    }
    if (event.key == KeyCode::PageUp) {
        scroll_offset_ = std::max(0, scroll_offset_ - 10);
        return true;
    }
    if (event.key == KeyCode::PageDown) {
        scroll_offset_ += 10;
        return true;
    }
    if (event.key == KeyCode::Home) {
        scroll_offset_ = 0;
        return true;
    }

    return false;
}

void ChatWidget::render_message(terminal::TerminalScreen& screen, const ChatMessage& msg, int& y, Rect area) const {
    uint8_t user_r = 122, user_g = 162, user_b = 247;
    uint8_t agent_r = 187, agent_g = 154, agent_b = 247;
    uint8_t header_r = 255, header_g = 158, header_b = 100;
    uint8_t code_r = 158, code_g = 206, code_b = 106;
    uint8_t text_r = 169, text_g = 177, text_b = 214;
    uint8_t muted_r = 68, muted_g = 75, muted_b = 106;
    uint8_t bullet_r = 255, bullet_g = 158, bullet_b = 100;

    if (color_system_) {
        auto info = color_system_->color(SemanticRole::Info);
        user_r = info.r; user_g = info.g; user_b = info.b;
        auto accent = color_system_->color(SemanticRole::Accent);
        agent_r = accent.r; agent_g = accent.g; agent_b = accent.b;
        auto warn = color_system_->color(SemanticRole::Warning);
        header_r = warn.r; header_g = warn.g; header_b = warn.b;
        auto success = color_system_->color(SemanticRole::Success);
        code_r = success.r; code_g = success.g; code_b = success.b;
        auto primary = color_system_->color(SemanticRole::Primary);
        text_r = primary.r; text_g = primary.g; text_b = primary.b;
        auto muted = color_system_->color(SemanticRole::Muted);
        muted_r = muted.r; muted_g = muted.g; muted_b = muted.b;
        bullet_r = warn.r; bullet_g = warn.g; bullet_b = warn.b;
    }

    if (y >= area.y && y < area.y + area.h) {
        std::string prefix = msg.is_user ? "➜  " : "◈  ";
        uint8_t pr = msg.is_user ? user_r : agent_r;
        uint8_t pg = msg.is_user ? user_g : agent_g;
        uint8_t pb = msg.is_user ? user_b : agent_b;
        
        screen.write_text(area.x + 2, y, prefix, pr, pg, pb, 0, 0, 0, true);
        screen.write_text(area.x + 5, y, msg.sender, pr, pg, pb, 0, 0, 0, true);
        
        if (!msg.model.empty()) {
            screen.write_text(area.x + 5 + msg.sender.size() + 2, y, std::format("[{}]", msg.model), muted_r, muted_g, muted_b);
        }
    }
    y++;

    int content_width = area.w - 7;
    if (content_width <= 0) return;

    std::istringstream ss(msg.content);
    std::string raw_line;
    bool in_code_block = false;

    while (std::getline(ss, raw_line)) {
        if (raw_line.starts_with("```")) {
            in_code_block = !in_code_block;
            if (y >= area.y && y < area.y + area.h) {
                std::string sep = "────────────────────────────────────────────────────────────────";
                if (sep.size() > (size_t)content_width) sep = sep.substr(0, content_width);
                screen.write_text(area.x + 5, y, sep, muted_r, muted_g, muted_b);
            }
            y++;
            continue;
        }

        if (in_code_block) {
            if (y >= area.y && y < area.y + area.h) {
                std::string line = raw_line;
                if (line.size() > (size_t)content_width) line = line.substr(0, content_width);
                screen.write_text(area.x + 5, y, line, code_r, code_g, code_b);
            }
            y++;
        } else if (raw_line.starts_with("#")) {
            std::vector<std::string> wrapped = wrap_text(raw_line, content_width);
            for (const auto& wl : wrapped) {
                if (y >= area.y && y < area.y + area.h) {
                    screen.write_text(area.x + 5, y, wl, header_r, header_g, header_b, 0, 0, 0, true);
                }
                y++;
            }
        } else if (raw_line.starts_with("- ") || raw_line.starts_with("* ") || (raw_line.size() > 2 && std::isdigit(raw_line[0]) && raw_line[1] == '.')) {
            std::string bullet = raw_line.substr(0, raw_line.find(' ') + 1);
            std::string text = raw_line.substr(bullet.size());
            std::vector<std::string> wrapped = wrap_text(text, content_width - (int)bullet.size());
            
            for (size_t i = 0; i < wrapped.size(); ++i) {
                if (y >= area.y && y < area.y + area.h) {
                    if (i == 0) {
                        screen.write_text(area.x + 5, y, bullet, bullet_r, bullet_g, bullet_b);
                        screen.write_text(area.x + 5 + (int)bullet.size(), y, wrapped[i], text_r, text_g, text_b);
                    } else {
                        screen.write_text(area.x + 5 + (int)bullet.size(), y, wrapped[i], text_r, text_g, text_b);
                    }
                }
                y++;
            }
        } else if (raw_line.empty()) {
            y++;
        } else {
            std::vector<std::string> wrapped = wrap_text(raw_line, content_width);
            for (const auto& wl : wrapped) {
                if (y >= area.y && y < area.y + area.h) {
                    screen.write_text(area.x + 5, y, wl, text_r, text_g, text_b);
                }
                y++;
            }
        }
    }

    if (msg.is_thinking) {
        if (y >= area.y && y < area.y + area.h) {
            screen.write_text(area.x + 5, y, "⠋ thinking...", muted_r, muted_g, muted_b);
        }
        y++;
    }
    
    y++; 
}

} // namespace euxis::cli::tui
