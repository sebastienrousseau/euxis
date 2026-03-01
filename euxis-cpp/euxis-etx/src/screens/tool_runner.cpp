#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QFont>
#include <QFrame>
#include <QWidget>
#include <QStackedWidget>
#include <QShortcut>
#include <QDateTime>

namespace euxis::etx {

QWidget* create_tool_runner_screen(QWidget* parent) {
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

    // Title (tool name placeholder)
    auto* title = new QLabel("Tool Runner", widget);
    title->setObjectName("tool_runner_title");
    QFont title_font;
    title_font.setPointSize(20);
    title_font.setBold(true);
    title->setFont(title_font);
    layout->addWidget(title);

    auto* tool_name = new QLabel("Tool: euxis-bridge verify", widget);
    tool_name->setObjectName("tool_name_label");
    tool_name->setStyleSheet("color: #888; font-size: 13px;");
    layout->addWidget(tool_name);

    // Separator
    auto* separator = new QFrame(widget);
    separator->setFrameShape(QFrame::HLine);
    separator->setStyleSheet("color: rgba(255,255,255,0.1);");
    layout->addWidget(separator);

    // Output display
    auto* output = new QPlainTextEdit(widget);
    output->setObjectName("tool_output");
    output->setReadOnly(true);
    QFont mono_font;
    mono_font.setFamily("monospace");
    mono_font.setPointSize(11);
    output->setFont(mono_font);
    output->setMaximumBlockCount(10000);
    output->setStyleSheet(
        "QPlainTextEdit { background: rgba(0,0,0,0.3); color: #c8c8c8; "
        "border: 1px solid rgba(255,255,255,0.1); border-radius: 8px; "
        "padding: 12px; }");

    // Populate with sample output
    auto now = QDateTime::currentDateTime();
    output->appendPlainText(
        now.addSecs(-10).toString("[hh:mm:ss]") +
        " euxis-bridge verify --source clawshub --skill data-parser");
    output->appendPlainText(
        now.addSecs(-9).toString("[hh:mm:ss]") +
        " Fetching manifest from ClawHub registry...");
    output->appendPlainText(
        now.addSecs(-7).toString("[hh:mm:ss]") +
        " Downloaded SKILL.md (2.4 KB) and openclaw.json (1.1 KB)");
    output->appendPlainText(
        now.addSecs(-6).toString("[hh:mm:ss]") +
        " Verifying signature: SHA-256 RSA-PKCS1-v1_5 ... OK");
    output->appendPlainText(
        now.addSecs(-5).toString("[hh:mm:ss]") +
        " Verifying content hash: SHA-256 ... OK");
    output->appendPlainText(
        now.addSecs(-4).toString("[hh:mm:ss]") +
        " WASM sandbox test: loading skill into Extism runtime...");
    output->appendPlainText(
        now.addSecs(-3).toString("[hh:mm:ss]") +
        " WASM sandbox test: execution completed (42ms, 0 violations)");
    output->appendPlainText(
        now.addSecs(-2).toString("[hh:mm:ss]") +
        " Supply chain check: 0 known vulnerabilities in dependencies");
    output->appendPlainText(
        now.addSecs(-1).toString("[hh:mm:ss]") +
        " ClawHavoc threat scan: CLEAN (no malicious patterns detected)");
    output->appendPlainText(
        now.toString("[hh:mm:ss]") +
        " RESULT: PASS — skill 'data-parser' verified for import");

    layout->addWidget(output, 1);

    // Button row
    auto* button_row = new QHBoxLayout();
    button_row->addStretch();

    auto* clear_btn = new QPushButton("Clear (Ctrl+L)", widget);
    clear_btn->setObjectName("clear_button");
    clear_btn->setCursor(Qt::PointingHandCursor);
    clear_btn->setMinimumHeight(40);
    clear_btn->setMinimumWidth(130);
    button_row->addWidget(clear_btn);

    auto* run_btn = new QPushButton("Run", widget);
    run_btn->setObjectName("run_button");
    run_btn->setCursor(Qt::PointingHandCursor);
    run_btn->setMinimumHeight(40);
    run_btn->setMinimumWidth(100);
    run_btn->setStyleSheet(
        "QPushButton { background: #0f3460; color: #fff; border: none; "
        "border-radius: 6px; font-weight: bold; font-size: 13px; padding: 8px 24px; }"
        "QPushButton:hover { background: #1a4a7a; }");
    button_row->addWidget(run_btn);

    layout->addLayout(button_row);

    // Clear action
    QObject::connect(clear_btn, &QPushButton::clicked, output, &QPlainTextEdit::clear);

    // Run action (placeholder: append simulated output)
    QObject::connect(run_btn, &QPushButton::clicked, widget, [output, tool_name]() {
        auto ts = QDateTime::currentDateTime().toString("[hh:mm:ss]");
        output->appendPlainText("");
        output->appendPlainText(ts + " Re-running: " + tool_name->text().mid(6));
        output->appendPlainText(ts + " Executing...");
    });

    // Ctrl+L to clear
    auto* shortcut_clear = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_L), widget);
    QObject::connect(shortcut_clear, &QShortcut::activated, output, &QPlainTextEdit::clear);

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
