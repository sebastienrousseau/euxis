#include <euxis/etx/app.hpp>
#include <euxis/etx/registry.hpp>
#include <euxis/etx/theme.hpp>
#include <euxis/etx/config.hpp>
#include <euxis/etx/chat_engine.hpp>
#include <euxis/etx/conversation_widget.hpp>
#include <euxis/etx/chat_input_widget.hpp>

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QFrame>
#include <QTimer>
#include <QPushButton>
#include <QSplitter>
#include <QListWidget>
#include <QShortcut>
#include <QStackedWidget>
#include <QDialog>

// Forward declarations from widget files
namespace euxis::etx {
    QWidget* create_header_widget(ThemeEngine* theme, QWidget* parent);
    QWidget* create_shortcut_bar_widget(QWidget* parent);
}

namespace euxis::etx {

// --- Agent Sidebar ---
class AgentSidebar : public QWidget {
    Q_OBJECT

public:
    explicit AgentSidebar(FleetRegistry* registry, EuxisApp* app,
                          QWidget* parent = nullptr)
        : QWidget(parent)
        , registry_(registry)
        , app_(app)
    {
        setObjectName("agent_sidebar");
        setMinimumWidth(220);
        setMaximumWidth(350);
        setStyleSheet(
            "QWidget#agent_sidebar { "
            "background: rgba(0,0,0,0.1); "
            "border-left: 1px solid rgba(255,255,255,0.06); }");

        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(8, 8, 8, 8);
        layout->setSpacing(6);

        // Sidebar header
        auto* header = new QLabel(tr("Agents"), this);
        QFont header_font;
        header_font.setPointSize(11);
        header_font.setBold(true);
        header->setFont(header_font);
        header->setStyleSheet(
            "color: #888; background: transparent; border: none; "
            "padding: 4px 0;");
        layout->addWidget(header);

        // Search filter
        search_ = new QLineEdit(this);
        search_->setObjectName("sidebar_search");
        search_->setPlaceholderText(tr("Filter agents..."));
        search_->setFixedHeight(32);
        search_->setStyleSheet(
            "QLineEdit { background: rgba(255,255,255,0.03); "
            "border: 1px solid rgba(255,255,255,0.08); border-radius: 6px; "
            "padding: 4px 8px; color: #c0c0c0; font-size: 11px; }"
            "QLineEdit:focus { border-color: rgba(15,52,96,0.6); }");
        layout->addWidget(search_);

        // Agent list
        agent_list_ = new QListWidget(this);
        agent_list_->setObjectName("agent_list");
        agent_list_->setStyleSheet(
            "QListWidget { background: transparent; border: none; outline: none; }"
            "QListWidget::item { padding: 6px 8px; border-radius: 6px; "
            "color: #c0c0c0; margin: 1px 0; }"
            "QListWidget::item:selected { background: rgba(15,52,96,0.4); "
            "color: #6db3f2; }"
            "QListWidget::item:hover { background: rgba(255,255,255,0.04); }");
        layout->addWidget(agent_list_, 1);

        // Squads section
        auto* squads_header = new QLabel(tr("Squads"), this);
        squads_header->setStyleSheet(
            "color: #666; font-size: 10px; font-weight: bold; "
            "background: transparent; border: none; padding: 8px 0 2px 0;");
        layout->addWidget(squads_header);

        squad_list_ = new QListWidget(this);
        squad_list_->setObjectName("squad_list");
        squad_list_->setMaximumHeight(120);
        squad_list_->setStyleSheet(agent_list_->styleSheet());
        layout->addWidget(squad_list_);

        // Quick actions
        auto* actions_header = new QLabel(tr("Quick Actions"), this);
        actions_header->setStyleSheet(squads_header->styleSheet());
        layout->addWidget(actions_header);

        struct QuickAction { QString label; int screen_index; };
        QList<QuickAction> actions = {
            {tr("Metrics"),   12}, {tr("Logs"),       7}, {tr("Playbooks"),  8},
            {tr("Approvals"),  9}, {tr("OmniGraph"), 13},
        };
        for (const auto& action : actions) {
            auto* btn = new QPushButton(action.label, this);
            btn->setCursor(Qt::PointingHandCursor);
            btn->setStyleSheet(
                "QPushButton { background: rgba(255,255,255,0.03); "
                "border: 1px solid rgba(255,255,255,0.06); border-radius: 6px; "
                "color: #888; padding: 4px 8px; font-size: 11px; text-align: left; }"
                "QPushButton:hover { background: rgba(255,255,255,0.06); color: #c0c0c0; }");
            int idx = action.screen_index;
            connect(btn, &QPushButton::clicked, this, [this, idx]() {
                emit screen_requested(idx);
            });
            layout->addWidget(btn);
        }

        connect(search_, &QLineEdit::textChanged,
                this, &AgentSidebar::filter_agents);

        connect(agent_list_, &QListWidget::currentItemChanged,
                this, [this](QListWidgetItem* current, QListWidgetItem*) {
            if (current) emit agent_clicked(current->data(Qt::UserRole).toString());
        });

        connect(agent_list_, &QListWidget::itemDoubleClicked,
                this, [this](QListWidgetItem* item) {
            if (item && app_)
                app_->show_agent(item->data(Qt::UserRole).toString().toStdString());
        });

        connect(squad_list_, &QListWidget::itemDoubleClicked,
                this, [this](QListWidgetItem* item) {
            if (item) emit squad_clicked(item->data(Qt::UserRole).toString());
        });

        connect(registry_, &FleetRegistry::refreshed,
                this, &AgentSidebar::populate);
        populate();
    }

