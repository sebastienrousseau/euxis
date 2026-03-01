#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QHeaderView>
#include <QFont>
#include <QWidget>
#include <QStackedWidget>

namespace euxis::etx {

QWidget* create_help_screen(QWidget* parent) {
    auto* widget = new QWidget(parent);
    auto* layout = new QVBoxLayout(widget);
    layout->setContentsMargins(32, 32, 32, 32);
    layout->setSpacing(16);

    // Back button
    auto* top_bar = new QHBoxLayout();
    auto* back_btn = new QPushButton("< Back", widget);
    back_btn->setCursor(Qt::PointingHandCursor);
    back_btn->setFixedWidth(100);
    top_bar->addWidget(back_btn);
    top_bar->addStretch();
    layout->addLayout(top_bar);

    QObject::connect(back_btn, &QPushButton::clicked, widget, [widget]() {
        if (auto* stack = qobject_cast<QStackedWidget*>(widget->parentWidget())) {
            stack->setCurrentIndex(1);
        }
    });

    // Title
    auto* title = new QLabel("Keyboard Shortcuts", widget);
    QFont title_font;
    title_font.setPointSize(20);
    title_font.setBold(true);
    title->setFont(title_font);
    layout->addWidget(title);

    // Shortcuts table
    auto* table = new QTableWidget(widget);
    table->setObjectName("shortcuts_table");
    table->setColumnCount(2);
    table->setHorizontalHeaderLabels({"Shortcut", "Action"});
    table->horizontalHeader()->setStretchLastSection(true);
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    table->verticalHeader()->setVisible(false);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setAlternatingRowColors(true);
    table->setShowGrid(false);

    struct Shortcut {
        QString key;
        QString action;
    };

    QList<Shortcut> shortcuts = {
        {"Ctrl+K",  "Open Command Palette"},
        {"Ctrl+P",  "Open Command Palette (alternative)"},
        {"/",       "Open Command Palette (vim-style)"},
        {"Ctrl+Q",  "Quit Application"},
        {"F3",      "Cycle Theme (liquid-glass / calm / focused)"},
        {"F5",      "Refresh Fleet Registry"},
        {"Enter",   "Continue from Welcome Screen"},
        {"J / K",   "Vim-style navigation (in lists)"},
        {"Escape",  "Close dialogs / Go back"},
    };

    table->setRowCount(shortcuts.size());
    for (int i = 0; i < shortcuts.size(); ++i) {
        auto* key_item = new QTableWidgetItem(shortcuts[i].key);
        key_item->setTextAlignment(Qt::AlignCenter);
        QFont key_font;
        key_font.setFamily("monospace");
        key_font.setBold(true);
        key_item->setFont(key_font);
        table->setItem(i, 0, key_item);

        auto* action_item = new QTableWidgetItem(shortcuts[i].action);
        table->setItem(i, 1, action_item);

        table->setRowHeight(i, 40);
    }

    table->setStyleSheet(
        "QTableWidget { background: rgba(255,255,255,0.03); "
        "border: 1px solid rgba(255,255,255,0.1); border-radius: 8px; }"
        "QTableWidget::item { padding: 8px; }"
        "QTableWidget::item:alternate { background: rgba(255,255,255,0.02); }"
        "QHeaderView::section { background: rgba(255,255,255,0.05); "
        "padding: 8px; border: none; font-weight: bold; }");

    layout->addWidget(table, 1);

    return widget;
}

} // namespace euxis::etx
