#pragma once

#include "euxis/cli/tui/widget.hpp"
#include "euxis/cli/tui/color_system.hpp"
#include "euxis/cli/tui/syntax.hpp"
#include "euxis/cli/tui/text_utils.hpp"

#include <string>
#include <vector>
#include <memory>

namespace euxis::cli::tui {

/** @brief Represents a single message in the chat flow. */
struct ChatMessage {
    std::string sender;         ///< ID of the agent or "You".
    std::string content;        ///< Raw message text.
    bool is_user{false};        ///< True if message is from the human user.
    bool is_thinking{false};    ///< True if this is a transient "thinking" state.
    std::string model;          ///< Optional model string used for this message.
    std::string timestamp;      ///< Generation time.
};

/**
 * @brief Scrollable chat display widget with Markdown support.
 * 
 * Handles rendering of a message stream with syntax highlighting, 
 * word wrapping, and interactive scrolling.
 */
class ChatWidget : public Widget {
public:
    ChatWidget();

    /** @brief Add a new complete message to the bottom. */
    void add_message(ChatMessage msg);
    
    /** @brief Append a token chunk to the last active message (for streaming). */
    void append_to_last(const std::string& chunk);
    
    /** @brief Toggle the thinking spinner for a specific agent. */
    void set_thinking(bool thinking, std::string agent_id = "");
    
    /** @brief Remove all messages and reset scroll. */
    void clear();

    /** @brief Widget implementation. */
    Size preferred_size() const override;
    void render(terminal::TerminalScreen& screen, Rect area) override;
    bool handle_event(const Event& event) override;

private:
    std::vector<ChatMessage> messages_;
    int scroll_offset_{0};
    const ColorSystem* color_system_{nullptr};
    SyntaxHighlighter highlighter_;

    /** @brief Internally render a single message block. */
    void render_message(terminal::TerminalScreen& screen, const ChatMessage& msg, int& y, Rect area) const;
};

} // namespace euxis::cli::tui
