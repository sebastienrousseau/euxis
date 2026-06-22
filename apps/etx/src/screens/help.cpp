#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QHeaderView>
#include <QFont>
#include <QWidget>
#include <QStackedWidget>
#include <QCoreApplication>

namespace euxis::etx {

QWidget* create_help_screen(QWidget* parent) {
    auto* widget = new QWidget(parent);
    auto* layout = new QVBoxLayout(widget);
    layout->setContentsMargins(32, 32, 32, 32);
    layout->setSpacing(16);

    // Back button
    auto* top_bar = new QHBoxLayout();
    auto* back_btn = new QPushButton(QCoreApplication::translate("HelpScreen", "< Back"), widget);
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
    auto* title = new QLabel(QCoreApplication::translate("HelpScreen", "Keyboard Shortcuts"), widget);
    QFont title_font;
    title_font.setPointSize(20);
    title_font.setBold(true);
    title->setFont(title_font);
    layout->addWidget(title);

    // Shortcuts table
    auto* table = new QTableWidget(widget);
    table->setObjectName("shortcuts_table");
    table->setColumnCount(2);
    table->setHorizontalHeaderLabels({QCoreApplication::translate("HelpScreen", "Shortcut"), QCoreApplication::translate("HelpScreen", "Action")});
    table->horizontalHeader()->setStretchLastSection(true);
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    table->verticalHeader()->setVisible(false);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setAlternatingRowColors(true);
    table->setShowGrid(false);

    struct Shortcut {
        QString key{};
        QString action{};
    };

    QList<Shortcut> shortcuts = {
        {"Ctrl+K",  QCoreApplication::translate("HelpScreen", "Open Command Palette")},
        {"Ctrl+P",  QCoreApplication::translate("HelpScreen", "Open Command Palette (alternative)")},
        {"/",       QCoreApplication::translate("HelpScreen", "Open Command Palette (vim-style)")},
        {"Ctrl+Q",  QCoreApplication::translate("HelpScreen", "Quit Application")},
        {"F3",      QCoreApplication::translate("HelpScreen", "Cycle Theme (liquid-glass / calm / focused)")},
        {"F5",      QCoreApplication::translate("HelpScreen", "Refresh Fleet Registry")},
        {"Enter",   QCoreApplication::translate("HelpScreen", "Continue from Welcome Screen")},
        {"J / K",   QCoreApplication::translate("HelpScreen", "Vim-style navigation (in lists)")},
        {"Escape",  QCoreApplication::translate("HelpScreen", "Close dialogs / Go back")},
    };

    table->setRowCount(static_cast<int>(shortcuts.size()));
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
