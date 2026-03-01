#include <QHBoxLayout>
#include <QLabel>
#include <QFont>
#include <QWidget>

namespace euxis::etx {

QWidget* create_shortcut_bar_widget(QWidget* parent) {
    auto* widget = new QWidget(parent);
    widget->setObjectName("shortcut_bar");
    widget->setFixedHeight(32);

    auto* layout = new QHBoxLayout(widget);
    layout->setContentsMargins(16, 0, 16, 0);
    layout->setSpacing(24);

    QFont hint_font;
    hint_font.setPointSize(10);
    hint_font.setFamily("monospace");

    struct Hint {
        QString shortcut;
        QString action;
    };

    QList<Hint> hints = {
        {"Ctrl+K", "Command Palette"},
        {"F3",     "Theme"},
        {"F5",     "Refresh"},
        {"Ctrl+Q", "Quit"},
    };

    layout->addStretch();

    for (const auto& h : hints) {
        auto* key_label = new QLabel(h.shortcut, widget);
        key_label->setFont(hint_font);
        key_label->setStyleSheet(
            "color: #666; background: rgba(255,255,255,0.05); "
            "border: 1px solid rgba(255,255,255,0.08); border-radius: 3px; "
            "padding: 2px 6px;");
        layout->addWidget(key_label);

        auto* action_label = new QLabel(h.action, widget);
        action_label->setFont(hint_font);
        action_label->setStyleSheet(
            "color: #555; background: transparent; border: none;");
        layout->addWidget(action_label);

        if (&h != &hints.last()) {
            auto* separator = new QLabel("|", widget);
            separator->setStyleSheet("color: #333; background: transparent; border: none;");
            layout->addWidget(separator);
        }
    }

    layout->addStretch();

    return widget;
}

} // namespace euxis::etx
