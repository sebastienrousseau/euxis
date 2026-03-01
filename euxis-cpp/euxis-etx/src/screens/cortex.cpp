#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QFont>
#include <QFrame>
#include <QWidget>
#include <QStackedWidget>
#include <QShortcut>
#include <QDateTime>

namespace euxis::etx {

QWidget* create_cortex_screen(QWidget* parent) {
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
    auto* title = new QLabel("Cortex Memory Browser", widget);
    QFont title_font;
    title_font.setPointSize(20);
    title_font.setBold(true);
    title->setFont(title_font);
    layout->addWidget(title);

    // Status label
    auto* status_label = new QLabel(widget);
    status_label->setObjectName("cortex_status");
    status_label->setText("Cortex: 1,247 embeddings | 42 namespaces | Last sync: "
                          + QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm"));
    status_label->setStyleSheet("color: #888; font-size: 12px;");
    layout->addWidget(status_label);

    // Separator
    auto* separator = new QFrame(widget);
    separator->setFrameShape(QFrame::HLine);
    separator->setStyleSheet("color: rgba(255,255,255,0.1);");
    layout->addWidget(separator);

    // Query input row
    auto* query_row = new QHBoxLayout();

    auto* query_input = new QLineEdit(widget);
    query_input->setObjectName("cortex_query_input");
    query_input->setPlaceholderText("Search cortex memory...");
    query_input->setMinimumHeight(40);
    query_input->setStyleSheet(
        "QLineEdit { background: rgba(255,255,255,0.05); "
        "border: 1px solid rgba(255,255,255,0.15); border-radius: 6px; "
        "padding: 8px 12px; color: #ddd; font-size: 13px; }"
        "QLineEdit:focus { border-color: rgba(15,52,96,0.8); }");
    query_row->addWidget(query_input);

    auto* search_btn = new QPushButton("Search", widget);
    search_btn->setObjectName("cortex_search_button");
    search_btn->setCursor(Qt::PointingHandCursor);
    search_btn->setMinimumHeight(40);
    search_btn->setMinimumWidth(100);
    search_btn->setStyleSheet(
        "QPushButton { background: #0f3460; color: #fff; border: none; "
        "border-radius: 6px; font-weight: bold; font-size: 13px; padding: 8px 24px; }"
        "QPushButton:hover { background: #1a4a7a; }");
    query_row->addWidget(search_btn);

    layout->addLayout(query_row);

    // Output display
    auto* output = new QPlainTextEdit(widget);
    output->setObjectName("cortex_output");
    output->setReadOnly(true);
    QFont mono_font;
    mono_font.setFamily("monospace");
    mono_font.setPointSize(11);
    output->setFont(mono_font);
    output->setStyleSheet(
        "QPlainTextEdit { background: rgba(0,0,0,0.3); color: #c8c8c8; "
        "border: 1px solid rgba(255,255,255,0.1); border-radius: 8px; "
        "padding: 12px; }");
    output->setPlainText(
        "Cortex semantic memory engine ready.\n"
        "Enter a query to search across agent memory stores.\n\n"
        "Supported query types:\n"
        "  - Natural language: \"What did the security agent find?\"\n"
        "  - Namespace filter: ns:bridge-agent <query>\n"
        "  - Similarity threshold: threshold:0.8 <query>\n"
        "  - Time range: after:2026-02-01 <query>");
    layout->addWidget(output, 1);

    // Search action
    auto search_action = [query_input, output, status_label]() {
        QString query = query_input->text().trimmed();
        if (query.isEmpty()) return;

        auto ts = QDateTime::currentDateTime().toString("[hh:mm:ss]");
        output->appendPlainText("\n" + ts + " Query: " + query);
        output->appendPlainText(QString(60, '-'));

        // Placeholder results (subprocess integration point)
        output->appendPlainText("  [0.94] security-agent/audit-2026-02 :");
        output->appendPlainText("         \"Supply chain verification passed for 184 dependencies.\"");
        output->appendPlainText("  [0.87] bridge-agent/import-log :");
        output->appendPlainText("         \"ClawHub skill 'data-parser' imported with WASM sandbox.\"");
        output->appendPlainText("  [0.81] research-agent/findings :");
        output->appendPlainText("         \"OpenClaw ClawHavoc: 20% malicious skill rate confirmed.\"");
        output->appendPlainText(QString("\n  3 results (%.1fms)").arg(12.4));

        status_label->setText("Cortex: 1,247 embeddings | 42 namespaces | Last query: "
                              + QDateTime::currentDateTime().toString("hh:mm:ss"));
        query_input->clear();
    };

    QObject::connect(search_btn, &QPushButton::clicked, widget, search_action);
    QObject::connect(query_input, &QLineEdit::returnPressed, widget, search_action);

    // Escape to go back
    auto* shortcut_esc = new QShortcut(QKeySequence(Qt::Key_Escape), widget);
    QObject::connect(shortcut_esc, &QShortcut::activated, widget, [widget]() {
        if (auto* stack = qobject_cast<QStackedWidget*>(widget->parentWidget())) {
            stack->setCurrentIndex(1);
        }
    });

    return widget;
}

} // namespace euxis::etx
