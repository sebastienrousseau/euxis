#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QFont>
#include <QFrame>
#include <QWidget>
#include <QStackedWidget>

namespace euxis::etx {

QWidget* create_agent_screen(QWidget* parent) {
    auto* widget = new QWidget(parent);
    auto* layout = new QVBoxLayout(widget);
    layout->setContentsMargins(32, 32, 32, 32);
    layout->setSpacing(16);

    // Back button
    auto* top_bar = new QHBoxLayout();
    auto* back_btn = new QPushButton("< Back", widget);
    back_btn->setObjectName("back_button");
    back_btn->setCursor(Qt::PointingHandCursor);
    back_btn->setFixedWidth(100);
    top_bar->addWidget(back_btn);
    top_bar->addStretch();
    layout->addLayout(top_bar);

    QObject::connect(back_btn, &QPushButton::clicked, widget, [widget]() {
        // Navigate back to dashboard (index 1)
        if (auto* stack = qobject_cast<QStackedWidget*>(widget->parentWidget())) {
            stack->setCurrentIndex(1);
        }
    });

    // Agent info card
    auto* card = new QFrame(widget);
    card->setObjectName("agent_detail_card");
    card->setFrameShape(QFrame::StyledPanel);
    auto* card_layout = new QVBoxLayout(card);
    card_layout->setSpacing(12);

    // Agent name
    auto* name_label = new QLabel("Agent Name", card);
    name_label->setObjectName("agent_name_label");
    QFont name_font;
    name_font.setPointSize(24);
    name_font.setBold(true);
    name_label->setFont(name_font);
    card_layout->addWidget(name_label);

    // Agent description
    auto* desc_label = new QLabel("Agent description will appear here.", card);
    desc_label->setObjectName("agent_desc_label");
    desc_label->setWordWrap(true);
    QFont desc_font;
    desc_font.setPointSize(13);
    desc_label->setFont(desc_font);
    desc_label->setStyleSheet("color: #aaa;");
    card_layout->addWidget(desc_label);

    // Status
    auto* status_label = new QLabel("Status: idle", card);
    status_label->setObjectName("agent_status_label");
    QFont status_font;
    status_font.setPointSize(12);
    status_label->setFont(status_font);
    status_label->setStyleSheet("color: #888;");
    card_layout->addWidget(status_label);

    layout->addWidget(card);

    // Separator
    auto* separator = new QFrame(widget);
    separator->setFrameShape(QFrame::HLine);
    separator->setStyleSheet("color: rgba(255,255,255,0.1);");
    layout->addWidget(separator);

    // Deploy section
    auto* deploy_label = new QLabel("Deploy Agent", widget);
    QFont deploy_font;
    deploy_font.setPointSize(16);
    deploy_font.setBold(true);
    deploy_label->setFont(deploy_font);
    layout->addWidget(deploy_label);

    // Task input
    auto* task_layout = new QHBoxLayout();
    auto* task_input = new QLineEdit(widget);
    task_input->setObjectName("task_input");
    task_input->setPlaceholderText("Enter task description...");
    task_input->setMinimumHeight(40);
    task_layout->addWidget(task_input);

    auto* deploy_btn = new QPushButton("Deploy", widget);
    deploy_btn->setObjectName("deploy_button");
    deploy_btn->setCursor(Qt::PointingHandCursor);
    deploy_btn->setMinimumHeight(40);
    deploy_btn->setMinimumWidth(120);
    deploy_btn->setStyleSheet(
        "QPushButton { background: #0f3460; color: #fff; border: none; "
        "border-radius: 6px; font-weight: bold; font-size: 14px; padding: 8px 24px; }"
        "QPushButton:hover { background: #1a4a7a; }");
    task_layout->addWidget(deploy_btn);

    layout->addLayout(task_layout);

    QObject::connect(deploy_btn, &QPushButton::clicked, widget, [task_input, name_label]() {
        // Deploy action placeholder
        QString task = task_input->text();
        if (!task.isEmpty()) {
            task_input->clear();
            task_input->setPlaceholderText("Task submitted: " + task);
        }
    });

    layout->addStretch();

    return widget;
}

} // namespace euxis::etx
