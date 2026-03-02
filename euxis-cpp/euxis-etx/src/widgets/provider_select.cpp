#include <euxis/etx/chat_engine.hpp>
#include <euxis/etx/oauth_flow.hpp>

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include <QFont>
#include <QLineEdit>
#include <QComboBox>
#include <QTimer>
#include <QInputDialog>

namespace euxis::etx {

class ProviderSelectDialog : public QDialog {
    Q_OBJECT

public:
    explicit ProviderSelectDialog(ChatEngine* chat, QWidget* parent = nullptr)
        : QDialog(parent)
        , chat_(chat)
    {
        setObjectName("provider_select");
        setWindowTitle(tr("Provider Accounts"));
        setMinimumSize(440, 420);

        setStyleSheet(
            "QDialog { background: #1a1a2e; color: #e0e0e0; }"
            "QLabel { background: transparent; border: none; }"
            "QListWidget { background: rgba(255,255,255,0.03); "
            "border: 1px solid rgba(255,255,255,0.1); border-radius: 6px; "
            "color: #e0e0e0; font-size: 12px; padding: 4px; }"
            "QListWidget::item { padding: 8px 12px; border-radius: 4px; }"
            "QListWidget::item:selected { background: rgba(15,52,96,0.6); }"
            "QListWidget::item:hover { background: rgba(255,255,255,0.05); }"
            "QPushButton { border-radius: 6px; padding: 6px 14px; font-size: 12px; }"
            "QPushButton:hover { opacity: 0.9; }");

        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(16, 16, 16, 16);
        layout->setSpacing(10);

        // Title
        auto* title = new QLabel(tr("Provider Accounts"), this);
        QFont title_font;
        title_font.setPointSize(14);
        title_font.setBold(true);
        title->setFont(title_font);
        title->setAlignment(Qt::AlignCenter);
        title->setStyleSheet("color: #e0e0e0;");
        layout->addWidget(title);

        // Profile list
        profile_list_ = new QListWidget(this);
        profile_list_->setObjectName("profile_list");
        layout->addWidget(profile_list_, 1);

        // OAuth connect buttons
        auto* connect_row = new QHBoxLayout();
        connect_row->setSpacing(8);

        auto* connect_anthropic = new QPushButton(tr("Connect Anthropic"), this);
        connect_anthropic->setStyleSheet(
            "QPushButton { background: rgba(204,120,50,0.3); color: #e8a64c; "
            "border: 1px solid rgba(204,120,50,0.4); }");
        connect_anthropic->setCursor(Qt::PointingHandCursor);
        connect_row->addWidget(connect_anthropic);

        auto* connect_google = new QPushButton(tr("Connect Google"), this);
        connect_google->setStyleSheet(
            "QPushButton { background: rgba(66,133,244,0.3); color: #6db3f2; "
            "border: 1px solid rgba(66,133,244,0.4); }");
        connect_google->setCursor(Qt::PointingHandCursor);
        connect_row->addWidget(connect_google);

        layout->addLayout(connect_row);

        // API key button + Remove button
        auto* action_row = new QHBoxLayout();
        action_row->setSpacing(8);

        auto* add_key_btn = new QPushButton(tr("Add API Key"), this);
        add_key_btn->setStyleSheet(
            "QPushButton { background: rgba(255,255,255,0.05); color: #aaa; "
            "border: 1px solid rgba(255,255,255,0.1); }");
        add_key_btn->setCursor(Qt::PointingHandCursor);
        action_row->addWidget(add_key_btn);

        auto* remove_btn = new QPushButton(tr("Remove"), this);
        remove_btn->setStyleSheet(
            "QPushButton { background: rgba(244,67,54,0.15); color: #e57373; "
            "border: 1px solid rgba(244,67,54,0.3); }");
        remove_btn->setCursor(Qt::PointingHandCursor);
        action_row->addWidget(remove_btn);

        layout->addLayout(action_row);

        // Fallback chain display
        fallback_label_ = new QLabel(this);
        fallback_label_->setStyleSheet("color: #666; font-size: 11px; padding: 4px 0;");
        layout->addWidget(fallback_label_);

        // OK/Cancel buttons
        auto* button_row = new QHBoxLayout();
        button_row->addStretch();

        auto* cancel_btn = new QPushButton(tr("Cancel"), this);
        cancel_btn->setStyleSheet(
            "QPushButton { background: rgba(255,255,255,0.05); color: #aaa; "
            "border: 1px solid rgba(255,255,255,0.1); }");
        button_row->addWidget(cancel_btn);

        auto* ok_btn = new QPushButton(tr("OK"), this);
        ok_btn->setStyleSheet(
            "QPushButton { background: rgba(15,52,96,0.6); color: #e0e0e0; "
            "border: 1px solid rgba(15,52,96,0.8); font-weight: bold; }");
        button_row->addWidget(ok_btn);

        layout->addLayout(button_row);

        // Connections
        connect(cancel_btn, &QPushButton::clicked, this, &QDialog::reject);
        connect(ok_btn, &QPushButton::clicked, this, &QDialog::accept);

        connect(connect_anthropic, &QPushButton::clicked, this, [this]() {
            chat_->start_login("anthropic");
        });
        connect(connect_google, &QPushButton::clicked, this, [this]() {
            chat_->start_login("gemini");
        });
        connect(add_key_btn, &QPushButton::clicked, this, &ProviderSelectDialog::on_add_api_key);
        connect(remove_btn, &QPushButton::clicked, this, &ProviderSelectDialog::on_remove);

        // OAuth flow signals
        if (auto* flow = chat_->oauth_flow()) {
            connect(flow, &OAuthFlow::login_succeeded, this, [this]() {
                refresh_profiles();
            });
            connect(flow, &OAuthFlow::login_failed, this,
                [this](const QString& /*provider*/, const QString& error) {
                    // Show error inline
                    fallback_label_->setText(tr("Login error: %1").arg(error));
                    fallback_label_->setStyleSheet("color: #e57373; font-size: 11px; padding: 4px 0;");
                });
        }

        // Cooldown timer
        auto* cooldown_timer = new QTimer(this);
        connect(cooldown_timer, &QTimer::timeout, this, &ProviderSelectDialog::refresh_profiles);
        cooldown_timer->start(5000);

        refresh_profiles();
    }

