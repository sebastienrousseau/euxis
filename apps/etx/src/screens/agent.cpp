#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QFont>
#include <QFrame>
#include <QWidget>
#include <QStackedWidget>
#include <QProcess>
#include <QDateTime>
#include <QScrollArea>
#include <QCoreApplication>

namespace euxis::etx {

QWidget* create_agent_screen(QWidget* parent) {
    auto* widget = new QWidget(parent);
    auto* outer_layout = new QVBoxLayout(widget);
    outer_layout->setContentsMargins(0, 0, 0, 0);
    outer_layout->setSpacing(0);

    // Scroll area for the whole screen
    auto* scroll = new QScrollArea(widget);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setStyleSheet("QScrollArea { background: transparent; border: none; }");

    auto* scroll_content = new QWidget(scroll);
    auto* layout = new QVBoxLayout(scroll_content);
    layout->setContentsMargins(32, 32, 32, 32);
    layout->setSpacing(16);

    // Back button
    auto* top_bar = new QHBoxLayout();
    auto* back_btn = new QPushButton(QCoreApplication::translate("AgentScreen", "< Back"), scroll_content);
    back_btn->setObjectName("back_button");
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

    // Agent info card
    auto* card = new QFrame(scroll_content);
    card->setObjectName("agent_detail_card");
    card->setFrameShape(QFrame::StyledPanel);
    auto* card_layout = new QVBoxLayout(card);
    card_layout->setSpacing(12);

    // Agent name
    auto* name_label = new QLabel(QCoreApplication::translate("AgentScreen", "Agent Name"), card);
    name_label->setObjectName("agent_name_label");
    QFont name_font;
    name_font.setPointSize(24);
    name_font.setBold(true);
    name_label->setFont(name_font);
    card_layout->addWidget(name_label);

    // Agent description
    auto* desc_label = new QLabel(QCoreApplication::translate("AgentScreen", "Agent description will appear here."), card);
    desc_label->setObjectName("agent_desc_label");
    desc_label->setWordWrap(true);
    QFont desc_font;
    desc_font.setPointSize(13);
    desc_label->setFont(desc_font);
    desc_label->setStyleSheet("color: #aaa;");
    card_layout->addWidget(desc_label);

    // Status
    auto* status_label = new QLabel(QCoreApplication::translate("AgentScreen", "Status: idle"), card);
    status_label->setObjectName("agent_status_label");
    QFont status_font;
    status_font.setPointSize(12);
    status_label->setFont(status_font);
    status_label->setStyleSheet("color: #888;");
    card_layout->addWidget(status_label);

    // Extended info row: tier, activation, version
    auto* info_row = new QHBoxLayout();
    info_row->setSpacing(16);

    auto* tier_label = new QLabel(QCoreApplication::translate("AgentScreen", "Tier: --"), card);
    tier_label->setObjectName("agent_tier_label");
    tier_label->setStyleSheet("color: #6db3f2; font-weight: bold; font-size: 12px;");
    info_row->addWidget(tier_label);

    auto* activation_label = new QLabel(QCoreApplication::translate("AgentScreen", "Activation: --"), card);
    activation_label->setObjectName("agent_activation_label");
    activation_label->setStyleSheet("color: #888; font-size: 12px;");
    info_row->addWidget(activation_label);

    auto* version_label = new QLabel(QCoreApplication::translate("AgentScreen", "Version: --"), card);
    version_label->setObjectName("agent_version_label");
    version_label->setStyleSheet("color: #888; font-size: 12px;");
    info_row->addWidget(version_label);

    info_row->addStretch();
    card_layout->addLayout(info_row);

    // Tags row
    auto* tags_frame = new QFrame(card);
    auto* tags_layout = new QVBoxLayout(tags_frame);
    tags_layout->setContentsMargins(0, 4, 0, 4);
    tags_layout->setSpacing(4);

    auto* tags_header = new QLabel(QCoreApplication::translate("AgentScreen", "Tags"), card);
    tags_header->setStyleSheet("color: #888; font-weight: bold; font-size: 11px;");
    tags_layout->addWidget(tags_header);

    auto* tags_label = new QLabel(QCoreApplication::translate("AgentScreen", "No tags"), card);
    tags_label->setObjectName("agent_tags_label");
    tags_label->setWordWrap(true);
    tags_label->setStyleSheet(
        "color: #aaa; font-size: 12px; background: rgba(255,255,255,0.03); "
        "padding: 6px 10px; border-radius: 4px;");
    tags_layout->addWidget(tags_label);
    card_layout->addWidget(tags_frame);

    // Capabilities row
    auto* caps_frame = new QFrame(card);
    auto* caps_layout = new QVBoxLayout(caps_frame);
    caps_layout->setContentsMargins(0, 4, 0, 4);
    caps_layout->setSpacing(4);

    auto* caps_header = new QLabel(QCoreApplication::translate("AgentScreen", "Capabilities"), card);
    caps_header->setStyleSheet("color: #888; font-weight: bold; font-size: 11px;");
    caps_layout->addWidget(caps_header);

    auto* caps_label = new QLabel(QCoreApplication::translate("AgentScreen", "No capabilities"), card);
    caps_label->setObjectName("agent_caps_label");
    caps_label->setWordWrap(true);
    caps_label->setStyleSheet(
        "color: #aaa; font-size: 12px; background: rgba(255,255,255,0.03); "
        "padding: 6px 10px; border-radius: 4px;");
    caps_layout->addWidget(caps_label);
    card_layout->addWidget(caps_frame);

    layout->addWidget(card);

    // Prompt content section
    auto* prompt_header = new QLabel(QCoreApplication::translate("AgentScreen", "System Prompt"), scroll_content);
    QFont prompt_header_font;
    prompt_header_font.setPointSize(14);
    prompt_header_font.setBold(true);
    prompt_header->setFont(prompt_header_font);
    layout->addWidget(prompt_header);

    auto* prompt_edit = new QPlainTextEdit(scroll_content);
    prompt_edit->setObjectName("agent_prompt_edit");
    prompt_edit->setReadOnly(true);
    QFont prompt_mono;
    prompt_mono.setFamily("monospace");
    prompt_mono.setPointSize(10);
    prompt_edit->setFont(prompt_mono);
    prompt_edit->setStyleSheet(
        "QPlainTextEdit { background: rgba(0,0,0,0.3); color: #c8c8c8; "
        "border: 1px solid rgba(255,255,255,0.1); border-radius: 8px; "
        "padding: 12px; }");
    prompt_edit->setMinimumHeight(200);
    prompt_edit->setPlainText(QCoreApplication::translate("AgentScreen", "(select an agent to view its prompt)"));
    layout->addWidget(prompt_edit);

    // Separator
    auto* separator = new QFrame(scroll_content);
    separator->setFrameShape(QFrame::HLine);
    separator->setStyleSheet("color: rgba(255,255,255,0.1);");
    layout->addWidget(separator);

    // Deploy section
    auto* deploy_label = new QLabel(QCoreApplication::translate("AgentScreen", "Deploy Agent"), scroll_content);
    QFont deploy_font;
    deploy_font.setPointSize(16);
    deploy_font.setBold(true);
    deploy_label->setFont(deploy_font);
    layout->addWidget(deploy_label);

    // Task input
    auto* task_layout = new QHBoxLayout();
    auto* task_input = new QLineEdit(scroll_content);
    task_input->setObjectName("task_input");
    task_input->setPlaceholderText(QCoreApplication::translate("AgentScreen", "Enter task description..."));
    task_input->setMinimumHeight(40);
    task_layout->addWidget(task_input);

    auto* deploy_btn = new QPushButton(QCoreApplication::translate("AgentScreen", "Deploy"), scroll_content);
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

    // Deploy output area
    auto* deploy_output = new QPlainTextEdit(scroll_content);
    deploy_output->setObjectName("deploy_output");
    deploy_output->setReadOnly(true);
    QFont deploy_mono;
    deploy_mono.setFamily("monospace");
    deploy_mono.setPointSize(11);
    deploy_output->setFont(deploy_mono);
    deploy_output->setStyleSheet(
        "QPlainTextEdit { background: rgba(0,0,0,0.3); color: #c8c8c8; "
        "border: 1px solid rgba(255,255,255,0.1); border-radius: 8px; "
        "padding: 12px; }");
    deploy_output->setMaximumHeight(200);
    layout->addWidget(deploy_output);

    QObject::connect(deploy_btn, &QPushButton::clicked, scroll_content,
        [task_input, name_label, deploy_output, status_label]() {
            QString task = task_input->text().trimmed();
            if (task.isEmpty()) return;

            QString agent_name = name_label->text();
            deploy_output->appendPlainText(
                QDateTime::currentDateTime().toString("[hh:mm:ss]") +
                QCoreApplication::translate("AgentScreen", " Deploying %1 with task: %2").arg(agent_name, task));

            auto* process = new QProcess(deploy_output);
            process->setProgram("euxis-cli");
            process->setArguments({"agent", "run", agent_name, "--task", task});

            QObject::connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                deploy_output, [deploy_output, process](int exitCode, QProcess::ExitStatus) {
                    QString out = process->readAllStandardOutput();
                    QString err = process->readAllStandardError();
                    if (!out.isEmpty()) deploy_output->appendPlainText(out.trimmed());
                    if (!err.isEmpty()) deploy_output->appendPlainText(QCoreApplication::translate("AgentScreen", "Error: %1").arg(err.trimmed()));
                    deploy_output->appendPlainText(
                        QDateTime::currentDateTime().toString("[hh:mm:ss]") +
                        QCoreApplication::translate("AgentScreen", " Exit code: %1").arg(exitCode));
                    process->deleteLater();
                });

            process->start();
            task_input->clear();

            status_label->setText(QCoreApplication::translate("AgentScreen", "Status: deploying..."));
        });

    layout->addStretch();
    scroll->setWidget(scroll_content);
    outer_layout->addWidget(scroll);

    return widget;
}

} // namespace euxis::etx
