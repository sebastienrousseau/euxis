#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QFont>
#include <QWidget>
#include <QStackedWidget>
#include <QDateTime>

namespace euxis::etx {

QWidget* create_logs_screen(QWidget* parent) {
    auto* widget = new QWidget(parent);
    auto* layout = new QVBoxLayout(widget);
    layout->setContentsMargins(32, 32, 32, 32);
    layout->setSpacing(16);

    // Top bar with back button
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
    auto* title = new QLabel("System Logs", widget);
    QFont title_font;
    title_font.setPointSize(20);
    title_font.setBold(true);
    title->setFont(title_font);
    layout->addWidget(title);

    // Log display
    auto* log_edit = new QPlainTextEdit(widget);
    log_edit->setObjectName("log_display");
    log_edit->setReadOnly(true);
    QFont mono_font;
    mono_font.setFamily("monospace");
    mono_font.setPointSize(11);
    log_edit->setFont(mono_font);
    log_edit->setMaximumBlockCount(10000);
    log_edit->setStyleSheet(
        "QPlainTextEdit { background: rgba(0,0,0,0.3); color: #c8c8c8; "
        "border: 1px solid rgba(255,255,255,0.1); border-radius: 8px; "
        "padding: 12px; }");

    // Populate with sample log entries
    auto now = QDateTime::currentDateTime();
    log_edit->appendPlainText(
        now.addSecs(-300).toString("[yyyy-MM-dd hh:mm:ss]") +
        " [INFO] Euxis ETX v0.0.3 initialized");
    log_edit->appendPlainText(
        now.addSecs(-280).toString("[yyyy-MM-dd hh:mm:ss]") +
        " [INFO] ThemeEngine: applied liquid-glass theme");
    log_edit->appendPlainText(
        now.addSecs(-260).toString("[yyyy-MM-dd hh:mm:ss]") +
        " [INFO] FleetRegistry: loaded 8 agents");
    log_edit->appendPlainText(
        now.addSecs(-200).toString("[yyyy-MM-dd hh:mm:ss]") +
        " [INFO] CryptoModule: AES-256-GCM backend ready (Rust/PyO3)");
    log_edit->appendPlainText(
        now.addSecs(-150).toString("[yyyy-MM-dd hh:mm:ss]") +
        " [INFO] BridgeModule: OpenClaw interop layer initialized");
    log_edit->appendPlainText(
        now.addSecs(-120).toString("[yyyy-MM-dd hh:mm:ss]") +
        " [WARN] IdentityAgent: KYA/DID endpoint unreachable, retrying...");
    log_edit->appendPlainText(
        now.addSecs(-90).toString("[yyyy-MM-dd hh:mm:ss]") +
        " [ERROR] IdentityAgent: failed to connect after 3 retries");
    log_edit->appendPlainText(
        now.addSecs(-60).toString("[yyyy-MM-dd hh:mm:ss]") +
        " [INFO] InferenceAgent: local model endpoint available at :8080");
    log_edit->appendPlainText(
        now.addSecs(-30).toString("[yyyy-MM-dd hh:mm:ss]") +
        " [INFO] MemoryAgent: encrypted checkpoint loaded (AES-256-GCM)");
    log_edit->appendPlainText(
        now.toString("[yyyy-MM-dd hh:mm:ss]") +
        " [INFO] ResearchAgent: task 'audit-openclaw-security' running");

    layout->addWidget(log_edit, 1);

    // Button row
    auto* button_row = new QHBoxLayout();
    button_row->addStretch();

    auto* clear_btn = new QPushButton("Clear", widget);
    clear_btn->setCursor(Qt::PointingHandCursor);
    clear_btn->setMinimumHeight(36);
    clear_btn->setMinimumWidth(100);
    button_row->addWidget(clear_btn);

    auto* refresh_btn = new QPushButton("Refresh", widget);
    refresh_btn->setCursor(Qt::PointingHandCursor);
    refresh_btn->setMinimumHeight(36);
    refresh_btn->setMinimumWidth(100);
    button_row->addWidget(refresh_btn);

    layout->addLayout(button_row);

    QObject::connect(clear_btn, &QPushButton::clicked, log_edit, &QPlainTextEdit::clear);

    QObject::connect(refresh_btn, &QPushButton::clicked, widget, [log_edit]() {
        auto ts = QDateTime::currentDateTime().toString("[yyyy-MM-dd hh:mm:ss]");
        log_edit->appendPlainText(ts + " [INFO] Log refreshed");
    });

    return widget;
}

} // namespace euxis::etx