    [[nodiscard]] QString selected_provider() const {
        auto* item = profile_list_->currentItem();
        if (!item) return {};
        // Extract provider from profile id (e.g., "anthropic:user@example.com" → "anthropic")
        auto id = item->data(Qt::UserRole).toString();
        return id.section(':', 0, 0);
    }

private slots:
    void on_add_api_key() {
        // Provider selection
        QStringList providers = {"anthropic", "openai", "gemini"};
        bool ok = false;
        QString provider = QInputDialog::getItem(this, tr("Add API Key"),
            tr("Provider:"), providers, 0, false, &ok);
        if (!ok || provider.isEmpty()) return;

        QString key = QInputDialog::getText(this, tr("Add API Key"),
            tr("API Key:"), QLineEdit::Password, {}, &ok);
        if (!ok || key.isEmpty()) return;

        QString label = QInputDialog::getText(this, tr("Add API Key"),
            tr("Label (optional):"), QLineEdit::Normal, {}, &ok);

        chat_->add_api_key(provider, key, label);
        refresh_profiles();
    }

    void on_remove() {
        auto* item = profile_list_->currentItem();
        if (!item) return;

        auto id = item->data(Qt::UserRole).toString();
        chat_->remove_auth_profile(id);
        refresh_profiles();
    }

    void refresh_profiles() {
        profile_list_->clear();

        auto profiles = chat_->auth_store().all_profiles();
        for (const auto& p : profiles) {
            QString status_icon;
            QString status_text;
            auto cooldown = chat_->auth_store().cooldown_remaining_ms(p.id);

            if (cooldown > 0) {
                status_icon = QString::fromUtf8("\xe2\x97\x8f");  // filled circle
                int minutes = static_cast<int>(cooldown / 60000);
                status_text = tr("Cooldown (%1m left)").arg(minutes);
            } else if (p.type == cli::ProfileType::OAuth && p.expires_at > 0) {
                auto now = QDateTime::currentMSecsSinceEpoch();
                if (now >= p.expires_at) {
                    status_icon = QString::fromUtf8("\xe2\x97\x8b");  // empty circle
                    status_text = tr("Expired");
                } else {
                    status_icon = QString::fromUtf8("\xe2\x97\x8f");  // filled circle
                    status_text = tr("Active");
                }
            } else {
                status_icon = QString::fromUtf8("\xe2\x97\x8f");
                status_text = tr("Active");
            }

            QString type_label = p.type == cli::ProfileType::OAuth ? tr("OAUTH") : tr("API KEY");
            QString label = p.label.empty()
                ? QString::fromStdString(p.id)
                : QString::fromStdString(p.label);

            QString display = QString("%1 %2  [%3]\n    %4 %5 %6")
                .arg(status_icon,
                     QString::fromStdString(p.id),
                     type_label,
                     label,
                     QString::fromStdString(p.source).isEmpty()
                         ? "" : "(" + QString::fromStdString(p.source) + ")",
                     status_text);

            auto* item = new QListWidgetItem(display, profile_list_);
            item->setData(Qt::UserRole, QString::fromStdString(p.id));

            // Color based on status
            if (cooldown > 0) {
                item->setForeground(QColor("#ff9800"));  // orange for cooldown
            } else if (p.type == cli::ProfileType::OAuth && p.expires_at > 0 &&
                       QDateTime::currentMSecsSinceEpoch() >= p.expires_at) {
                item->setForeground(QColor("#f44336"));  // red for expired
            } else {
                item->setForeground(QColor("#4caf50"));  // green for active
            }
        }

        // Update fallback chain display
        auto current = chat_->current_provider().toStdString();
        auto chain = chat_->auth_store().fallback_chain(current);
        QString chain_text = tr("Fallback: %1").arg(chat_->current_provider());
        for (const auto& fb : chain) {
            chain_text += QString::fromUtf8(" \xe2\x86\x92 ") + QString::fromStdString(fb);
        }
        fallback_label_->setText(chain_text);
        fallback_label_->setStyleSheet("color: #666; font-size: 11px; padding: 4px 0;");
    }

private:
    ChatEngine* chat_;
    QListWidget* profile_list_;
    QLabel* fallback_label_;
};

QDialog* create_provider_select_dialog(ChatEngine* chat, QWidget* parent) {
    return new ProviderSelectDialog(chat, parent);
}

} // namespace euxis::etx

#include "provider_select.moc"
