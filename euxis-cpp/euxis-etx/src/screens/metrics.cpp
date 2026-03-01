#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QHeaderView>
#include <QProgressBar>
#include <QFont>
#include <QFrame>
#include <QWidget>
#include <QStackedWidget>
#include <QShortcut>

namespace euxis::etx {

QWidget* create_metrics_screen(QWidget* parent) {
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
    auto* title = new QLabel("Performance Metrics", widget);
    QFont title_font;
    title_font.setPointSize(20);
    title_font.setBold(true);
    title->setFont(title_font);
    layout->addWidget(title);

    // Summary section
    auto* summary_card = new QFrame(widget);
    summary_card->setObjectName("metrics_summary");
    summary_card->setFrameShape(QFrame::StyledPanel);
    auto* summary_layout = new QHBoxLayout(summary_card);
    summary_layout->setContentsMargins(20, 16, 20, 16);
    summary_layout->setSpacing(32);

    auto add_stat = [&](const QString& label, const QString& value, const QString& color) {
        auto* stat_box = new QVBoxLayout();
        stat_box->setSpacing(4);

        auto* val_label = new QLabel(value, summary_card);
        QFont val_font;
        val_font.setPointSize(28);
        val_font.setBold(true);
        val_label->setFont(val_font);
        val_label->setStyleSheet(QString("color: %1;").arg(color));
        val_label->setAlignment(Qt::AlignCenter);
        stat_box->addWidget(val_label);

        auto* name_label = new QLabel(label, summary_card);
        name_label->setStyleSheet("color: #888; font-size: 12px;");
        name_label->setAlignment(Qt::AlignCenter);
        stat_box->addWidget(name_label);

        summary_layout->addLayout(stat_box);
    };

    add_stat("Total Runs", "1,247", "#4caf50");
    add_stat("Agents Used", "8", "#2196f3");
    add_stat("Avg Time", "2.3s", "#ff9800");

    summary_layout->addStretch();
    layout->addWidget(summary_card);

    // Per-agent stats table
    auto* table_label = new QLabel("Per-Agent Statistics", widget);
    QFont table_label_font;
    table_label_font.setPointSize(16);
    table_label_font.setBold(true);
    table_label->setFont(table_label_font);
    layout->addWidget(table_label);

    auto* table = new QTableWidget(widget);
    table->setObjectName("metrics_table");
    table->setColumnCount(4);
    table->setHorizontalHeaderLabels({"Agent", "Runs", "Avg Duration", "Status"});
    table->horizontalHeader()->setStretchLastSection(true);
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    table->verticalHeader()->setVisible(false);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setAlternatingRowColors(true);
    table->setShowGrid(false);

    struct AgentStats {
        QString name;
        int runs;
        QString avg_duration;
        QString status;
        QString status_color;
    };

    QList<AgentStats> stats = {
        {"code-agent",      412, "1.8s", "running",  "#4caf50"},
        {"research-agent",  287, "3.1s", "running",  "#4caf50"},
        {"security-agent",  198, "2.7s", "idle",     "#2196f3"},
        {"bridge-agent",    156, "1.4s", "running",  "#4caf50"},
        {"identity-agent",  194, "2.9s", "error",    "#f44336"},
    };

    table->setRowCount(stats.size());
    for (int i = 0; i < stats.size(); ++i) {
        table->setItem(i, 0, new QTableWidgetItem(stats[i].name));
        table->setItem(i, 1, new QTableWidgetItem(QString::number(stats[i].runs)));
        table->setItem(i, 2, new QTableWidgetItem(stats[i].avg_duration));

        auto* status_item = new QTableWidgetItem(stats[i].status);
        status_item->setForeground(QColor(stats[i].status_color));
        table->setItem(i, 3, status_item);

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

    layout->addWidget(table, 1);

    // Provider distribution
    auto* provider_label = new QLabel("Provider Distribution", widget);
    QFont provider_font;
    provider_font.setPointSize(16);
    provider_font.setBold(true);
    provider_label->setFont(provider_font);
    layout->addWidget(provider_label);

    auto* provider_card = new QFrame(widget);
    provider_card->setObjectName("provider_card");
    provider_card->setFrameShape(QFrame::StyledPanel);
    auto* provider_layout = new QVBoxLayout(provider_card);
    provider_layout->setContentsMargins(20, 16, 20, 16);
    provider_layout->setSpacing(12);

    struct Provider {
        QString name;
        int percentage;
        QString color;
    };

    QList<Provider> providers = {
        {"Anthropic (Claude)",  62, "#0f3460"},
        {"Local Inference",     25, "#4caf50"},
        {"OpenAI (Fallback)",   13, "#ff9800"},
    };

    for (const auto& p : providers) {
        auto* row = new QHBoxLayout();
        row->setSpacing(12);

        auto* name = new QLabel(p.name, provider_card);
        name->setFixedWidth(180);
        name->setStyleSheet("color: #ccc; font-size: 13px;");
        row->addWidget(name);

        auto* bar = new QProgressBar(provider_card);
        bar->setRange(0, 100);
        bar->setValue(p.percentage);
        bar->setFormat(QString("%1%").arg(p.percentage));
        bar->setMinimumHeight(24);
        bar->setStyleSheet(
            QString("QProgressBar { background: rgba(255,255,255,0.05); "
                    "border: 1px solid rgba(255,255,255,0.1); border-radius: 4px; "
                    "text-align: center; color: #ccc; }"
                    "QProgressBar::chunk { background: %1; border-radius: 3px; }")
            .arg(p.color));
        row->addWidget(bar);

        provider_layout->addLayout(row);
    }

    layout->addWidget(provider_card);

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
