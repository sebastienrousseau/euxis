#include <euxis/etx/registry.hpp>
#include <euxis/etx/config.hpp>
#include <euxis/etx/semantic_colors.hpp>

#include <nlohmann/json.hpp>
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
#include <QFile>
#include <QCoreApplication>

namespace euxis::etx {

using json = nlohmann::json;

QWidget* create_metrics_screen(FleetRegistry* registry, ETXConfig* /*config*/,
                               QWidget* parent) {
    auto* widget = new QWidget(parent);
    auto* layout = new QVBoxLayout(widget);
    layout->setContentsMargins(32, 32, 32, 32);
    layout->setSpacing(16);

    // Back button
    auto* top_bar = new QHBoxLayout();
    auto* back_btn = new QPushButton(QCoreApplication::translate("MetricsScreen", "< Back"), widget);
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
    auto* title = new QLabel(QCoreApplication::translate("MetricsScreen", "Fleet Metrics"), widget);
    QFont title_font;
    title_font.setPointSize(20);
    title_font.setBold(true);
    title->setFont(title_font);
    layout->addWidget(title);

    // Summary section — real counts from registry
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

    add_stat(QCoreApplication::translate("MetricsScreen", "Agents"),
             QString::number(registry->agent_count()),
             severity_color(Severity::Info).name());
    add_stat(QCoreApplication::translate("MetricsScreen", "Squads"),
             QString::number(registry->squad_count()),
             severity_color(Severity::Success).name());
    add_stat(QCoreApplication::translate("MetricsScreen", "Combos"),
             QString::number(registry->combo_count()),
             severity_color(Severity::Warning).name());
    add_stat(QCoreApplication::translate("MetricsScreen", "Playbooks"),
             QString::number(static_cast<int>(registry->playbooks().size())),
             "#9c27b0");  // purple — no severity equivalent

    summary_layout->addStretch();
    layout->addWidget(summary_card);

    // Provider tier table from router.json
    auto* provider_label = new QLabel(QCoreApplication::translate("MetricsScreen", "Router Configuration"), widget);
    QFont provider_font;
    provider_font.setPointSize(16);
    provider_font.setBold(true);
    provider_label->setFont(provider_font);
    layout->addWidget(provider_label);

    // Load router.json
    auto* router_table = new QTableWidget(widget);
    router_table->setObjectName("router_table");
    router_table->setColumnCount(3);
    router_table->setHorizontalHeaderLabels({
        QCoreApplication::translate("MetricsScreen", "Tier"),
        QCoreApplication::translate("MetricsScreen", "Model"),
        QCoreApplication::translate("MetricsScreen", "Cost ($/M tokens)")});
    router_table->horizontalHeader()->setStretchLastSection(true);
    router_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    router_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    router_table->verticalHeader()->setVisible(false);
    router_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    router_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    router_table->setAlternatingRowColors(true);
    router_table->setShowGrid(false);

    QString router_path = ETXConfig::data_dir() + "/config/router.json";
    QFile router_file(router_path);
    if (router_file.open(QIODevice::ReadOnly)) {
        try {
            auto doc = json::parse(router_file.readAll().toStdString());
            auto models = doc.value("models", json::object());
            auto costs = doc.value("cost_estimates", json::object());

            QStringList tiers = {"routine", "data", "code", "reason"};
            router_table->setRowCount(tiers.size());

            for (int i = 0; i < tiers.size(); ++i) {
                std::string tier = tiers[i].toStdString();
                router_table->setItem(i, 0, new QTableWidgetItem(tiers[i]));
                router_table->setItem(i, 1, new QTableWidgetItem(
                    QString::fromStdString(models.value(tier, ""))));
                double cost = costs.value(tier, 0.0);
                router_table->setItem(i, 2, new QTableWidgetItem(
                    QString("$%1").arg(cost, 0, 'f', 2)));
                router_table->setRowHeight(i, 36);
            }
        } catch (const json::exception&) {
            // Ignore parse errors
        }
    }

    router_table->setMaximumHeight(200);
    router_table->setStyleSheet(
        "QTableWidget { background: rgba(255,255,255,0.03); "
        "border: 1px solid rgba(255,255,255,0.1); border-radius: 8px; }"
        "QTableWidget::item { padding: 8px; }"
        "QTableWidget::item:alternate { background: rgba(255,255,255,0.02); }"
        "QTableWidget::item:selected { background: rgba(15,52,96,0.6); }"
        "QHeaderView::section { background: rgba(255,255,255,0.05); "
        "padding: 8px; border: none; font-weight: bold; }");
    layout->addWidget(router_table);

    // Agent table — all agents with tier/activation/tags
    auto* agent_label = new QLabel(QCoreApplication::translate("MetricsScreen", "All Agents"), widget);
    QFont agent_label_font;
    agent_label_font.setPointSize(16);
    agent_label_font.setBold(true);
    agent_label->setFont(agent_label_font);
    layout->addWidget(agent_label);

    auto* table = new QTableWidget(widget);
    table->setObjectName("metrics_table");
    table->setColumnCount(4);
    table->setHorizontalHeaderLabels({
        QCoreApplication::translate("MetricsScreen", "Agent"),
        QCoreApplication::translate("MetricsScreen", "Tier"),
        QCoreApplication::translate("MetricsScreen", "Activation"),
        QCoreApplication::translate("MetricsScreen", "Tags")});
    table->horizontalHeader()->setStretchLastSection(true);
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    table->verticalHeader()->setVisible(false);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setAlternatingRowColors(true);
    table->setShowGrid(false);

    const auto& agents = registry->agents();
    table->setRowCount(static_cast<int>(agents.size()));
    for (int i = 0; i < static_cast<int>(agents.size()); ++i) {
        const auto& a = agents[static_cast<size_t>(i)];
        table->setItem(i, 0, new QTableWidgetItem(a.name));

        auto* tier_item = new QTableWidgetItem(a.tier.toUpper());
        tier_item->setForeground(a.tier == "core" ? QColor("#6db3f2") : QColor("#888"));
        table->setItem(i, 1, tier_item);

        table->setItem(i, 2, new QTableWidgetItem(a.activation));
        table->setItem(i, 3, new QTableWidgetItem(a.tags.join(", ")));
        table->setRowHeight(i, 32);
    }

    table->setStyleSheet(
        "QTableWidget { background: rgba(255,255,255,0.03); "
        "border: 1px solid rgba(255,255,255,0.1); border-radius: 8px; }"
        "QTableWidget::item { padding: 6px; }"
        "QTableWidget::item:alternate { background: rgba(255,255,255,0.02); }"
        "QTableWidget::item:selected { background: rgba(15,52,96,0.6); }"
        "QHeaderView::section { background: rgba(255,255,255,0.05); "
        "padding: 8px; border: none; font-weight: bold; }");

    layout->addWidget(table, 1);

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
