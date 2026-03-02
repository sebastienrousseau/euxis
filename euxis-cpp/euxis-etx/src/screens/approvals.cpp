#include <euxis/etx/config.hpp>

#include <nlohmann/json.hpp>
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
#include <QFile>
#include <QFileInfo>
#include <QCoreApplication>

namespace euxis::etx {

using json = nlohmann::json;

QWidget* create_approvals_screen(ETXConfig* /*config*/, QWidget* parent) {
    auto* widget = new QWidget(parent);
    auto* layout = new QVBoxLayout(widget);
    layout->setContentsMargins(32, 32, 32, 32);
    layout->setSpacing(16);

    // Back button
    auto* top_bar = new QHBoxLayout();
    auto* back_btn = new QPushButton(QCoreApplication::translate("ApprovalsScreen", "< Back"), widget);
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
    auto* title = new QLabel(QCoreApplication::translate("ApprovalsScreen", "HITL Approvals"), widget);
    QFont title_font;
    title_font.setPointSize(20);
    title_font.setBold(true);
    title->setFont(title_font);
    layout->addWidget(title);

    auto* subtitle = new QLabel(QCoreApplication::translate("ApprovalsScreen", "Pending agent actions requiring human approval"), widget);
    subtitle->setStyleSheet("color: #888; font-size: 12px;");
    layout->addWidget(subtitle);

    // Check for queue file
    QString queue_path = ETXConfig::runtime_dir() + "/approvals/queue.json";
    QFile queue_file(queue_path);
    bool has_queue = false;
    json queue_data;

    if (queue_file.open(QIODevice::ReadOnly)) {
        try {
            queue_data = json::parse(queue_file.readAll().toStdString());
            if (queue_data.is_array() && !queue_data.empty()) {
                has_queue = true;
            }
        } catch (const json::exception&) {
            // ignore
        }
    }

    if (!has_queue) {
        // Empty state
        auto* empty_frame = new QFrame(widget);
        empty_frame->setFrameShape(QFrame::StyledPanel);
        auto* empty_layout = new QVBoxLayout(empty_frame);
        empty_layout->setContentsMargins(40, 60, 40, 60);
        empty_layout->setAlignment(Qt::AlignCenter);

        auto* empty_icon = new QLabel("--", empty_frame);
        QFont icon_font;
        icon_font.setPointSize(48);
        empty_icon->setFont(icon_font);
        empty_icon->setStyleSheet("color: #444;");
        empty_icon->setAlignment(Qt::AlignCenter);
        empty_layout->addWidget(empty_icon);

        auto* empty_title = new QLabel(QCoreApplication::translate("ApprovalsScreen", "No Pending Approvals"), empty_frame);
        QFont empty_title_font;
        empty_title_font.setPointSize(18);
        empty_title_font.setBold(true);
        empty_title->setFont(empty_title_font);
        empty_title->setAlignment(Qt::AlignCenter);
        empty_layout->addWidget(empty_title);

        auto* empty_desc = new QLabel(
            QCoreApplication::translate("ApprovalsScreen",
            "All agent actions are approved or no actions require HITL review.\n"
            "Approval requests appear here when agents need permission for\n"
            "file writes, network requests, skill imports, or system commands."),
            empty_frame);
        empty_desc->setAlignment(Qt::AlignCenter);
        empty_desc->setStyleSheet("color: #888; font-size: 13px;");
        empty_layout->addWidget(empty_desc);

        auto* path_label = new QLabel(QCoreApplication::translate("ApprovalsScreen", "Queue file: %1").arg(queue_path), empty_frame);
        path_label->setAlignment(Qt::AlignCenter);
        path_label->setStyleSheet("color: #555; font-size: 11px;");
        empty_layout->addWidget(path_label);

        layout->addWidget(empty_frame, 1);
    } else {
        // Splitter: table on top, detail panel below
        auto* splitter = new QSplitter(Qt::Vertical, widget);

        auto* table = new QTableWidget(splitter);
        table->setObjectName("approvals_table");
        table->setColumnCount(4);
        table->setHorizontalHeaderLabels({
            QCoreApplication::translate("ApprovalsScreen", "Run ID"),
            QCoreApplication::translate("ApprovalsScreen", "Agent"),
            QCoreApplication::translate("ApprovalsScreen", "Action"),
            QCoreApplication::translate("ApprovalsScreen", "Task Preview")});
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

        table->setRowCount(static_cast<int>(queue_data.size()));
        for (int i = 0; i < static_cast<int>(queue_data.size()); ++i) {
            const auto& item = queue_data[static_cast<size_t>(i)];
            table->setItem(i, 0, new QTableWidgetItem(
                QString::fromStdString(item.value("run_id", ""))));
            table->setItem(i, 1, new QTableWidgetItem(
                QString::fromStdString(item.value("agent", ""))));
            table->setItem(i, 2, new QTableWidgetItem(
                QString::fromStdString(item.value("action", ""))));
            table->setItem(i, 3, new QTableWidgetItem(
                QString::fromStdString(item.value("preview", ""))));
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

        auto* detail_label = new QLabel(QCoreApplication::translate("ApprovalsScreen", "Details"), detail_panel);
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
        detail_text->setPlainText(QCoreApplication::translate("ApprovalsScreen", "Select a pending action to view details."));
        detail_layout->addWidget(detail_text, 1);

        splitter->addWidget(detail_panel);
        splitter->setStretchFactor(0, 2);
        splitter->setStretchFactor(1, 1);

        layout->addWidget(splitter, 1);

        // Connect row selection
        QObject::connect(table, &QTableWidget::currentCellChanged, widget,
                         [queue_data, detail_text](int row, int, int, int) {
            if (row >= 0 && row < static_cast<int>(queue_data.size())) {
                const auto& item = queue_data[static_cast<size_t>(row)];
                QString details = QString::fromStdString(
                    item.value("details", item.dump(2)));
                detail_text->setPlainText(details);
            }
        });

        // Action buttons
        auto* button_row = new QHBoxLayout();
        button_row->addStretch();

        auto* reject_btn = new QPushButton(QCoreApplication::translate("ApprovalsScreen", "Reject (N)"), widget);
        reject_btn->setCursor(Qt::PointingHandCursor);
        reject_btn->setMinimumHeight(40);
        reject_btn->setMinimumWidth(120);
        reject_btn->setStyleSheet(
            "QPushButton { background: #6b2020; color: #fff; border: none; "
            "border-radius: 6px; font-weight: bold; font-size: 13px; padding: 8px 24px; }"
            "QPushButton:hover { background: #8b3030; }");
        button_row->addWidget(reject_btn);

        auto* approve_btn = new QPushButton(QCoreApplication::translate("ApprovalsScreen", "Approve (Y)"), widget);
        approve_btn->setCursor(Qt::PointingHandCursor);
        approve_btn->setMinimumHeight(40);
        approve_btn->setMinimumWidth(120);
        approve_btn->setStyleSheet(
            "QPushButton { background: #0f3460; color: #fff; border: none; "
            "border-radius: 6px; font-weight: bold; font-size: 13px; padding: 8px 24px; }"
            "QPushButton:hover { background: #1a4a7a; }");
        button_row->addWidget(approve_btn);

        layout->addLayout(button_row);

        auto approve_action = [table, detail_text]() {
            int row = table->currentRow();
            if (row < 0) return;
            QString run_id = table->item(row, 0)->text();
            table->removeRow(row);
            detail_text->setPlainText(QCoreApplication::translate("ApprovalsScreen", "Approved: %1").arg(run_id));
        };

        auto reject_action = [table, detail_text]() {
            int row = table->currentRow();
            if (row < 0) return;
            QString run_id = table->item(row, 0)->text();
            table->removeRow(row);
            detail_text->setPlainText(QCoreApplication::translate("ApprovalsScreen", "Rejected: %1").arg(run_id));
        };

        QObject::connect(approve_btn, &QPushButton::clicked, widget, approve_action);
        QObject::connect(reject_btn, &QPushButton::clicked, widget, reject_action);

        auto* shortcut_y = new QShortcut(QKeySequence(Qt::Key_Y), widget);
        QObject::connect(shortcut_y, &QShortcut::activated, widget, approve_action);

        auto* shortcut_n = new QShortcut(QKeySequence(Qt::Key_N), widget);
        QObject::connect(shortcut_n, &QShortcut::activated, widget, reject_action);
    }

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
