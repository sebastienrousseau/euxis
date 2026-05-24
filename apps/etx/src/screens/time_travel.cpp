#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QListWidget>
#include <QSlider>
#include <QScrollArea>
#include <QFont>
#include <QFrame>
#include <QWidget>
#include <QStackedWidget>
#include <QShortcut>
#include <QSplitter>
#include <QCoreApplication>

namespace euxis::etx {

QWidget* create_time_travel_screen(QWidget* parent) {
    auto* widget = new QWidget(parent);
    auto* layout = new QVBoxLayout(widget);
    layout->setContentsMargins(32, 32, 32, 32);
    layout->setSpacing(16);

    // Back button
    auto* top_bar = new QHBoxLayout();
    auto* back_btn = new QPushButton(QCoreApplication::translate("TimeTravelScreen", "< Back"), widget);
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
    auto* title = new QLabel(QCoreApplication::translate("TimeTravelScreen", "Time Travel"), widget);
    QFont title_font;
    title_font.setPointSize(20);
    title_font.setBold(true);
    title->setFont(title_font);
    layout->addWidget(title);

    auto* subtitle = new QLabel(QCoreApplication::translate("TimeTravelScreen", "Mission replay timeline \u2014 scrub through past events"), widget);
    subtitle->setStyleSheet("color: #888; font-size: 12px;");
    layout->addWidget(subtitle);

    // Splitter: mission list on left, timeline on right
    auto* splitter = new QSplitter(Qt::Horizontal, widget);

    // Mission list
    auto* mission_list = new QListWidget(splitter);
    mission_list->setObjectName("mission_list");
    mission_list->setMinimumWidth(220);
    mission_list->setMaximumWidth(280);
    mission_list->setStyleSheet(
        "QListWidget { background: rgba(255,255,255,0.03); "
        "border: 1px solid rgba(255,255,255,0.1); border-radius: 8px; "
        "padding: 8px; }"
        "QListWidget::item { padding: 10px 12px; border-radius: 4px; }"
        "QListWidget::item:selected { background: rgba(15,52,96,0.6); }");

    mission_list->addItem("run-0042: code-agent deploy");
    mission_list->addItem("run-0041: identity verify");
    mission_list->addItem("run-0040: bridge-squad import");
    mission_list->addItem("run-0039: security audit");
    mission_list->addItem("run-0038: ops-combo restart");

    splitter->addWidget(mission_list);

    // Right panel: timeline + events
    auto* right_panel = new QWidget(splitter);
    auto* right_layout = new QVBoxLayout(right_panel);
    right_layout->setContentsMargins(0, 0, 0, 0);
    right_layout->setSpacing(12);

    // Timeline controls
    auto* timeline_card = new QFrame(right_panel);
    timeline_card->setObjectName("timeline_card");
    timeline_card->setFrameShape(QFrame::StyledPanel);
    auto* timeline_layout = new QVBoxLayout(timeline_card);
    timeline_layout->setContentsMargins(16, 12, 16, 12);
    timeline_layout->setSpacing(8);

    // Position label
    auto* position_label = new QLabel(QCoreApplication::translate("TimeTravelScreen", "Event %1 of %2").arg(1).arg(5), timeline_card);
    position_label->setObjectName("timeline_position");
    position_label->setAlignment(Qt::AlignCenter);
    position_label->setStyleSheet("color: #ccc; font-size: 14px; font-weight: bold;");
    timeline_layout->addWidget(position_label);

    // Slider
    auto* slider = new QSlider(Qt::Horizontal, timeline_card);
    slider->setObjectName("timeline_slider");
    slider->setRange(0, 4);
    slider->setValue(0);
    slider->setTickPosition(QSlider::TicksBelow);
    slider->setTickInterval(1);
    slider->setStyleSheet(
        "QSlider::groove:horizontal { background: rgba(255,255,255,0.1); "
        "height: 6px; border-radius: 3px; }"
        "QSlider::handle:horizontal { background: #0f3460; "
        "width: 18px; height: 18px; margin: -6px 0; border-radius: 9px; }"
        "QSlider::sub-page:horizontal { background: #0f3460; border-radius: 3px; }");
    timeline_layout->addWidget(slider);

    right_layout->addWidget(timeline_card);

    // Event display area
    auto* event_scroll = new QScrollArea(right_panel);
    event_scroll->setObjectName("event_scroll");
    event_scroll->setWidgetResizable(true);
    event_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    event_scroll->setStyleSheet(
        "QScrollArea { background: transparent; border: none; }");

    auto* event_container = new QWidget(event_scroll);
    auto* event_layout = new QVBoxLayout(event_container);
    event_layout->setSpacing(8);

    // Sample events for mission run-0042
    struct Event {
        QString type_icon{};
        QString timestamp{};
        QString agent{};
        QString data{};
    };

    QList<Event> events = {
        {"[START]",  "14:20:01", "ops-combo",
         "Mission initiated: deploy code-agent task 'write auth module'"},
        {"[PLAN]",   "14:20:03", "code-agent",
         "Plan generated: 4 steps — analyze requirements, generate code, "
         "write tests, submit for review"},
        {"[EXEC]",   "14:20:15", "code-agent",
         "Step 1/4 complete: requirements analyzed, 284 lines planned for "
         "src/auth.py (JWT, sessions, RBAC)"},
        {"[HITL]",   "14:21:02", "code-agent",
         "HITL approval requested: file-write to src/auth.py — awaiting "
         "human confirmation"},
        {"[DONE]",   "14:22:30", "code-agent",
         "Mission complete: auth module written and submitted for review. "
         "Token usage: 7,200. Duration: 2m 29s"},
    };

    // Build event cards
    QList<QFrame*> event_cards;
    for (const auto& evt : events) {
        auto* card = new QFrame(event_container);
        card->setObjectName("event_card");
        card->setFrameShape(QFrame::StyledPanel);
        card->setStyleSheet(
            "QFrame#event_card { background: rgba(255,255,255,0.03); "
            "border: 1px solid rgba(255,255,255,0.1); border-radius: 8px; }");
        auto* card_layout = new QVBoxLayout(card);
        card_layout->setContentsMargins(16, 12, 16, 12);
        card_layout->setSpacing(6);

        // Header row: type icon + timestamp + agent
        auto* header_row = new QHBoxLayout();

        auto* type_label = new QLabel(evt.type_icon, card);
        QFont type_font;
        type_font.setFamily("monospace");
        type_font.setBold(true);
        type_font.setPointSize(11);
        type_label->setFont(type_font);

        // Color-code by type
        QString type_color = "#888";
        if (evt.type_icon.contains("START")) type_color = "#4caf50";
        else if (evt.type_icon.contains("PLAN")) type_color = "#2196f3";
        else if (evt.type_icon.contains("EXEC")) type_color = "#ff9800";
        else if (evt.type_icon.contains("HITL")) type_color = "#f44336";
        else if (evt.type_icon.contains("DONE")) type_color = "#4caf50";
        type_label->setStyleSheet(QString("color: %1;").arg(type_color));
        header_row->addWidget(type_label);

        auto* ts_label = new QLabel(evt.timestamp, card);
        ts_label->setStyleSheet("color: #888; font-size: 11px;");
        header_row->addWidget(ts_label);

        auto* agent_label = new QLabel(evt.agent, card);
        agent_label->setStyleSheet(
            "background: rgba(255,255,255,0.08); color: #ccc; "
            "padding: 2px 8px; border-radius: 4px; font-size: 11px;");
        header_row->addWidget(agent_label);
        header_row->addStretch();
        card_layout->addLayout(header_row);

        // Data preview
        auto* data_label = new QLabel(evt.data, card);
        data_label->setWordWrap(true);
        data_label->setStyleSheet("color: #bbb; font-size: 12px;");
        card_layout->addWidget(data_label);

        event_layout->addWidget(card);
        event_cards.append(card);
    }

    event_layout->addStretch();
    event_scroll->setWidget(event_container);
    right_layout->addWidget(event_scroll, 1);

    splitter->addWidget(right_panel);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);

