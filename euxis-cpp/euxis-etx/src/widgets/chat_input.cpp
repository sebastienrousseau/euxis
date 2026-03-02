#include <euxis/etx/chat_input_widget.hpp>
#include <euxis/etx/registry.hpp>
#include <euxis/etx/ghost_text.hpp>

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QKeyEvent>
#include <QFont>

namespace euxis::etx {

ChatInputBar::ChatInputBar(FleetRegistry* registry, QWidget* parent)
    : QWidget(parent)
    , registry_(registry)
    , ghost_engine_(new GhostTextEngine())
{
    setObjectName("chat_input_bar");
    setFixedHeight(80);

    auto* outer_layout = new QVBoxLayout(this);
    outer_layout->setContentsMargins(0, 4, 0, 0);
    outer_layout->setSpacing(0);

    // Separator line
    auto* separator = new QFrame(this);
    separator->setFrameShape(QFrame::HLine);
    separator->setStyleSheet(
        "color: rgba(255,255,255,0.08); background: rgba(255,255,255,0.08);");
    separator->setFixedHeight(1);
    outer_layout->addWidget(separator);

    auto* input_row = new QHBoxLayout();
    input_row->setContentsMargins(16, 6, 16, 6);
    input_row->setSpacing(8);

    // Agent indicator chip
    agent_chip_ = new QLabel(this);
    agent_chip_->setObjectName("agent_chip");
    update_agent_chip("code-agent");
    agent_chip_->setCursor(Qt::PointingHandCursor);
    input_row->addWidget(agent_chip_);

    // Provider button
    provider_btn_ = new QPushButton(tr("provider"), this);
    provider_btn_->setObjectName("provider_btn");
    provider_btn_->setCursor(Qt::PointingHandCursor);
    provider_btn_->setStyleSheet(
        "QPushButton { color: #888; background: rgba(255,255,255,0.03); "
        "border: 1px solid rgba(255,255,255,0.08); border-radius: 10px; "
        "padding: 2px 8px; font-size: 10px; }"
        "QPushButton:hover { background: rgba(255,255,255,0.08); color: #c0c0c0; }");
    connect(provider_btn_, &QPushButton::clicked,
            this, &ChatInputBar::provider_button_clicked);
    input_row->addWidget(provider_btn_);

    // Text input
    input_ = new QTextEdit(this);
    input_->setObjectName("chat_input");
    input_->setPlaceholderText(tr("Message code-agent... (@ to switch agent, / for commands)"));
    input_->setMaximumHeight(60);
    input_->setTabChangesFocus(true);
    input_->installEventFilter(this);
    input_->setStyleSheet(
        "QTextEdit { background: rgba(255,255,255,0.05); "
        "border: 1px solid rgba(255,255,255,0.12); border-radius: 8px; "
        "padding: 8px 12px; color: #e0e0e0; font-size: 13px; }"
        "QTextEdit:focus { border-color: rgba(15,52,96,0.8); "
        "background: rgba(255,255,255,0.07); }");
    // Ghost text overlay (sits on top of input)
    ghost_label_ = new QLabel(input_);
    ghost_label_->setObjectName("ghost_label");
    ghost_label_->setStyleSheet(
        "color: rgba(255,255,255,0.25); background: transparent; border: none; "
        "padding: 8px 12px; font-size: 13px;");
    ghost_label_->setAttribute(Qt::WA_TransparentForMouseEvents);
    ghost_label_->hide();

    input_row->addWidget(input_, 1);

    // Send button
    send_btn_ = new QPushButton(">", this);
    send_btn_->setObjectName("send_button");
    send_btn_->setFixedSize(36, 36);
    send_btn_->setCursor(Qt::PointingHandCursor);
    send_btn_->setStyleSheet(
        "QPushButton { background: rgba(15,52,96,0.6); color: #e0e0e0; "
        "border: none; border-radius: 18px; font-size: 16px; font-weight: bold; }"
        "QPushButton:hover { background: rgba(15,52,96,0.9); }"
        "QPushButton:pressed { background: rgba(15,52,96,1.0); }");
    connect(send_btn_, &QPushButton::clicked, this, &ChatInputBar::submit);
    input_row->addWidget(send_btn_);

    outer_layout->addLayout(input_row);

    // Autocomplete popup (hidden by default)
    autocomplete_ = new QListWidget(this);
    autocomplete_->setObjectName("autocomplete_popup");
    autocomplete_->setWindowFlags(Qt::Popup);
    autocomplete_->setMaximumHeight(200);
    autocomplete_->setStyleSheet(
        "QListWidget { background: #1a1a2e; border: 1px solid rgba(255,255,255,0.15); "
        "border-radius: 6px; color: #e0e0e0; font-size: 12px; padding: 4px; }"
        "QListWidget::item { padding: 4px 8px; }"
        "QListWidget::item:selected { background: rgba(15,52,96,0.6); }");
    autocomplete_->hide();
    connect(autocomplete_, &QListWidget::itemActivated,
            this, &ChatInputBar::on_autocomplete_selected);

    connect(input_, &QTextEdit::textChanged,
            this, &ChatInputBar::on_text_changed);
}

void ChatInputBar::set_focus() {
    input_->setFocus();
}

void ChatInputBar::update_placeholder(const QString& agent_id) {
    input_->setPlaceholderText(
        tr("Message %1... (@ to switch agent, / for commands)")
            .arg(agent_id));
    update_agent_chip(agent_id);
}

void ChatInputBar::update_provider_label(const QString& provider) {
    provider_btn_->setText(provider);
}

bool ChatInputBar::eventFilter(QObject* obj, QEvent* event) {
    if (obj == input_ && event->type() == QEvent::KeyPress) {
        auto* ke = static_cast<QKeyEvent*>(event);

        if (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) {
            if (ke->modifiers() & Qt::ShiftModifier) return false;
            submit();
            return true;
        }
        if (ke->key() == Qt::Key_Up && input_->toPlainText().isEmpty()) {
            navigate_history(-1);
            return true;
        }
        if (ke->key() == Qt::Key_Down && input_->toPlainText().isEmpty()) {
            navigate_history(1);
            return true;
        }
        if ((ke->key() == Qt::Key_Tab || ke->key() == Qt::Key_Right) &&
            ghost_label_->isVisible()) {
            auto cursor = input_->textCursor();
            if (ke->key() == Qt::Key_Tab || cursor.atEnd()) {
                accept_ghost_suggestion();
                return true;
            }
        }
        if (ke->key() == Qt::Key_Escape) {
            if (autocomplete_->isVisible()) {
                autocomplete_->hide();
                return true;
            }
            input_->clearFocus();
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}

void ChatInputBar::submit() {
    QString text = input_->toPlainText().trimmed();
    if (text.isEmpty()) return;

    // Record for ghost text suggestions
    ghost_engine_->record(text.toStdString());
    ghost_label_->hide();

    input_history_.append(text);
    history_index_ = input_history_.size();
    input_->clear();
    autocomplete_->hide();

    if (text.startsWith("/")) {
        handle_slash_command(text);
        return;
    }

    emit message_submitted(text);
}

void ChatInputBar::on_text_changed() {
    QString text = input_->toPlainText();

    if (text.startsWith("@")) {
        ghost_label_->hide();
        show_agent_autocomplete(text.mid(1).split(" ").first());
        return;
    }
    if (text.startsWith("/") && !text.contains(" ")) {
        ghost_label_->hide();
        show_command_autocomplete(text.mid(1));
        return;
    }
    autocomplete_->hide();
    update_ghost_text();
}

void ChatInputBar::on_autocomplete_selected(QListWidgetItem* item) {
    QString value = item->data(Qt::UserRole).toString();
    QString current = input_->toPlainText();

    if (current.startsWith("@")) {
        input_->setPlainText("@" + value + " ");
        emit agent_selected(value);
    } else if (current.startsWith("/")) {
        input_->setPlainText("/" + value + " ");
    }

    autocomplete_->hide();
    auto cursor = input_->textCursor();
    cursor.movePosition(QTextCursor::End);
    input_->setTextCursor(cursor);
    input_->setFocus();
}

void ChatInputBar::update_agent_chip(const QString& agent_id) {
    agent_chip_->setText(agent_id);
    agent_chip_->setStyleSheet(
        "color: #6db3f2; background: rgba(15,52,96,0.4); "
        "border: 1px solid rgba(109,179,242,0.3); border-radius: 10px; "
        "padding: 2px 10px; font-size: 11px; font-weight: bold;");
}

void ChatInputBar::handle_slash_command(const QString& text) {
    QString cmd = text.mid(1).split(" ").first().toLower();
    QString args = text.mid(1 + cmd.length()).trimmed();
    emit slash_command(cmd, args);
}

void ChatInputBar::show_agent_autocomplete(const QString& prefix) {
    autocomplete_->clear();
    const auto& agents = registry_->agents();
    for (const auto& agent : agents) {
        if (agent.type != "agent") continue;
        if (!prefix.isEmpty() &&
            !agent.id.contains(prefix, Qt::CaseInsensitive)) continue;

        auto* item = new QListWidgetItem(
            agent.name + "  (" + agent.tier.toUpper() + ")",
            autocomplete_);
        item->setData(Qt::UserRole, agent.id);
    }
    if (autocomplete_->count() > 0) {
        QPoint pos = input_->mapToGlobal(QPoint(0, 0));
        int popup_h = qMin(autocomplete_->count() * 28 + 8, 200);
        autocomplete_->setGeometry(pos.x(), pos.y() - popup_h,
                                    input_->width(), popup_h);
        autocomplete_->show();
    } else {
        autocomplete_->hide();
    }
}

void ChatInputBar::show_command_autocomplete(const QString& prefix) {
    autocomplete_->clear();

    struct SlashCmd { QString name; QString desc; };
    QList<SlashCmd> commands = {
        {"clear",     tr("Clear conversation")},
        {"agent",     tr("Switch active agent")},
        {"model",     tr("Show current model/provider")},
        {"dashboard", tr("Switch to dashboard view")},
        {"metrics",   tr("Go to metrics screen")},
        {"logs",      tr("Go to logs screen")},
        {"playbooks", tr("Go to playbooks screen")},
        {"approvals", tr("Go to approvals screen")},
        {"cortex",    tr("Go to cortex screen")},
        {"omnigraph", tr("Go to OmniGraph screen")},
        {"help",      tr("Show available commands")},
    };

    for (const auto& cmd : commands) {
        if (!prefix.isEmpty() &&
            !cmd.name.contains(prefix, Qt::CaseInsensitive)) continue;
        auto* item = new QListWidgetItem(
            "/" + cmd.name + "  - " + cmd.desc, autocomplete_);
        item->setData(Qt::UserRole, cmd.name);
    }

    if (autocomplete_->count() > 0) {
        QPoint pos = input_->mapToGlobal(QPoint(0, 0));
        int popup_h = qMin(autocomplete_->count() * 28 + 8, 200);
        autocomplete_->setGeometry(pos.x(), pos.y() - popup_h,
                                    input_->width(), popup_h);
        autocomplete_->show();
    } else {
        autocomplete_->hide();
    }
}

void ChatInputBar::navigate_history(int direction) {
    if (input_history_.isEmpty()) return;
    history_index_ += direction;
    history_index_ = qBound(0, history_index_, input_history_.size());

    if (history_index_ < input_history_.size()) {
        input_->setPlainText(input_history_[history_index_]);
    } else {
        input_->clear();
    }
}

void ChatInputBar::update_ghost_text() {
    QString text = input_->toPlainText();
    if (text.isEmpty()) {
        ghost_label_->hide();
        return;
    }

    auto suggestion = ghost_engine_->suggest(text.toStdString());
    if (suggestion.has_value()) {
        // Show the suffix portion as ghost text
        QString full = QString::fromStdString(*suggestion);
        QString suffix = full.mid(text.length());
        if (!suffix.isEmpty()) {
            // Position ghost label to show suffix after current text
            ghost_label_->setText(QString(text.length(), ' ') + suffix);
            ghost_label_->resize(input_->size());
            ghost_label_->show();
            return;
        }
    }
    ghost_label_->hide();
}

void ChatInputBar::accept_ghost_suggestion() {
    QString text = input_->toPlainText();
    auto suggestion = ghost_engine_->suggest(text.toStdString());
    if (!suggestion.has_value()) { return; }

    QString full = QString::fromStdString(*suggestion);
    input_->setPlainText(full);
    auto cursor = input_->textCursor();
    cursor.movePosition(QTextCursor::End);
    input_->setTextCursor(cursor);
    ghost_label_->hide();
}

} // namespace euxis::etx
