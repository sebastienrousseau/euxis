#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QHeaderView>
#include <QTextEdit>
#include <QFont>
#include <QFrame>
#include <QWidget>
#include <QStackedWidget>
#include <QShortcut>
#include <QSplitter>

namespace euxis::etx {

QWidget* create_approvals_screen(QWidget* parent) {
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
    auto* title = new QLabel("HITL Approvals", widget);
    QFont title_font;
    title_font.setPointSize(20);
    title_font.setBold(true);
    title->setFont(title_font);
    layout->addWidget(title);

    auto* subtitle = new QLabel("Pending agent actions requiring human approval", widget);
    subtitle->setStyleSheet("color: #888; font-size: 12px;");
    layout->addWidget(subtitle);

    // Splitter: table on top, detail panel below
    auto* splitter = new QSplitter(Qt::Vertical, widget);

    // Approval table
    auto* table = new QTableWidget(splitter);
    table->setObjectName("approvals_table");
    table->setColumnCount(4);
    table->setHorizontalHeaderLabels({"Run ID", "Agent", "Action", "Task Preview"});
    table->horizontalHeader()->setStretchLastSection(true);
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    table->verticalHeader()->setVisible(false);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setAlternatingRowColors(true);
    table->setShowGrid(false);

    // Sample data
    struct Approval {
        QString run_id;
        QString agent;
        QString action;
        QString preview;
        QString details;
    };

    QList<Approval> approvals = {
        {"run-0042", "code-agent", "file-write",
         "Write generated module to src/auth.py",
         "Run ID: run-0042\nAgent: code-agent\nAction: file-write\n"
         "Target: src/auth.py\n\nThe agent wants to write a new authentication "
         "module containing JWT token validation, session management, and "
         "RBAC middleware. File size: 284 lines.\n\nRisk: MEDIUM - new file creation "
         "in source tree."},
        {"run-0043", "security-agent", "network-request",
         "POST to https://api.vulndb.io/v2/scan",
         "Run ID: run-0043\nAgent: security-agent\nAction: network-request\n"
         "Method: POST\nURL: https://api.vulndb.io/v2/scan\n\nThe agent wants to "
         "submit a dependency manifest for vulnerability scanning. Payload includes "
         "package names and versions only (no secrets).\n\nRisk: LOW - read-only "
         "external API call."},
        {"run-0044", "bridge-agent", "skill-import",
         "Import 'data-parser' from ClawHub registry",
         "Run ID: run-0044\nAgent: bridge-agent\nAction: skill-import\n"
         "Source: ClawHub registry\nSkill: data-parser v2.1.0\n\nThe agent wants to "
         "import a third-party skill from ClawHub. Bridge verification passed "
         "(signature valid, hash matches). WASM sandbox test: PASSED.\n\nRisk: MEDIUM "
         "- external skill import, ClawHavoc threat vector."},
        {"run-0045", "ops-agent", "system-exec",
         "Execute 'systemctl restart euxis-gateway'",
         "Run ID: run-0045\nAgent: ops-agent\nAction: system-exec\n"
         "Command: systemctl restart euxis-gateway\n\nThe agent wants to restart the "
         "gateway service after configuration changes. Current uptime: 14d 6h.\n"
         "Active connections: 3\n\nRisk: HIGH - service restart will cause brief "
         "downtime."},
    };

    table->setRowCount(approvals.size());
    for (int i = 0; i < approvals.size(); ++i) {
        table->setItem(i, 0, new QTableWidgetItem(approvals[i].run_id));
        table->setItem(i, 1, new QTableWidgetItem(approvals[i].agent));
        table->setItem(i, 2, new QTableWidgetItem(approvals[i].action));
        table->setItem(i, 3, new QTableWidgetItem(approvals[i].preview));
        table->setRowHeight(i, 40);
    }

    table->setStyleSheet(
        "QTableWidget { background: rgba(255,255,255,0.03); "
        "border: 1px solid rgba(255,255,255,0.1); border-radius: 8px; }"
        "QTableWidget::item { padding: 8px; }"
        "QTableWidget::item:alternate { background: rgba(255,255,255,0.02); }"
        "QTableWidget::item:selected { background: rgba(15,52,96,0.6); }"
        "QHeaderView::section { background: rgba(255,255,255,0.05); "
        "padding: 8px; border: none; font-weight: bold; }");

    splitter->addWidget(table);

    // Detail panel
    auto* detail_panel = new QFrame(splitter);
    detail_panel->setObjectName("approval_detail");
    detail_panel->setFrameShape(QFrame::StyledPanel);
    auto* detail_layout = new QVBoxLayout(detail_panel);
    detail_layout->setContentsMargins(16, 16, 16, 16);

    auto* detail_label = new QLabel("Details", detail_panel);
    QFont detail_font;
    detail_font.setPointSize(14);
    detail_font.setBold(true);
    detail_label->setFont(detail_font);
    detail_layout->addWidget(detail_label);

    auto* detail_text = new QTextEdit(detail_panel);
    detail_text->setObjectName("approval_detail_text");
    detail_text->setReadOnly(true);
    QFont mono_font;
    mono_font.setFamily("monospace");
    mono_font.setPointSize(11);
    detail_text->setFont(mono_font);
    detail_text->setStyleSheet(
        "QTextEdit { background: transparent; border: none; color: #bbb; }");
    detail_text->setPlainText("Select a pending action to view details.");
    detail_layout->addWidget(detail_text, 1);

    splitter->addWidget(detail_panel);
    splitter->setStretchFactor(0, 2);
    splitter->setStretchFactor(1, 1);

    layout->addWidget(splitter, 1);

    // Connect row selection to detail panel
    QObject::connect(table, &QTableWidget::currentCellChanged, widget,
                     [approvals, detail_text](int row, int /*col*/, int /*prev_row*/, int /*prev_col*/) {
        if (row >= 0 && row < approvals.size()) {
            detail_text->setPlainText(approvals[row].details);
        }
    });

    // Action buttons
    auto* button_row = new QHBoxLayout();
    button_row->addStretch();

    auto* refresh_btn = new QPushButton("Refresh (R)", widget);
    refresh_btn->setObjectName("refresh_button");
    refresh_btn->setCursor(Qt::PointingHandCursor);
    refresh_btn->setMinimumHeight(40);
    refresh_btn->setMinimumWidth(120);
    button_row->addWidget(refresh_btn);

    auto* reject_btn = new QPushButton("Reject (N)", widget);
    reject_btn->setObjectName("reject_button");
    reject_btn->setCursor(Qt::PointingHandCursor);
    reject_btn->setMinimumHeight(40);
    reject_btn->setMinimumWidth(120);
    reject_btn->setStyleSheet(
        "QPushButton { background: #6b2020; color: #fff; border: none; "
        "border-radius: 6px; font-weight: bold; font-size: 13px; padding: 8px 24px; }"
        "QPushButton:hover { background: #8b3030; }");
    button_row->addWidget(reject_btn);

    auto* approve_btn = new QPushButton("Approve (Y)", widget);
    approve_btn->setObjectName("approve_button");
    approve_btn->setCursor(Qt::PointingHandCursor);
    approve_btn->setMinimumHeight(40);
    approve_btn->setMinimumWidth(120);
    approve_btn->setStyleSheet(
        "QPushButton { background: #0f3460; color: #fff; border: none; "
        "border-radius: 6px; font-weight: bold; font-size: 13px; padding: 8px 24px; }"
        "QPushButton:hover { background: #1a4a7a; }");
    button_row->addWidget(approve_btn);

    layout->addLayout(button_row);

    // Approve action
    auto approve_action = [table, detail_text]() {
        int row = table->currentRow();
        if (row < 0) return;
        QString run_id = table->item(row, 0)->text();
        table->removeRow(row);
        detail_text->setPlainText("Approved: " + run_id);
    };

    // Reject action
    auto reject_action = [table, detail_text]() {
        int row = table->currentRow();
        if (row < 0) return;
        QString run_id = table->item(row, 0)->text();
        table->removeRow(row);
        detail_text->setPlainText("Rejected: " + run_id);
    };

    // Refresh action
    auto refresh_action = [detail_text]() {
        detail_text->setPlainText("Refreshing pending approvals...");
    };

    QObject::connect(approve_btn, &QPushButton::clicked, widget, approve_action);
    QObject::connect(reject_btn, &QPushButton::clicked, widget, reject_action);
    QObject::connect(refresh_btn, &QPushButton::clicked, widget, refresh_action);

    // Keyboard shortcuts
    auto* shortcut_y = new QShortcut(QKeySequence(Qt::Key_Y), widget);
    QObject::connect(shortcut_y, &QShortcut::activated, widget, approve_action);

    auto* shortcut_n = new QShortcut(QKeySequence(Qt::Key_N), widget);
    QObject::connect(shortcut_n, &QShortcut::activated, widget, reject_action);

    auto* shortcut_r = new QShortcut(QKeySequence(Qt::Key_R), widget);
    QObject::connect(shortcut_r, &QShortcut::activated, widget, refresh_action);

    auto* shortcut_esc = new QShortcut(QKeySequence(Qt::Key_Escape), widget);
    QObject::connect(shortcut_esc, &QShortcut::activated, widget, [widget]() {
        if (auto* stack = qobject_cast<QStackedWidget*>(widget->parentWidget())) {
            stack->setCurrentIndex(1);
        }
    });

    return widget;
}

} // namespace euxis::etx
