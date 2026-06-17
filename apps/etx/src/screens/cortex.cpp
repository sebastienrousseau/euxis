#include <euxis/etx/config.hpp>

#include <nlohmann/json.hpp>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QFont>
#include <QFrame>
#include <QWidget>
#include <QStackedWidget>
#include <QShortcut>
#include <QDateTime>
#include <QProcess>
#include <QFile>
#include <QCoreApplication>

namespace euxis::etx {

using json = nlohmann::json;

static int count_cortex_entries(const QString& runtime_dir) {
    QString path = runtime_dir + "/memory/cortex/entries.json";
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return 0;

    try {
        auto doc = json::parse(file.readAll().toStdString());
        if (doc.is_array()) return static_cast<int>(doc.size());
        if (doc.is_object()) return static_cast<int>(doc.size());
    } catch (const json::exception&) {
        // ignore — swallowed: best-effort
        (void)0;
    }
    return 0;
}

QWidget* create_cortex_screen(ETXConfig* /*config*/, QWidget* parent) {
    auto* widget = new QWidget(parent);
    auto* layout = new QVBoxLayout(widget);
    layout->setContentsMargins(32, 32, 32, 32);
    layout->setSpacing(16);

    // Back button
    auto* top_bar = new QHBoxLayout();
    auto* back_btn = new QPushButton(QCoreApplication::translate("CortexScreen", "< Back"), widget);
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
    auto* title = new QLabel(QCoreApplication::translate("CortexScreen", "Cortex Memory Browser"), widget);
    QFont title_font;
    title_font.setPointSize(20);
    title_font.setBold(true);
    title->setFont(title_font);
    layout->addWidget(title);

    // Status label — real entry count
    QString runtime_dir = ETXConfig::runtime_dir();
    // NOLINTNEXTLINE(cppcoreguidelines-init-variables): function-call init not detected
    int entry_count = count_cortex_entries(runtime_dir);
    auto* status_label = new QLabel(widget);
    status_label->setObjectName("cortex_status");
    if (entry_count > 0) {
        status_label->setText(QCoreApplication::translate("CortexScreen", "Cortex: %1 entries | Last sync: %2")
            .arg(entry_count)
            .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm")));
    } else {
        status_label->setText(QCoreApplication::translate("CortexScreen", "Cortex: No entries found | %1/memory/cortex/entries.json").arg(runtime_dir));
    }
    status_label->setStyleSheet("color: #888; font-size: 12px;");
    layout->addWidget(status_label);

    // Separator
    auto* separator = new QFrame(widget);
    separator->setFrameShape(QFrame::HLine);
    separator->setStyleSheet("color: rgba(255,255,255,0.1);");
    layout->addWidget(separator);

    // Query input row
    auto* query_row = new QHBoxLayout();

    auto* query_input = new QLineEdit(widget);
    query_input->setObjectName("cortex_query_input");
    query_input->setPlaceholderText(QCoreApplication::translate("CortexScreen", "Search cortex memory..."));
    query_input->setMinimumHeight(40);
    query_input->setStyleSheet(
        "QLineEdit { background: rgba(255,255,255,0.05); "
        "border: 1px solid rgba(255,255,255,0.15); border-radius: 6px; "
        "padding: 8px 12px; color: #ddd; font-size: 13px; }"
        "QLineEdit:focus { border-color: rgba(15,52,96,0.8); }");
    query_row->addWidget(query_input);

    auto* search_btn = new QPushButton(QCoreApplication::translate("CortexScreen", "Search"), widget);
    search_btn->setObjectName("cortex_search_button");
    search_btn->setCursor(Qt::PointingHandCursor);
    search_btn->setMinimumHeight(40);
    search_btn->setMinimumWidth(100);
    search_btn->setStyleSheet(
        "QPushButton { background: #0f3460; color: #fff; border: none; "
        "border-radius: 6px; font-weight: bold; font-size: 13px; padding: 8px 24px; }"
        "QPushButton:hover { background: #1a4a7a; }");
    query_row->addWidget(search_btn);

    layout->addLayout(query_row);

    // Output display
    auto* output = new QPlainTextEdit(widget);
    output->setObjectName("cortex_output");
    output->setReadOnly(true);
    QFont mono_font;
    mono_font.setFamily("monospace");
    mono_font.setPointSize(11);
    output->setFont(mono_font);
    output->setStyleSheet(
        "QPlainTextEdit { background: rgba(0,0,0,0.3); color: #c8c8c8; "
        "border: 1px solid rgba(255,255,255,0.1); border-radius: 8px; "
        "padding: 12px; }");
    output->setPlainText(QCoreApplication::translate("CortexScreen",
        "Cortex semantic memory engine ready.\n"
        "Enter a query to search across agent memory stores.\n\n"
        "Supported query types:\n"
        "  - Natural language: \"What did the security agent find?\"\n"
        "  - Namespace filter: ns:bridge-agent <query>\n"
        "  - Similarity threshold: threshold:0.8 <query>\n"
        "  - Time range: after:2026-02-01 <query>"));
    layout->addWidget(output, 1);

    // Search action
    auto search_action = [query_input, output, status_label]() {
        QString query = query_input->text().trimmed();
        if (query.isEmpty()) return;

        auto ts = QDateTime::currentDateTime().toString("[hh:mm:ss]");
        output->appendPlainText("\n" + ts + QCoreApplication::translate("CortexScreen", " Query: %1").arg(query));
        output->appendPlainText(QString(60, '-'));

        auto* process = new QProcess(query_input->parentWidget());
        process->setProgram("euxis-cli");
        process->setArguments({"cortex", "recall", query});
        auto start_time = QDateTime::currentDateTime();

        QObject::connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            query_input->parentWidget(), [output, status_label, start_time, process](int exitCode, QProcess::ExitStatus) {
                QString result_text = process->readAllStandardOutput();
                QString error_text = process->readAllStandardError();
                auto elapsed = start_time.msecsTo(QDateTime::currentDateTime());

                if (exitCode == 0 && !result_text.isEmpty()) {
                    output->appendPlainText(result_text.trimmed());
                } else if (!error_text.isEmpty()) {
                    output->appendPlainText(QCoreApplication::translate("CortexScreen", "  Error: %1").arg(error_text.trimmed()));
                } else {
                    output->appendPlainText(QCoreApplication::translate("CortexScreen", "  No results found."));
                }
                output->appendPlainText(QCoreApplication::translate("CortexScreen", "\n  (completed in %1ms)").arg(elapsed));

                status_label->setText(QCoreApplication::translate("CortexScreen", "Cortex: Last query: %1")
                                      .arg(QDateTime::currentDateTime().toString("hh:mm:ss")));
                process->deleteLater();
            });

        process->start();
        if (!process->waitForStarted(3000)) {
            output->appendPlainText(QCoreApplication::translate("CortexScreen", "  Error: Could not start euxis-cli"));
        }
        query_input->clear();
    };

    QObject::connect(search_btn, &QPushButton::clicked, widget, search_action);
    QObject::connect(query_input, &QLineEdit::returnPressed, widget, search_action);

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
