#include <euxis/etx/registry.hpp>

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
#include <QProcess>
#include <QFile>
#include <QCoreApplication>

#include <nlohmann/json.hpp>

namespace euxis::etx {

using json = nlohmann::json;

QWidget* create_playbooks_screen(FleetRegistry* registry, QWidget* parent) {
    auto* widget = new QWidget(parent);
    auto* layout = new QVBoxLayout(widget);
    layout->setContentsMargins(32, 32, 32, 32);
    layout->setSpacing(16);

    // Back button
    auto* top_bar = new QHBoxLayout();
    auto* back_btn = new QPushButton(QCoreApplication::translate("PlaybooksScreen", "< Back"), widget);
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
    auto* title = new QLabel(QCoreApplication::translate("PlaybooksScreen", "Playbooks"), widget);
    QFont title_font;
    title_font.setPointSize(20);
    title_font.setBold(true);
    title->setFont(title_font);
    layout->addWidget(title);

    auto* count_label = new QLabel(widget);
    count_label->setStyleSheet("color: #888; font-size: 12px;");
    layout->addWidget(count_label);

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

    // Populate from registry
    const auto& playbooks = registry->playbooks();
    count_label->setText(QCoreApplication::translate("PlaybooksScreen", "%1 playbooks available").arg(playbooks.size()));

    for (const auto& pb : playbooks) {
        auto* item = new QListWidgetItem(pb.name);
        item->setData(Qt::UserRole, pb.file_path);
        item->setData(Qt::UserRole + 1, pb.id);
        list->addItem(item);
    }

    splitter->addWidget(list);

    // Description panel
    auto* desc_panel = new QFrame(splitter);
    desc_panel->setObjectName("playbook_detail");
    desc_panel->setFrameShape(QFrame::StyledPanel);
    auto* desc_layout = new QVBoxLayout(desc_panel);
    desc_layout->setContentsMargins(20, 20, 20, 20);

    auto* desc_title = new QLabel(QCoreApplication::translate("PlaybooksScreen", "Select a playbook"), desc_panel);
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
    desc_text->setPlainText(QCoreApplication::translate("PlaybooksScreen", "Choose a playbook from the list to see its details."));
    desc_layout->addWidget(desc_text, 1);

    // Run button
    auto* run_btn = new QPushButton(QCoreApplication::translate("PlaybooksScreen", "Run Playbook"), desc_panel);
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

    // Connect list selection to description — load full JSON
    QObject::connect(list, &QListWidget::currentItemChanged, widget,
                     [desc_title, desc_text, run_btn](QListWidgetItem* current, QListWidgetItem*) {
        if (!current) return;

        QString file_path = current->data(Qt::UserRole).toString();
        desc_title->setText(current->text());
        run_btn->setEnabled(true);

        // Load full playbook JSON for details
        QFile file(file_path);
        if (!file.open(QIODevice::ReadOnly)) {
            desc_text->setPlainText(QCoreApplication::translate("PlaybooksScreen", "Could not load playbook file."));
            return;
        }

        try {
            auto doc = json::parse(file.readAll().toStdString());
            QString details;
            details += QString::fromStdString(doc.value("description", "")) + "\n\n";

            if (doc.contains("phases") && doc["phases"].is_array()) {
                details += QCoreApplication::translate("PlaybooksScreen", "Phases: %1").arg(doc["phases"].size()) + "\n\n";
                for (const auto& phase : doc["phases"]) {
                    int num = phase.value("phase", 0);
                    std::string pname = phase.value("name", "");
                    std::string mode = phase.value("mode", "");
                    std::string checkpoint = phase.value("checkpoint", "");

                    details += QCoreApplication::translate("PlaybooksScreen", "  Phase %1: %2")
                        .arg(num)
                        .arg(QString::fromStdString(pname));

                    if (!mode.empty()) {
                        details += QString(" [%1]").arg(QString::fromStdString(mode));
                    }
                    details += "\n";

                    if (phase.contains("squad")) {
                        details += QCoreApplication::translate("PlaybooksScreen", "    Squad: %1\n")
                            .arg(QString::fromStdString(phase["squad"].get<std::string>()));
                    }

                    if (phase.contains("delegates") && phase["delegates"].is_array()) {
                        for (const auto& d : phase["delegates"]) {
                            details += QCoreApplication::translate("PlaybooksScreen", "    Agent: %1\n")
                                .arg(QString::fromStdString(d.value("agent", "")));
                        }
                    }

                    if (!checkpoint.empty()) {
                        details += QCoreApplication::translate("PlaybooksScreen", "    Checkpoint: %1\n")
                            .arg(QString::fromStdString(checkpoint));
                    }
                    details += "\n";
                }
            }
            desc_text->setPlainText(details);
        } catch (const json::exception&) {
            desc_text->setPlainText(QCoreApplication::translate("PlaybooksScreen", "Error parsing playbook JSON."));
        }
    });

    // Run button action
    QObject::connect(run_btn, &QPushButton::clicked, widget, [list, desc_text]() {
        auto* current = list->currentItem();
        if (!current) return;

        QString pb_id = current->data(Qt::UserRole + 1).toString();
        desc_text->append("\n--- " + QCoreApplication::translate("PlaybooksScreen", "Running playbook: %1").arg(pb_id) + " ---\n");

        auto* process = new QProcess(desc_text);
        process->setProgram("euxis-cli");
        process->setArguments({"playbook", "run", pb_id});

        QObject::connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            desc_text, [desc_text, process](int exitCode, QProcess::ExitStatus) {
                QString out = process->readAllStandardOutput();
                QString err = process->readAllStandardError();
                if (!out.isEmpty()) desc_text->append(out.trimmed());
                if (!err.isEmpty()) desc_text->append(QCoreApplication::translate("PlaybooksScreen", "Error: %1").arg(err.trimmed()));
                desc_text->append(QCoreApplication::translate("PlaybooksScreen", "Exit code: %1").arg(exitCode));
                process->deleteLater();
            });

        process->start();
    });

    return widget;
}

} // namespace euxis::etx