    void set_active_agent(const QString& agent_id) {
        for (int i = 0; i < agent_list_->count(); ++i) {
            auto* item = agent_list_->item(i);
            if (item->data(Qt::UserRole).toString() == agent_id) {
                agent_list_->setCurrentItem(item);
                break;
            }
        }
    }

signals:
    void agent_clicked(const QString& agent_id);
    void squad_clicked(const QString& squad_id);
    void screen_requested(int index);

private:
    void populate() {
        agent_list_->clear();
        const auto& agents = registry_->agents();
        for (const auto& agent : agents) {
            if (agent.type != "agent" || agent.tier != "core") continue;
            add_agent_item(agent);
        }
        for (const auto& agent : agents) {
            if (agent.type != "agent" || agent.tier == "core") continue;
            add_agent_item(agent);
        }
        squad_list_->clear();
        for (const auto& squad : registry_->squads()) {
            auto* item = new QListWidgetItem(squad.name, squad_list_);
            item->setData(Qt::UserRole, squad.id);
        }
    }

    void add_agent_item(const AgentInfo& agent) {
        QString prefix = (agent.status == "running" || agent.status == "error")
            ? QString::fromUtf8("\xe2\x97\x8f ") : QString::fromUtf8("\xe2\x97\x8b ");
        auto* item = new QListWidgetItem(prefix + agent.name, agent_list_);
        item->setData(Qt::UserRole, agent.id);
        item->setToolTip(agent.description);
        if (agent.status == "error") item->setForeground(QColor("#f44336"));
    }

    void filter_agents(const QString& text) {
        for (int i = 0; i < agent_list_->count(); ++i) {
            auto* item = agent_list_->item(i);
            item->setHidden(
                !item->text().contains(text, Qt::CaseInsensitive) &&
                !item->data(Qt::UserRole).toString().contains(text, Qt::CaseInsensitive));
        }
    }

    FleetRegistry* registry_;
    EuxisApp* app_;
    QLineEdit* search_;
    QListWidget* agent_list_;
    QListWidget* squad_list_;
};

// --- Dashboard Screen (split layout) ---
class DashboardScreen : public QWidget {
    Q_OBJECT

public:
    explicit DashboardScreen(EuxisApp* app, FleetRegistry* registry,
                             ThemeEngine* theme, ETXConfig* /*config*/,
                             ChatEngine* chat, QWidget* parent = nullptr)
        : QWidget(parent)
        , chat_(chat)
        , app_(app)
    {
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);

