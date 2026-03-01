#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QListWidget>
#include <QTextEdit>
#include <QFont>
#include <QFrame>
#include <QWidget>
#include <QStackedWidget>
#include <QSplitter>

namespace euxis::etx {

QWidget* create_playbooks_screen(QWidget* parent) {
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
    auto* title = new QLabel("Playbooks", widget);
    QFont title_font;
    title_font.setPointSize(20);
    title_font.setBold(true);
    title->setFont(title_font);
    layout->addWidget(title);

    // Splitter: list on left, description on right
    auto* splitter = new QSplitter(Qt::Horizontal, widget);

    // Playbook list
    auto* list = new QListWidget(splitter);
    list->setObjectName("playbook_list");
    list->setMinimumWidth(250);
    list->setStyleSheet(
        "QListWidget { background: rgba(255,255,255,0.03); "
        "border: 1px solid rgba(255,255,255,0.1); border-radius: 8px; "
        "padding: 8px; }"
        "QListWidget::item { padding: 10px 12px; border-radius: 4px; }"
        "QListWidget::item:selected { background: rgba(15,52,96,0.6); }");

    struct Playbook {
        QString name;
        QString description;
    };

    QList<Playbook> playbooks = {
        {"security-audit",
         "Comprehensive security audit playbook.\n\n"
         "Steps:\n"
         "1. Run supply chain verification on all dependencies\n"
         "2. Execute vulnerability scanning with security-agent\n"
         "3. Verify WASM sandbox integrity via Extism runtime\n"
         "4. Check AES-256-GCM encryption key rotation\n"
         "5. Audit OpenClaw bridge imports for ClawHavoc threats\n"
         "6. Generate security report"},
        {"full-deployment",
         "Deploy all agents and squads to production.\n\n"
         "Steps:\n"
         "1. Pre-flight checks (health, memory, identity)\n"
         "2. Deploy inference-agent with FinOps routing\n"
         "3. Deploy code-agent and research-agent\n"
         "4. Initialize bridge-squad for OpenClaw interop\n"
         "5. Start ops-combo monitoring\n"
         "6. Verify A2A protocol connectivity"},
        {"bridge-import",
         "Import skills from ClawHub with verification.\n\n"
         "Steps:\n"
         "1. Enumerate available ClawHub skills\n"
         "2. Download SKILL.md and openclaw.json manifests\n"
         "3. Run bridge verification (signature + hash)\n"
         "4. Sandbox-test imported skills in WASM\n"
         "5. Register verified skills in local registry"},
        {"incident-response",
         "Respond to production incidents.\n\n"
         "Steps:\n"
         "1. Isolate affected agents\n"
         "2. Capture encrypted memory snapshots\n"
         "3. Engage security-agent for threat assessment\n"
         "4. Roll back to last known-good checkpoint\n"
         "5. Verify system integrity post-recovery\n"
         "6. Generate incident report"},
        {"onboarding",
         "New agent onboarding playbook.\n\n"
         "Steps:\n"
         "1. Generate KYA/DID identity for new agent\n"
         "2. Provision AES-256-GCM encrypted memory store\n"
         "3. Configure WASM sandbox boundaries\n"
         "4. Register agent in fleet registry\n"
         "5. Run baseline capability assessment\n"
         "6. Assign to appropriate squad"},
    };

    for (const auto& pb : playbooks) {
        list->addItem(pb.name);
    }

    splitter->addWidget(list);

    // Description panel
    auto* desc_panel = new QFrame(splitter);
    desc_panel->setObjectName("playbook_detail");
    desc_panel->setFrameShape(QFrame::StyledPanel);
    auto* desc_layout = new QVBoxLayout(desc_panel);
    desc_layout->setContentsMargins(20, 20, 20, 20);

    auto* desc_title = new QLabel("Select a playbook", desc_panel);
    desc_title->setObjectName("playbook_title_label");
    QFont desc_title_font;
    desc_title_font.setPointSize(16);
    desc_title_font.setBold(true);
    desc_title->setFont(desc_title_font);
    desc_layout->addWidget(desc_title);

    auto* desc_text = new QTextEdit(desc_panel);
    desc_text->setObjectName("playbook_desc_text");
    desc_text->setReadOnly(true);
    QFont desc_font;
    desc_font.setPointSize(12);
    desc_text->setFont(desc_font);
    desc_text->setStyleSheet(
        "QTextEdit { background: transparent; border: none; color: #bbb; }");
    desc_text->setPlainText("Choose a playbook from the list to see its details.");
    desc_layout->addWidget(desc_text, 1);

    // Run button
    auto* run_btn = new QPushButton("Run Playbook", desc_panel);
    run_btn->setObjectName("run_playbook_button");
    run_btn->setCursor(Qt::PointingHandCursor);
    run_btn->setMinimumHeight(40);
    run_btn->setEnabled(false);
    run_btn->setStyleSheet(
        "QPushButton { background: #0f3460; color: #fff; border: none; "
        "border-radius: 6px; font-weight: bold; font-size: 13px; padding: 8px 24px; }"
        "QPushButton:hover { background: #1a4a7a; }"
        "QPushButton:disabled { background: #333; color: #666; }");
    desc_layout->addWidget(run_btn);

    splitter->addWidget(desc_panel);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 2);

    layout->addWidget(splitter, 1);

    // Connect list selection to description
    QObject::connect(list, &QListWidget::currentRowChanged, widget,
                     [playbooks, desc_title, desc_text, run_btn](int row) {
        if (row >= 0 && row < playbooks.size()) {
            desc_title->setText(playbooks[row].name);
            desc_text->setPlainText(playbooks[row].description);
            run_btn->setEnabled(true);
        }
    });

    return widget;
}

} // namespace euxis::etx