    layout->addWidget(splitter, 1);

    // Action buttons
    auto* button_row = new QHBoxLayout();
    button_row->addStretch();

    auto* retry_btn = new QPushButton(QCoreApplication::translate("TimeTravelScreen", "Retry Checkpoint"), widget);
    retry_btn->setObjectName("retry_button");
    retry_btn->setCursor(Qt::PointingHandCursor);
    retry_btn->setMinimumHeight(40);
    retry_btn->setMinimumWidth(140);
    retry_btn->setStyleSheet(
        "QPushButton { background: #0f3460; color: #fff; border: none; "
        "border-radius: 6px; font-weight: bold; font-size: 13px; padding: 8px 24px; }"
        "QPushButton:hover { background: #1a4a7a; }");
    button_row->addWidget(retry_btn);

    auto* export_btn = new QPushButton(QCoreApplication::translate("TimeTravelScreen", "Export"), widget);
    export_btn->setObjectName("export_button");
    export_btn->setCursor(Qt::PointingHandCursor);
    export_btn->setMinimumHeight(40);
    export_btn->setMinimumWidth(100);
    button_row->addWidget(export_btn);

    layout->addLayout(button_row);

    // Slider change -> update position label and highlight current event card
    QObject::connect(slider, &QSlider::valueChanged, widget,
                     [position_label, event_cards](int value) {
        position_label->setText(QCoreApplication::translate("TimeTravelScreen", "Event %1 of %2")
                                .arg(value + 1)
                                .arg(event_cards.size()));

        // Highlight current event, dim others
        for (int i = 0; i < event_cards.size(); ++i) {
            if (i == value) {
                event_cards[i]->setStyleSheet(
                    "QFrame#event_card { background: rgba(15,52,96,0.3); "
                    "border: 1px solid rgba(15,52,96,0.6); border-radius: 8px; }");
            } else {
                event_cards[i]->setStyleSheet(
                    "QFrame#event_card { background: rgba(255,255,255,0.03); "
                    "border: 1px solid rgba(255,255,255,0.1); border-radius: 8px; }");
            }
        }

        // Scroll to current event
        if (value >= 0 && value < event_cards.size()) {
            if (auto* scroll = qobject_cast<QScrollArea*>(
                    event_cards[value]->parentWidget()->parentWidget())) {
                scroll->ensureWidgetVisible(event_cards[value], 0, 100);
            }
        }
    });

    // Initialize: highlight first event
    slider->setValue(0);
    emit slider->valueChanged(0);

    // Keyboard shortcuts: Left/H = scrub left, Right/L = scrub right
    auto scrub_left = [slider]() {
        if (slider->value() > slider->minimum()) {
            slider->setValue(slider->value() - 1);
        }
    };

    auto scrub_right = [slider]() {
        if (slider->value() < slider->maximum()) {
            slider->setValue(slider->value() + 1);
        }
    };

    auto* shortcut_left = new QShortcut(QKeySequence(Qt::Key_Left), widget);
    QObject::connect(shortcut_left, &QShortcut::activated, widget, scrub_left);

    auto* shortcut_h = new QShortcut(QKeySequence(Qt::Key_H), widget);
    QObject::connect(shortcut_h, &QShortcut::activated, widget, scrub_left);

    auto* shortcut_right = new QShortcut(QKeySequence(Qt::Key_Right), widget);
    QObject::connect(shortcut_right, &QShortcut::activated, widget, scrub_right);

    auto* shortcut_l = new QShortcut(QKeySequence(Qt::Key_L), widget);
    QObject::connect(shortcut_l, &QShortcut::activated, widget, scrub_right);

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
