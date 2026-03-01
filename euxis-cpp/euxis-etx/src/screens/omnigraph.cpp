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

namespace euxis::etx {

QWidget* create_omnigraph_screen(QWidget* parent) {
    auto* widget = new QWidget(parent);
    auto* layout = new QVBoxLayout(widget);
    layout->setContentsMargins(32, 32, 32, 32);
    layout->setSpacing(16);

    // Back button + refresh
    auto* top_bar = new QHBoxLayout();
    auto* back_btn = new QPushButton("< Back", widget);
    back_btn->setCursor(Qt::PointingHandCursor);
    back_btn->setFixedWidth(100);
    top_bar->addWidget(back_btn);
    top_bar->addStretch();

    auto* refresh_btn = new QPushButton("Refresh", widget);
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
    auto* title = new QLabel("Omnigraph", widget);
    QFont title_font;
    title_font.setPointSize(20);
    title_font.setBold(true);
    title->setFont(title_font);
    layout->addWidget(title);

    auto* subtitle = new QLabel("Dependency graph and token budget viewer", widget);
    subtitle->setStyleSheet("color: #888; font-size: 12px;");
    layout->addWidget(subtitle);

    // Token budget display
    auto* budget_card = new QFrame(widget);
    budget_card->setObjectName("token_budget_card");
    budget_card->setFrameShape(QFrame::StyledPanel);
    auto* budget_layout = new QHBoxLayout(budget_card);
    budget_layout->setContentsMargins(20, 12, 20, 12);
    budget_layout->setSpacing(16);

    auto* budget_label = new QLabel("Token Budget:", budget_card);
    budget_label->setStyleSheet("color: #888; font-weight: bold; font-size: 13px;");
    budget_layout->addWidget(budget_label);

    auto* budget_bar = new QProgressBar(budget_card);
    budget_bar->setObjectName("token_budget_bar");
    budget_bar->setRange(0, 100);
    budget_bar->setValue(34);
    budget_bar->setFormat("34,200 / 100,000 tokens (34%)");
    budget_bar->setMinimumHeight(28);
    budget_bar->setStyleSheet(
        "QProgressBar { background: rgba(255,255,255,0.05); "
        "border: 1px solid rgba(255,255,255,0.1); border-radius: 4px; "
        "text-align: center; color: #ccc; font-size: 12px; }"
        "QProgressBar::chunk { background: #0f3460; border-radius: 3px; }");
    budget_layout->addWidget(budget_bar, 1);

    layout->addWidget(budget_card);

    // Splitter: tree on left, details on right
    auto* splitter = new QSplitter(Qt::Horizontal, widget);

    // Tree widget
    auto* tree = new QTreeWidget(splitter);
    tree->setObjectName("omnigraph_tree");
    tree->setHeaderLabels({"Node", "Type", "Tokens"});
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

    // Populate tree with sample data
    auto* root = new QTreeWidgetItem(tree, {"euxis-workspace", "root", "34,200"});
    root->setExpanded(true);

    // Agents
    auto* agents_node = new QTreeWidgetItem(root, {"agents/", "group", "18,400"});
    agents_node->setExpanded(true);
    new QTreeWidgetItem(agents_node, {"code-agent", "agent", "7,200"});
    new QTreeWidgetItem(agents_node, {"research-agent", "agent", "6,800"});
    new QTreeWidgetItem(agents_node, {"security-agent", "agent", "4,400"});

    // Squads
    auto* squads_node = new QTreeWidgetItem(root, {"squads/", "group", "9,600"});
    squads_node->setExpanded(true);
    new QTreeWidgetItem(squads_node, {"bridge-squad", "squad", "5,200"});
    new QTreeWidgetItem(squads_node, {"ops-squad", "squad", "4,400"});

    // Tools
    auto* tools_node = new QTreeWidgetItem(root, {"tools/", "group", "6,200"});
    tools_node->setExpanded(true);
    new QTreeWidgetItem(tools_node, {"wasm-sandbox", "tool", "3,800"});
    new QTreeWidgetItem(tools_node, {"crypto-backend", "tool", "2,400"});

    splitter->addWidget(tree);

    // Detail panel
    auto* detail_panel = new QFrame(splitter);
    detail_panel->setObjectName("omnigraph_detail");
    detail_panel->setFrameShape(QFrame::StyledPanel);
    auto* detail_layout = new QVBoxLayout(detail_panel);
    detail_layout->setContentsMargins(20, 20, 20, 20);

    auto* detail_title = new QLabel("Node Details", detail_panel);
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
    detail_text->setPlainText("Select a node in the tree to view its details.");
    detail_layout->addWidget(detail_text, 1);

    splitter->addWidget(detail_panel);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 1);

    layout->addWidget(splitter, 1);

    // Connect tree selection to detail display
    QObject::connect(tree, &QTreeWidget::currentItemChanged, widget,
                     [detail_text](QTreeWidgetItem* current, QTreeWidgetItem* /*previous*/) {
        if (!current) return;

        QString name = current->text(0);
        QString type = current->text(1);
        QString tokens = current->text(2);

        QString details = QString("Name: %1\nType: %2\nTokens: %3\n")
                          .arg(name, type, tokens);

        if (type == "agent") {
            details += "\nCapabilities:\n"
                       "  - Task execution\n"
                       "  - Memory persistence (AES-256-GCM)\n"
                       "  - WASM sandboxed runtime\n"
                       "\nDependencies:\n"
                       "  - euxis-core (mesh orchestration)\n"
                       "  - euxis-crypto (encryption)\n"
                       "  - euxis-memory (checkpoint/resume)";
        } else if (type == "squad") {
            details += "\nComposition:\n"
                       "  - Lead agent + 2-4 members\n"
                       "  - A2A protocol coordination\n"
                       "\nDependencies:\n"
                       "  - euxis-core (swarm orchestration)\n"
                       "  - euxis-a2a (protocol support)";
        } else if (type == "tool") {
            details += "\nRuntime:\n"
                       "  - Extism WASM plugin\n"
                       "  - Isolated execution context\n"
                       "\nDependencies:\n"
                       "  - euxis-core (plugin host)";
        } else if (type == "root") {
            details += "\nWorkspace Summary:\n"
                       "  Agents: 3\n"
                       "  Squads: 2\n"
                       "  Tools: 2\n"
                       "  Token utilization: 34.2%";
        }

        detail_text->setPlainText(details);
    });

    // Refresh action
    QObject::connect(refresh_btn, &QPushButton::clicked, widget, [detail_text]() {
        detail_text->setPlainText("Refreshing dependency graph...");
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
