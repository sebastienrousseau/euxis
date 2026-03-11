#include <euxis/etx/config.hpp>

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QFont>
#include <QWidget>
#include <QStackedWidget>
#include <QDateTime>
#include <QFile>
#include <QTextStream>
#include <QFileInfo>
#include <QCoreApplication>

namespace euxis::etx {

static void load_daemon_log(QPlainTextEdit* log_edit, const QString& runtime_dir) {
    QString log_path = runtime_dir + "/logs/daemon.log";
    QFile file(log_path);

    if (!file.exists()) {
        log_edit->setPlainText(QCoreApplication::translate("LogsScreen", "No daemon log found at:\n%1\n\nThe daemon has not been started yet, or logs are stored elsewhere.").arg(log_path));
        return;
    }

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        log_edit->setPlainText(QCoreApplication::translate("LogsScreen", "Could not open daemon log:\n%1").arg(log_path));
        return;
    }

    // Read last 500 lines
    QStringList lines;
    QTextStream in(&file);
    while (!in.atEnd()) {
        lines.append(in.readLine());
        if (lines.size() > 500) {
            lines.removeFirst();
        }
    }

    log_edit->setPlainText(lines.join('\n'));

    // Scroll to bottom
    auto cursor = log_edit->textCursor();
    cursor.movePosition(QTextCursor::End);
    log_edit->setTextCursor(cursor);
}

QWidget* create_logs_screen(ETXConfig* /*config*/, QWidget* parent) {
    auto* widget = new QWidget(parent);
    auto* layout = new QVBoxLayout(widget);
    layout->setContentsMargins(32, 32, 32, 32);
    layout->setSpacing(16);

    // Top bar with back button
    auto* top_bar = new QHBoxLayout();
    auto* back_btn = new QPushButton(QCoreApplication::translate("LogsScreen", "< Back"), widget);
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
    auto* title = new QLabel(QCoreApplication::translate("LogsScreen", "System Logs"), widget);
    QFont title_font;
    title_font.setPointSize(20);
    title_font.setBold(true);
    title->setFont(title_font);
    layout->addWidget(title);

    // Source label
    QString runtime_dir = ETXConfig::runtime_dir();
    auto* source_label = new QLabel(QCoreApplication::translate("LogsScreen", "Source: %1/logs/daemon.log").arg(runtime_dir), widget);
    source_label->setStyleSheet("color: #666; font-size: 11px;");
    layout->addWidget(source_label);

    // Log display
    auto* log_edit = new QPlainTextEdit(widget);
    log_edit->setObjectName("log_display");
    log_edit->setReadOnly(true);
    QFont mono_font;
    mono_font.setFamily("monospace");
    mono_font.setPointSize(11);
    log_edit->setFont(mono_font);
    log_edit->setMaximumBlockCount(10000);
    log_edit->setStyleSheet(
        "QPlainTextEdit { background: rgba(0,0,0,0.3); color: #c8c8c8; "
        "border: 1px solid rgba(255,255,255,0.1); border-radius: 8px; "
        "padding: 12px; }");

    // Load daemon.log
    load_daemon_log(log_edit, runtime_dir);

    layout->addWidget(log_edit, 1);

    // Button row
    auto* button_row = new QHBoxLayout();
    button_row->addStretch();

    auto* clear_btn = new QPushButton(QCoreApplication::translate("LogsScreen", "Clear"), widget);
    clear_btn->setCursor(Qt::PointingHandCursor);
    clear_btn->setMinimumHeight(36);
    clear_btn->setMinimumWidth(100);
    button_row->addWidget(clear_btn);

    auto* refresh_btn = new QPushButton(QCoreApplication::translate("LogsScreen", "Refresh"), widget);
    refresh_btn->setCursor(Qt::PointingHandCursor);
    refresh_btn->setMinimumHeight(36);
    refresh_btn->setMinimumWidth(100);
    button_row->addWidget(refresh_btn);

    layout->addLayout(button_row);

    QObject::connect(clear_btn, &QPushButton::clicked, log_edit, &QPlainTextEdit::clear);

    QObject::connect(refresh_btn, &QPushButton::clicked, widget,
                     [log_edit, runtime_dir]() {
        load_daemon_log(log_edit, runtime_dir);
    });

    return widget;
}

} // namespace euxis::etx
