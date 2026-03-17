#include <euxis/etx/registry.hpp>

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QTextEdit>
#include <QProgressBar>
#include <QFont>
#include <QFrame>
#include <QWidget>
#include <QStackedWidget>
#include <QShortcut>
#include <QSplitter>
#include <QHeaderView>
#include <QCoreApplication>

namespace euxis::etx {

QWidget* create_omnigraph_screen(FleetRegistry* registry, QWidget* parent) {
    auto* widget = new QWidget(parent);
    auto* layout = new QVBoxLayout(widget);
    layout->setContentsMargins(32, 32, 32, 32);
    layout->setSpacing(16);

    // Back button + refresh
    auto* top_bar = new QHBoxLayout();
    auto* back_btn = new QPushButton(QCoreApplication::translate("OmnigraphScreen", "< Back"), widget);
    back_btn->setCursor(Qt::PointingHandCursor);
    back_btn->setFixedWidth(100);
    top_bar->addWidget(back_btn);
    top_bar->addStretch();

    auto* refresh_btn = new QPushButton(QCoreApplication::translate("OmnigraphScreen", "Refresh"), widget);
    refresh_btn->setObjectName("omnigraph_refresh");
    refresh_btn->setCursor(Qt::PointingHandCursor);
    refresh_btn->setMinimumHeight(36);
    refresh_btn->setMinimumWidth(100);
    top_bar->addWidget(refresh_btn);

    layout->addLayout(top_bar);

    QObject::connect(back_btn, &QPushButton::clicked, widget, [widget]() {
        if (auto* stack = qobject_cast<QStackedWidget*>(widget->parentWidget())) {
            stack->setCurrentIndex(1);
        }
    });

    // Title
    auto* title = new QLabel(QCoreApplication::translate("OmnigraphScreen", "Omnigraph"), widget);
    QFont title_font;
    title_font.setPointSize(20);
    title_font.setBold(true);
    title->setFont(title_font);
    layout->addWidget(title);

    auto* subtitle = new QLabel(QCoreApplication::translate("OmnigraphScreen", "Agent and squad dependency graph"), widget);
    subtitle->setStyleSheet("color: #888; font-size: 12px;");
    layout->addWidget(subtitle);

    // Summary bar
    auto* summary_card = new QFrame(widget);
    summary_card->setFrameShape(QFrame::StyledPanel);
    auto* summary_layout = new QHBoxLayout(summary_card);
    summary_layout->setContentsMargins(20, 12, 20, 12);
    summary_layout->setSpacing(24);

    auto* agents_count = new QLabel(widget);
    agents_count->setObjectName("omnigraph_agents_count");
    agents_count->setStyleSheet("color: #2196f3; font-weight: bold; font-size: 14px;");
    summary_layout->addWidget(agents_count);

    auto* squads_count = new QLabel(widget);
    squads_count->setObjectName("omnigraph_squads_count");
    squads_count->setStyleSheet("color: #4caf50; font-weight: bold; font-size: 14px;");
    summary_layout->addWidget(squads_count);

    auto* combos_count = new QLabel(widget);
    combos_count->setObjectName("omnigraph_combos_count");
    combos_count->setStyleSheet("color: #ff9800; font-weight: bold; font-size: 14px;");
    summary_layout->addWidget(combos_count);

    summary_layout->addStretch();
    layout->addWidget(summary_card);

    // Splitter: tree on left, details on right
    auto* splitter = new QSplitter(Qt::Horizontal, widget);

    // Tree widget
    auto* tree = new QTreeWidget(splitter);
    tree->setObjectName("omnigraph_tree");
    tree->setHeaderLabels({
        QCoreApplication::translate("OmnigraphScreen", "Node"),
        QCoreApplication::translate("OmnigraphScreen", "Type"),
        QCoreApplication::translate("OmnigraphScreen", "Info")});
    tree->setMinimumWidth(350);
    tree->setAlternatingRowColors(true);
    tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    tree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    tree->setStyleSheet(
        "QTreeWidget { background: rgba(255,255,255,0.03); "
        "border: 1px solid rgba(255,255,255,0.1); border-radius: 8px; "
        "padding: 8px; }"
        "QTreeWidget::item { padding: 4px 8px; }"
        "QTreeWidget::item:alternate { background: rgba(255,255,255,0.02); }"
        "QTreeWidget::item:selected { background: rgba(15,52,96,0.6); }"
        "QHeaderView::section { background: rgba(255,255,255,0.05); "
        "padding: 6px; border: none; font-weight: bold; }");

    splitter->addWidget(tree);

    // Detail panel
    auto* detail_panel = new QFrame(splitter);
    detail_panel->setObjectName("omnigraph_detail");
    detail_panel->setFrameShape(QFrame::StyledPanel);
    auto* detail_layout = new QVBoxLayout(detail_panel);
    detail_layout->setContentsMargins(20, 20, 20, 20);

    auto* detail_title = new QLabel(QCoreApplication::translate("OmnigraphScreen", "Node Details"), detail_panel);
    QFont detail_font;
    detail_font.setPointSize(14);
    detail_font.setBold(true);
    detail_title->setFont(detail_font);
    detail_layout->addWidget(detail_title);

    auto* detail_text = new QTextEdit(detail_panel);
    detail_text->setObjectName("omnigraph_detail_text");
    detail_text->setReadOnly(true);
    QFont mono_font;
    mono_font.setFamily("monospace");
    mono_font.setPointSize(11);
    detail_text->setFont(mono_font);
    detail_text->setStyleSheet(
        "QTextEdit { background: transparent; border: none; color: #bbb; }");
    detail_text->setPlainText(QCoreApplication::translate("OmnigraphScreen", "Select a node in the tree to view its details."));
    detail_layout->addWidget(detail_text, 1);

    splitter->addWidget(detail_panel);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 1);

