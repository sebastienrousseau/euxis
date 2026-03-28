#pragma once
#include <memory>

#include <QWidget>
#include <QTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QListWidget>
#include <QStringList>

namespace euxis::etx {

class FleetRegistry;
class GhostTextEngine;

/// Chat input bar widget with agent/command autocomplete.
///
/// Provides a multi-line text input with:
/// - "@agent" autocomplete from the fleet registry
/// - "/command" slash-command autocomplete
/// - Input history navigation (Up/Down arrows)
/// - Provider selection button
class ChatInputBar : public QWidget {
    Q_OBJECT
public:
    /// Construct an input bar backed by @p registry for agent autocomplete.
    explicit ChatInputBar(FleetRegistry* registry, QWidget* parent = nullptr);
    ~ChatInputBar() override;

    /// Give keyboard focus to the text input.
    void set_focus();
    /// Update the placeholder text to reflect the active agent.
    void update_placeholder(const QString& agent_id);
    /// Update the provider chip label.
    void update_provider_label(const QString& provider);

signals:
    /// Emitted when the user submits a message (Enter key).
    void message_submitted(const QString& text);
    /// Emitted when a slash command is entered (e.g. "/clear").
    void slash_command(const QString& command, const QString& args);
    /// Emitted when the user selects an agent via "@" autocomplete.
    void agent_selected(const QString& agent_id);
    /// Emitted when the provider chip button is clicked.
    void provider_button_clicked();

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private slots:
    void submit();
    void on_text_changed();
    void on_autocomplete_selected(QListWidgetItem* item);

private:
    void update_agent_chip(const QString& agent_id);
    void handle_slash_command(const QString& text);
    void show_agent_autocomplete(const QString& prefix);
    void show_command_autocomplete(const QString& prefix);
    void navigate_history(int direction);

    FleetRegistry* registry_;
    std::unique_ptr<GhostTextEngine> ghost_engine_;
    QTextEdit* input_;
    QLabel* ghost_label_;
    QPushButton* send_btn_;
    QLabel* agent_chip_;
    QPushButton* provider_btn_;
    QListWidget* autocomplete_;
    QStringList input_history_;
    int history_index_{0};

    void update_ghost_text();
    void accept_ghost_suggestion();
};

} // namespace euxis::etx
