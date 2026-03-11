#include <QHBoxLayout>
#include <QLineEdit>
#include <QLabel>
#include <QWidget>
#include <QCoreApplication>

namespace euxis::etx {

QWidget* create_search_bar_widget(QWidget* parent) {
    auto* widget = new QWidget(parent);
    widget->setObjectName("search_bar_widget");
    widget->setFixedHeight(48);

    auto* layout = new QHBoxLayout(widget);
    layout->setContentsMargins(0, 4, 0, 4);
    layout->setSpacing(8);

    // Search icon (text-based)
    auto* icon = new QLabel("/", widget);
    icon->setFixedWidth(24);
    icon->setAlignment(Qt::AlignCenter);
    icon->setStyleSheet(
        "color: #666; background: rgba(255,255,255,0.05); "
        "border: 1px solid rgba(255,255,255,0.1); border-radius: 4px; "
        "font-family: monospace; font-weight: bold;");
    layout->addWidget(icon);

    // Search input
    auto* input = new QLineEdit(widget);
    input->setObjectName("search_input");
    input->setPlaceholderText(QCoreApplication::translate("SearchBar", "Search agents..."));
    input->setMinimumHeight(36);
    input->setStyleSheet(
        "QLineEdit { background: rgba(255,255,255,0.03); "
        "border: 1px solid rgba(255,255,255,0.1); border-radius: 6px; "
        "padding: 6px 12px; color: #e0e0e0; font-size: 13px; }"
        "QLineEdit:focus { border-color: rgba(15,52,96,0.8); "
        "background: rgba(255,255,255,0.05); }");
    layout->addWidget(input);

    return widget;
}

} // namespace euxis::etx
