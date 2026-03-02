#pragma once
#include <QScrollArea>
#include <QVBoxLayout>
#include <QLabel>
#include <QTimer>

namespace euxis::etx {

struct ChatMessage;

/// Animated "thinking..." indicator label shown while waiting for a response.
class ThinkingIndicator : public QLabel {
    Q_OBJECT
public:
    explicit ThinkingIndicator(QWidget* parent = nullptr);
    /// Start the animation with the given agent name displayed.
    void start(const QString& agent_id);
    /// Stop the animation and hide the indicator.
    void stop();
private slots:
    void animate();
private:
    QTimer* timer_;
    QString agent_id_;
    int dot_count_{1};
};

/// Scrollable conversation view displaying chat bubbles.
///
/// Renders user, assistant, and system messages as styled bubbles.
/// Auto-scrolls to the latest message. Shows a thinking indicator
/// while awaiting a provider response.
class ConversationWidget : public QScrollArea {
    Q_OBJECT
public:
    explicit ConversationWidget(QWidget* parent = nullptr);

    /// Append a user message bubble.
    void add_user_message(const QString& text);
    /// Append an assistant response bubble with model and timing metadata.
    void add_assistant_message(const QString& agent_id, const QString& text,
                               const QString& model, double duration_ms);
    /// Append a system information bubble.
    void add_system_message(const QString& text);
    /// Show the thinking indicator for the given agent.
    void show_thinking(const QString& agent_id);
    /// Hide the thinking indicator.
    void hide_thinking();
    /// Remove all message bubbles from the conversation.
    void clear_conversation();

private:
    void insert_bubble(QWidget* bubble);
    void scroll_to_bottom();

    QWidget* container_;
    QVBoxLayout* layout_;
    ThinkingIndicator* thinking_;
};

} // namespace euxis::etx
