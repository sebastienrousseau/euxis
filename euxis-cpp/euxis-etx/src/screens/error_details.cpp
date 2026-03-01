#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QProgressBar>
#include <QListWidget>
#include <QTextEdit>
#include <QFont>
#include <QFrame>
#include <QWidget>
#include <QStackedWidget>
#include <QShortcut>

namespace euxis::etx {

QWidget* create_error_details_screen(QWidget* parent) {
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
    auto* title = new QLabel("Error Analysis", widget);
    QFont title_font;
    title_font.setPointSize(20);
    title_font.setBold(true);
    title->setFont(title_font);
    layout->addWidget(title);

    // Error info card
    auto* error_card = new QFrame(widget);
    error_card->setObjectName("error_info_card");
    error_card->setFrameShape(QFrame::StyledPanel);
    auto* error_layout = new QVBoxLayout(error_card);
    error_layout->setSpacing(10);
    error_layout->setContentsMargins(20, 20, 20, 20);

    auto add_info_row = [&](const QString& label, const QString& value,
                            const QString& value_style = "color: #ccc;") {
        auto* row = new QHBoxLayout();
        auto* lbl = new QLabel(label, error_card);
        lbl->setStyleSheet("color: #888; font-weight: bold;");
        lbl->setFixedWidth(140);
        row->addWidget(lbl);

        auto* val = new QLabel(value, error_card);
        val->setStyleSheet(value_style);
        val->setWordWrap(true);
        row->addWidget(val);
        row->addStretch();
        error_layout->addLayout(row);
    };

    add_info_row("Timestamp:", "2026-03-01 14:23:07 UTC");
    add_info_row("Agent:", "identity-agent");
    add_info_row("Error:", "KYA/DID endpoint unreachable after 3 retries "
                 "(ConnectionError: ETIMEDOUT)", "color: #f44336;");
    add_info_row("Last Task:", "Verify agent identity for run-0041");

    layout->addWidget(error_card);

    // Analysis section
    auto* analysis_label = new QLabel("Analysis", widget);
    QFont analysis_font;
    analysis_font.setPointSize(16);
    analysis_font.setBold(true);
    analysis_label->setFont(analysis_font);
    layout->addWidget(analysis_label);

    auto* analysis_card = new QFrame(widget);
    analysis_card->setObjectName("analysis_card");
    analysis_card->setFrameShape(QFrame::StyledPanel);
    auto* analysis_layout = new QVBoxLayout(analysis_card);
    analysis_layout->setSpacing(12);
    analysis_layout->setContentsMargins(20, 20, 20, 20);

    // Severity
    auto* severity_row = new QHBoxLayout();
    auto* severity_label = new QLabel("Severity:", analysis_card);
    severity_label->setStyleSheet("color: #888; font-weight: bold;");
    severity_label->setFixedWidth(140);
    severity_row->addWidget(severity_label);

    auto* severity_badge = new QLabel("HIGH", analysis_card);
    severity_badge->setObjectName("severity_badge");
    severity_badge->setStyleSheet(
        "background: #d32f2f; color: #fff; padding: 4px 16px; "
        "border-radius: 4px; font-weight: bold; font-size: 12px;");
    severity_badge->setFixedWidth(80);
    severity_badge->setAlignment(Qt::AlignCenter);
    severity_row->addWidget(severity_badge);
    severity_row->addStretch();
    analysis_layout->addLayout(severity_row);

    // Confidence
    auto* confidence_row = new QHBoxLayout();
    auto* confidence_label = new QLabel("Confidence:", analysis_card);
    confidence_label->setStyleSheet("color: #888; font-weight: bold;");
    confidence_label->setFixedWidth(140);
    confidence_row->addWidget(confidence_label);

    auto* confidence_bar = new QProgressBar(analysis_card);
    confidence_bar->setObjectName("confidence_bar");
    confidence_bar->setRange(0, 100);
    confidence_bar->setValue(87);
    confidence_bar->setFormat("%p%");
    confidence_bar->setMinimumWidth(200);
    confidence_bar->setMaximumWidth(300);
    confidence_bar->setMinimumHeight(24);
    confidence_bar->setStyleSheet(
        "QProgressBar { background: rgba(255,255,255,0.05); "
        "border: 1px solid rgba(255,255,255,0.1); border-radius: 4px; "
        "text-align: center; color: #ccc; }"
        "QProgressBar::chunk { background: #0f3460; border-radius: 3px; }");
    confidence_row->addWidget(confidence_bar);
    confidence_row->addStretch();
    analysis_layout->addLayout(confidence_row);

    // Recovery steps
    auto* steps_label = new QLabel("Recovery Steps:", analysis_card);
    steps_label->setStyleSheet("color: #888; font-weight: bold;");
    analysis_layout->addWidget(steps_label);

    auto* steps_list = new QListWidget(analysis_card);
    steps_list->setObjectName("recovery_steps");
    steps_list->addItem("1. Check KYA/DID endpoint availability at did.euxis.io");
    steps_list->addItem("2. Verify network connectivity and DNS resolution");
    steps_list->addItem("3. Check identity-agent retry configuration (max_retries: 3)");
    steps_list->addItem("4. Fallback: use cached identity token if within TTL");
    steps_list->addItem("5. Escalate to ops-squad if endpoint remains unreachable");
    steps_list->setMaximumHeight(160);
    steps_list->setStyleSheet(
        "QListWidget { background: rgba(255,255,255,0.03); "
        "border: 1px solid rgba(255,255,255,0.1); border-radius: 8px; "
        "padding: 8px; }"
        "QListWidget::item { padding: 6px 12px; color: #bbb; }");
    analysis_layout->addWidget(steps_list);

    layout->addWidget(analysis_card);

    // Audit trail
    auto* audit_label = new QLabel("Delegation Chain", widget);
    QFont audit_font;
    audit_font.setPointSize(14);
    audit_font.setBold(true);
    audit_label->setFont(audit_font);
    layout->addWidget(audit_label);

    auto* audit_text = new QTextEdit(widget);
    audit_text->setObjectName("audit_trail");
    audit_text->setReadOnly(true);
    QFont mono_font;
    mono_font.setFamily("monospace");
    mono_font.setPointSize(11);
    audit_text->setFont(mono_font);
    audit_text->setMaximumHeight(120);
    audit_text->setStyleSheet(
        "QTextEdit { background: rgba(0,0,0,0.3); color: #c8c8c8; "
        "border: 1px solid rgba(255,255,255,0.1); border-radius: 8px; "
        "padding: 12px; }");
    audit_text->setPlainText(
        "[14:23:01] ops-combo -> identity-agent: verify_identity(run-0041)\n"
        "[14:23:02] identity-agent -> KYA/DID endpoint: POST /v1/verify\n"
        "[14:23:05] identity-agent: retry 1/3 (ETIMEDOUT)\n"
        "[14:23:06] identity-agent: retry 2/3 (ETIMEDOUT)\n"
        "[14:23:07] identity-agent: retry 3/3 (ETIMEDOUT) -> ERROR raised\n"
        "[14:23:07] ops-combo: escalation triggered for identity-agent");
    layout->addWidget(audit_text);

    // Action buttons
    auto* button_row = new QHBoxLayout();
    button_row->addStretch();

    auto* restart_btn = new QPushButton("Restart Agent", widget);
    restart_btn->setObjectName("restart_button");
    restart_btn->setCursor(Qt::PointingHandCursor);
    restart_btn->setMinimumHeight(40);
    restart_btn->setMinimumWidth(130);
    restart_btn->setStyleSheet(
        "QPushButton { background: #0f3460; color: #fff; border: none; "
        "border-radius: 6px; font-weight: bold; font-size: 13px; padding: 8px 24px; }"
        "QPushButton:hover { background: #1a4a7a; }");
    button_row->addWidget(restart_btn);

    auto* simulate_btn = new QPushButton("Simulate Fix", widget);
    simulate_btn->setObjectName("simulate_button");
    simulate_btn->setCursor(Qt::PointingHandCursor);
    simulate_btn->setMinimumHeight(40);
    simulate_btn->setMinimumWidth(130);
    button_row->addWidget(simulate_btn);

    auto* logs_btn = new QPushButton("View Logs", widget);
    logs_btn->setObjectName("view_logs_button");
    logs_btn->setCursor(Qt::PointingHandCursor);
    logs_btn->setMinimumHeight(40);
    logs_btn->setMinimumWidth(130);
    button_row->addWidget(logs_btn);

    auto* close_btn = new QPushButton("Close", widget);
    close_btn->setObjectName("close_button");
    close_btn->setCursor(Qt::PointingHandCursor);
    close_btn->setMinimumHeight(40);
    close_btn->setMinimumWidth(100);
    button_row->addWidget(close_btn);

    layout->addLayout(button_row);

    QObject::connect(close_btn, &QPushButton::clicked, widget, [widget]() {
        if (auto* stack = qobject_cast<QStackedWidget*>(widget->parentWidget())) {
            stack->setCurrentIndex(1);
        }
    });

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