        // Header
        layout->addWidget(create_header_widget(theme, this));

        // Main split: conversation (left) + sidebar (right)
        auto* splitter = new QSplitter(Qt::Horizontal, this);
        splitter->setObjectName("dashboard_splitter");
        splitter->setHandleWidth(1);
        splitter->setStyleSheet(
            "QSplitter { background: transparent; }"
            "QSplitter::handle { background: rgba(255,255,255,0.06); }");

        // --- Left panel: conversation + input ---
        auto* left_panel = new QWidget(splitter);
        auto* left_layout = new QVBoxLayout(left_panel);
        left_layout->setContentsMargins(0, 0, 0, 0);
        left_layout->setSpacing(0);

        conversation_ = new ConversationWidget(left_panel);
        left_layout->addWidget(conversation_, 1);

        input_ = new ChatInputBar(registry, left_panel);
        left_layout->addWidget(input_);

        // --- Right panel: agent sidebar ---
        sidebar_ = new AgentSidebar(registry, app, splitter);

        splitter->addWidget(left_panel);
        splitter->addWidget(sidebar_);
        splitter->setSizes({700, 300});

        layout->addWidget(splitter, 1);

        // Shortcut bar
        layout->addWidget(create_shortcut_bar_widget(this));

        // --- Wire signals: direct method calls, no QMetaObject ---

        // Input bar -> this
        connect(input_, &ChatInputBar::message_submitted,
                this, &DashboardScreen::on_message_submitted);
        connect(input_, &ChatInputBar::slash_command,
                this, &DashboardScreen::on_slash_command);
        connect(input_, &ChatInputBar::agent_selected,
                this, &DashboardScreen::on_agent_selected);

        // ChatEngine -> conversation
        connect(chat_, &ChatEngine::response_started,
                this, [this](const QString& agent_id, const QString&) {
            conversation_->show_thinking(agent_id);
        });
        connect(chat_, &ChatEngine::response_finished,
                this, [this](const QString& full_text,
                             const QString& model, double duration_ms) {
            conversation_->hide_thinking();
            conversation_->add_assistant_message(
                chat_->active_agent(), full_text, model, duration_ms);
        });
        connect(chat_, &ChatEngine::response_error,
                this, [this](const QString& error) {
            conversation_->hide_thinking();
            conversation_->add_system_message(tr("Error: %1").arg(error));
        });

        // Sidebar -> agent selection
        connect(sidebar_, &AgentSidebar::agent_clicked,
                this, [this](const QString& agent_id) {
            chat_->set_active_agent(agent_id);
            input_->update_placeholder(agent_id);
        });
        connect(sidebar_, &AgentSidebar::screen_requested,
                this, [app](int index) {
            auto* stack = app->findChild<QStackedWidget*>();
            if (stack) stack->setCurrentIndex(index);
        });

        // Provider button -> show selection dialog
        connect(input_, &ChatInputBar::provider_button_clicked,
                this, &DashboardScreen::on_provider_button);

        // Update provider label when provider changes
        connect(chat_, &ChatEngine::provider_changed,
                input_, &ChatInputBar::update_provider_label);

        // Fallback and cooldown notifications
        connect(chat_, &ChatEngine::fallback_activated, this,
                [this](const QString& from, const QString& to) {
            conversation_->add_system_message(
                tr("Provider fallback: %1 unavailable, switched to %2")
                    .arg(from, to));
        });
        connect(chat_, &ChatEngine::profile_cooldown_started, this,
                [this](const QString& profile_id, const QString& reason) {
            conversation_->add_system_message(
                tr("Auth profile %1 cooled down (%2). Rotating to next profile.")
                    .arg(profile_id, reason));
        });

