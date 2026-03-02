#include <euxis/etx/registry.hpp>
#include <euxis/etx/semantic_colors.hpp>

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QListWidget>
#include <QFont>
#include <QFrame>
#include <QWidget>
#include <QStackedWidget>
#include <QCoreApplication>

namespace euxis::etx {

QWidget* create_fleet_monitor_screen(FleetRegistry* registry, QWidget* parent) {
    auto* widget = new QWidget(parent);
    auto* layout = new QVBoxLayout(widget);
    layout->setContentsMargins(32, 32, 32, 32);
    layout->setSpacing(16);

    // Back button
    auto* top_bar = new QHBoxLayout();
    auto* back_btn = new QPushButton(QCoreApplication::translate("FleetMonitorScreen", "< Back"), widget);
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
    auto* title = new QLabel(QCoreApplication::translate("FleetMonitorScreen", "Fleet Monitor"), widget);
    QFont title_font;
    title_font.setPointSize(20);
    title_font.setBold(true);
    title->setFont(title_font);
    layout->addWidget(title);

    // Operation type label
    auto* op_label = new QLabel(QCoreApplication::translate("FleetMonitorScreen", "Operation: Fleet Overview"), widget);
    op_label->setObjectName("operation_type_label");
    QFont op_font;
    op_font.setPointSize(14);
    op_label->setFont(op_font);
    op_label->setStyleSheet("color: #aaa;");
    layout->addWidget(op_label);

    // Separator
    auto* separator = new QFrame(widget);
    separator->setFrameShape(QFrame::HLine);
    separator->setStyleSheet("color: rgba(255,255,255,0.1);");
    layout->addWidget(separator);

    // Members section
    auto* members_label = new QLabel(QCoreApplication::translate("FleetMonitorScreen", "Agents"), widget);
    QFont members_font;
    members_font.setPointSize(16);
    members_font.setBold(true);
    members_label->setFont(members_font);
    layout->addWidget(members_label);

    // Member list
    auto* member_list = new QListWidget(widget);
    member_list->setObjectName("member_list");
    member_list->setAlternatingRowColors(true);
    member_list->setMinimumHeight(300);
    member_list->setStyleSheet(
        "QListWidget { background: rgba(255,255,255,0.03); border: 1px solid rgba(255,255,255,0.1); "
        "border-radius: 8px; padding: 8px; }"
        "QListWidget::item { padding: 8px 12px; border-radius: 4px; }"
        "QListWidget::item:alternate { background: rgba(255,255,255,0.02); }"
        "QListWidget::item:selected { background: rgba(15,52,96,0.6); }");

    // Populate with all agents from registry
    auto populate = [member_list, registry]() {
        member_list->clear();
        const auto& agents = registry->agents();
        for (const auto& a : agents) {
            QString display = a.name + "  [" + a.tier.toUpper() + "]  " + a.activation;
            member_list->addItem(display);
        }
    };
    populate();

    QObject::connect(registry, &FleetRegistry::refreshed, widget, populate);

    layout->addWidget(member_list, 1);

    // Status section
    auto* status_bar = new QHBoxLayout();

    auto add_status_indicator = [&](const QString& label, const QString& color) {
        auto* dot = new QLabel(widget);
        dot->setFixedSize(10, 10);
        dot->setStyleSheet(
            QString("background: %1; border-radius: 5px; border: none;").arg(color));
        status_bar->addWidget(dot);
        auto* lbl = new QLabel(label, widget);
        lbl->setStyleSheet("color: #888; font-size: 11px;");
        status_bar->addWidget(lbl);
        status_bar->addSpacing(16);
    };

    add_status_indicator(QCoreApplication::translate("FleetMonitorScreen", "Idle"),
                         severity_color(Severity::Info).name());
    add_status_indicator(QCoreApplication::translate("FleetMonitorScreen", "Running"),
                         severity_color(Severity::Success).name());
    add_status_indicator(QCoreApplication::translate("FleetMonitorScreen", "Error"),
                         severity_color(Severity::Error).name());
    status_bar->addStretch();

    layout->addLayout(status_bar);

    return widget;
}

} // namespace euxis::etx
