#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include <QFont>

namespace euxis::etx {

class ProviderSelectDialog : public QDialog {
    Q_OBJECT

public:
    explicit ProviderSelectDialog(QWidget* parent = nullptr)
        : QDialog(parent)
    {
        setObjectName("provider_select");
        setWindowTitle("Select Provider");
        setFixedSize(320, 280);

        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(16, 16, 16, 16);
        layout->setSpacing(12);

        // Title
        auto* title = new QLabel("Select Provider", this);
        title->setObjectName("provider_select_title");
        QFont title_font;
        title_font.setPointSize(14);
        title_font.setBold(true);
        title->setFont(title_font);
        title->setAlignment(Qt::AlignCenter);
        title->setStyleSheet("color: #e0e0e0; background: transparent; border: none;");
        layout->addWidget(title);

        // Provider list
        list_ = new QListWidget(this);
        list_->setObjectName("provider_list");

        struct Provider {
            QString label;
            QString value;
        };

        QList<Provider> providers = {
            {"Claude (Anthropic)", "anthropic"},
            {"GPT-4 (OpenAI)",    "openai"},
            {"Gemini (Google)",   "google"},
            {"Local (Ollama)",    "ollama"},
        };

        for (const auto& p : providers) {
            auto* item = new QListWidgetItem(p.label, list_);
            item->setData(Qt::UserRole, p.value);
        }

        list_->setCurrentRow(0);
        list_->setStyleSheet(
            "QListWidget { background: rgba(255,255,255,0.03); "
            "border: 1px solid rgba(255,255,255,0.1); border-radius: 6px; "
            "color: #e0e0e0; font-size: 13px; padding: 4px; }"
            "QListWidget::item { padding: 8px 12px; border-radius: 4px; }"
            "QListWidget::item:selected { background: rgba(15,52,96,0.6); "
            "color: #ffffff; }"
            "QListWidget::item:hover { background: rgba(255,255,255,0.05); }");
        layout->addWidget(list_);

        // Buttons
        auto* button_row = new QHBoxLayout();
        button_row->addStretch();

        auto* cancel_btn = new QPushButton("Cancel", this);
        cancel_btn->setObjectName("provider_cancel_btn");
        cancel_btn->setStyleSheet(
            "QPushButton { background: rgba(255,255,255,0.05); color: #aaa; "
            "border: 1px solid rgba(255,255,255,0.1); border-radius: 4px; "
            "padding: 6px 16px; font-size: 12px; }"
            "QPushButton:hover { background: rgba(255,255,255,0.1); }");
        button_row->addWidget(cancel_btn);

        auto* select_btn = new QPushButton("Select", this);
        select_btn->setObjectName("provider_select_btn");
        select_btn->setStyleSheet(
            "QPushButton { background: rgba(15,52,96,0.6); color: #e0e0e0; "
            "border: 1px solid rgba(15,52,96,0.8); border-radius: 4px; "
            "padding: 6px 16px; font-size: 12px; font-weight: bold; }"
            "QPushButton:hover { background: rgba(15,52,96,0.8); }");
        button_row->addWidget(select_btn);

        layout->addLayout(button_row);

        // Connections
        connect(cancel_btn, &QPushButton::clicked, this, &QDialog::reject);
        connect(select_btn, &QPushButton::clicked, this, &QDialog::accept);
        connect(list_, &QListWidget::itemDoubleClicked, this, &QDialog::accept);
    }

    [[nodiscard]] QString selected_provider() const {
        auto* item = list_->currentItem();
        if (!item) return {};
        return item->data(Qt::UserRole).toString();
    }

private:
    QListWidget* list_;
};

QDialog* create_provider_select_dialog(QWidget* parent) {
    return new ProviderSelectDialog(parent);
}

} // namespace euxis::etx

#include "provider_select.moc"