    layout->addWidget(splitter, 1);

    // Build tree from registry data
    auto build_tree = [tree, registry, agents_count, squads_count, combos_count]() {
        tree->clear();

        auto* root = new QTreeWidgetItem(tree, {"euxis-fleet", "root",
            QCoreApplication::translate("OmnigraphScreen", "%1 agents").arg(registry->agent_count())});
        root->setExpanded(true);

        // Core agents
        auto core_agents = registry->filter_by_tier("core");
        auto* core_node = new QTreeWidgetItem(root, {"core/", "tier",
            QCoreApplication::translate("OmnigraphScreen", "%1 agents").arg(core_agents.size())});
        core_node->setExpanded(true);
        for (const auto& a : core_agents) {
            new QTreeWidgetItem(core_node, {a.id, "agent", a.activation});
        }

        // Fleet agents
        auto fleet_agents = registry->filter_by_tier("fleet");
        auto* fleet_node = new QTreeWidgetItem(root, {"fleet/", "tier",
            QCoreApplication::translate("OmnigraphScreen", "%1 agents").arg(fleet_agents.size())});
        fleet_node->setExpanded(true);
        for (const auto& a : fleet_agents) {
            new QTreeWidgetItem(fleet_node, {a.id, "agent", a.activation});
        }

        // Squads
        const auto& squads = registry->squads();
        if (!squads.empty()) {
            auto* squads_node = new QTreeWidgetItem(root, {"squads/", "group",
                QCoreApplication::translate("OmnigraphScreen", "%1 squads").arg(squads.size())});
            squads_node->setExpanded(true);
            for (const auto& s : squads) {
                auto* snode = new QTreeWidgetItem(squads_node, {s.name, "squad",
                    QCoreApplication::translate("OmnigraphScreen", "%1 members").arg(s.members.size())});
                for (const auto& m : s.members) {
                    new QTreeWidgetItem(snode, {m, "member", ""});
                }
            }
        }

        // Combos
        const auto& combos = registry->combos();
        if (!combos.empty()) {
            auto* combos_node = new QTreeWidgetItem(root, {"combos/", "group",
                QCoreApplication::translate("OmnigraphScreen", "%1 combos").arg(combos.size())});
            combos_node->setExpanded(true);
            for (const auto& c : combos) {
                auto* cnode = new QTreeWidgetItem(combos_node, {c.name, "combo",
                    QCoreApplication::translate("OmnigraphScreen", "%1 steps").arg(c.chain.size())});
                for (const auto& step : c.chain) {
                    new QTreeWidgetItem(cnode, {step, "step", ""});
                }
            }
        }

        // Update summary
        agents_count->setText(QCoreApplication::translate("OmnigraphScreen", "Agents: %1").arg(registry->agent_count()));
        squads_count->setText(QCoreApplication::translate("OmnigraphScreen", "Squads: %1").arg(registry->squad_count()));
        combos_count->setText(QCoreApplication::translate("OmnigraphScreen", "Combos: %1").arg(registry->combo_count()));
    };