        // Ctrl+L focuses chat input
        auto* ctrl_l = new QShortcut(QKeySequence("Ctrl+L"), this);
        connect(ctrl_l, &QShortcut::activated, input_, &ChatInputBar::set_focus);

        // Initialize provider label
        input_->update_provider_label(chat_->current_provider());

        // Welcome message with provider info
        auto available = chat_->available_providers();
        QString provider_info = available.isEmpty()
            ? tr("No providers detected. Click the provider button to configure.")
            : tr("Provider: %1 | Available: %2")
                .arg(chat_->current_provider(), available.join(", "));

        conversation_->add_system_message(
            tr("Welcome to Euxis ETX. Active agent: %1.\n%2\n"
               "Type a message, use @ to switch agents, or / for commands.")
                .arg(chat_->active_agent(), provider_info));

        // Focus input on show
        QTimer::singleShot(200, input_, &ChatInputBar::set_focus);
    }

private slots:
    void on_message_submitted(const QString& text) {
        conversation_->add_user_message(text);
        chat_->send_message(text);
    }

    void on_slash_command(const QString& command, const QString& args) {
        if (command == "clear") {
            conversation_->clear_conversation();
            chat_->clear_history();
            conversation_->add_system_message(tr("Conversation cleared."));
        } else if (command == "agent") {
            if (!args.isEmpty()) {
                chat_->set_active_agent(args);
                sidebar_->set_active_agent(args);
                input_->update_placeholder(args);
                conversation_->add_system_message(tr("Switched to %1").arg(args));
            } else {
                conversation_->add_system_message(
                    tr("Usage: /agent <name>. Active: %1").arg(chat_->active_agent()));
            }
        } else if (command == "model") {
            conversation_->add_system_message(
                tr("Active agent: %1").arg(chat_->active_agent()) + "\n" +
                chat_->provider_status_text());
        } else if (command == "help") {
            conversation_->add_system_message(
                tr("Commands: /clear /agent <name> /model /dashboard "
                   "/metrics /logs /playbooks /approvals /cortex "
                   "/omnigraph /help"));
        } else if (command == "dashboard") {
            // Already here
        } else {
            auto* stack = app_->findChild<QStackedWidget*>();
            if (!stack) return;

            if (command == "metrics") stack->setCurrentIndex(12);
            else if (command == "logs") stack->setCurrentIndex(7);
            else if (command == "playbooks") stack->setCurrentIndex(8);
            else if (command == "approvals") stack->setCurrentIndex(9);
            else if (command == "cortex") stack->setCurrentIndex(10);
            else if (command == "omnigraph") stack->setCurrentIndex(13);
            else conversation_->add_system_message(
                tr("Unknown command: /%1").arg(command));
        }
    }

    void on_agent_selected(const QString& agent_id) {
        chat_->set_active_agent(agent_id);
        sidebar_->set_active_agent(agent_id);
        input_->update_placeholder(agent_id);
    }

    void on_provider_button() {
        // Use the enhanced provider select dialog with profile management
        extern QDialog* create_provider_select_dialog(ChatEngine* chat, QWidget* parent);
        auto* dialog = create_provider_select_dialog(chat_, this);

        if (dialog->exec() == QDialog::Accepted) {
            conversation_->add_system_message(
                tr("Provider accounts updated. Active: %1").arg(chat_->current_provider()));
        }
        dialog->deleteLater();
    }

private:
    ChatEngine* chat_;
    EuxisApp* app_;
    ConversationWidget* conversation_;
    ChatInputBar* input_;
    AgentSidebar* sidebar_;
};

QWidget* create_dashboard_screen(EuxisApp* app, FleetRegistry* registry,
                                  ThemeEngine* theme, ETXConfig* config,
                                  ChatEngine* chat, QWidget* parent) {
    return new DashboardScreen(app, registry, theme, config, chat, parent);
}

} // namespace euxis::etx

#include "dashboard.moc"