    build_tree();

    // Connect tree selection to detail display
    QObject::connect(tree, &QTreeWidget::currentItemChanged, widget,
                     [detail_text, registry](QTreeWidgetItem* current, QTreeWidgetItem*) {
        if (!current) return;

        QString name = current->text(0);
        QString type = current->text(1);
        QString info = current->text(2);

        QString details = QCoreApplication::translate("OmnigraphScreen", "Name: %1\nType: %2\n").arg(name, type);
        if (!info.isEmpty()) {
            details += QCoreApplication::translate("OmnigraphScreen", "Info: %1").arg(info) + "\n";
        }

        if (type == "agent" || type == "member" || type == "step") {
            const auto* agent = registry->find(name);
            if (agent) {
                details += "\n" + QCoreApplication::translate("OmnigraphScreen", "Tier: %1").arg(agent->tier.toUpper());
                details += "\n" + QCoreApplication::translate("OmnigraphScreen", "Activation: %1").arg(agent->activation);
                details += "\n" + QCoreApplication::translate("OmnigraphScreen", "Version: %1").arg(agent->version);
                if (!agent->tags.isEmpty()) {
                    details += "\n" + QCoreApplication::translate("OmnigraphScreen", "Tags: %1").arg(agent->tags.join(", "));
                }
                if (!agent->capability_tags.isEmpty()) {
                    details += "\n" + QCoreApplication::translate("OmnigraphScreen", "Capabilities: %1").arg(agent->capability_tags.join(", "));
                }
                details += "\n" + QCoreApplication::translate("OmnigraphScreen", "Description: %1").arg(agent->description);
            }
        } else if (type == "squad") {
            const auto& squads = registry->squads();
            for (const auto& s : squads) {
                if (s.name == name) {
                    details += "\n" + QCoreApplication::translate("OmnigraphScreen", "Purpose: %1").arg(s.purpose);
                    details += "\n" + QCoreApplication::translate("OmnigraphScreen", "Lead: %1").arg(s.lead);
                    details += "\n" + QCoreApplication::translate("OmnigraphScreen", "Members: %1").arg(s.members.join(", "));
                    break;
                }
            }
        } else if (type == "combo") {
            const auto& combos = registry->combos();
            for (const auto& c : combos) {
                if (c.name == name) {
                    details += "\n" + QCoreApplication::translate("OmnigraphScreen", "Description: %1").arg(c.description);
                    details += "\n" + QCoreApplication::translate("OmnigraphScreen", "Chain: %1").arg(c.chain.join(" -> "));
                    break;
                }
            }
        }

        detail_text->setPlainText(details);
    });

    // Refresh
    QObject::connect(refresh_btn, &QPushButton::clicked, widget, build_tree);
    QObject::connect(registry, &FleetRegistry::refreshed, widget, build_tree);

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
