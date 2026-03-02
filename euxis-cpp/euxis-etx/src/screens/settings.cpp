#include <euxis/etx/theme.hpp>
#include <euxis/etx/config.hpp>

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QFont>
#include <QFrame>
#include <QWidget>
#include <QStackedWidget>
#include <QMessageBox>
#include <QCoreApplication>

namespace euxis::etx {

QWidget* create_settings_screen(ThemeEngine* theme, ETXConfig* config,
                                 QWidget* parent) {
    auto* widget = new QWidget(parent);
    auto* layout = new QVBoxLayout(widget);
    layout->setContentsMargins(32, 32, 32, 32);
    layout->setSpacing(16);

    // Back button
    auto* top_bar = new QHBoxLayout();
    auto* back_btn = new QPushButton(QCoreApplication::translate("SettingsScreen", "< Back"), widget);
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
    auto* title = new QLabel(QCoreApplication::translate("SettingsScreen", "Settings"), widget);
    QFont title_font;
    title_font.setPointSize(20);
    title_font.setBold(true);
    title->setFont(title_font);
    layout->addWidget(title);

    // Settings card
    auto* card = new QFrame(widget);
    card->setObjectName("settings_card");
    card->setFrameShape(QFrame::StyledPanel);
    auto* card_layout = new QVBoxLayout(card);
    card_layout->setSpacing(20);
    card_layout->setContentsMargins(24, 24, 24, 24);

    // Theme selector
    auto* theme_row = new QHBoxLayout();
    auto* theme_label = new QLabel(QCoreApplication::translate("SettingsScreen", "Theme:"), card);
    theme_label->setFixedWidth(150);
    theme_label->setStyleSheet("font-weight: bold; font-size: 13px;");
    theme_row->addWidget(theme_label);

    auto* theme_combo = new QComboBox(card);
    theme_combo->setObjectName("theme_combo");
    theme_combo->addItems(theme->available_themes());
    theme_combo->setCurrentText(config->theme());
    theme_combo->setMinimumWidth(200);
    theme_combo->setMinimumHeight(36);
    theme_row->addWidget(theme_combo);
    theme_row->addStretch();
    card_layout->addLayout(theme_row);

    // Refresh interval
    auto* refresh_row = new QHBoxLayout();
    auto* refresh_label = new QLabel(QCoreApplication::translate("SettingsScreen", "Refresh Interval:"), card);
    refresh_label->setFixedWidth(150);
    refresh_label->setStyleSheet("font-weight: bold; font-size: 13px;");
    refresh_row->addWidget(refresh_label);

    auto* refresh_spin = new QSpinBox(card);
    refresh_spin->setObjectName("refresh_spin");
    refresh_spin->setRange(1000, 60000);
    refresh_spin->setSingleStep(1000);
    refresh_spin->setSuffix(" ms");
    refresh_spin->setValue(config->refresh_interval());
    refresh_spin->setMinimumWidth(200);
    refresh_spin->setMinimumHeight(36);
    refresh_row->addWidget(refresh_spin);
    refresh_row->addStretch();
    card_layout->addLayout(refresh_row);

    // Show tips
    auto* tips_row = new QHBoxLayout();
    auto* tips_label = new QLabel(QCoreApplication::translate("SettingsScreen", "Show Tips:"), card);
    tips_label->setFixedWidth(150);
    tips_label->setStyleSheet("font-weight: bold; font-size: 13px;");
    tips_row->addWidget(tips_label);

    auto* tips_check = new QCheckBox(QCoreApplication::translate("SettingsScreen", "Display helpful tips in the dashboard"), card);
    tips_check->setObjectName("tips_check");
    tips_check->setChecked(config->show_tips());
    tips_check->setMinimumHeight(36);
    tips_row->addWidget(tips_check);
    tips_row->addStretch();
    card_layout->addLayout(tips_row);

    layout->addWidget(card);

    // Save button
    auto* button_row = new QHBoxLayout();
    button_row->addStretch();

    auto* save_btn = new QPushButton(QCoreApplication::translate("SettingsScreen", "Save Settings"), widget);
    save_btn->setObjectName("save_button");
    save_btn->setCursor(Qt::PointingHandCursor);
    save_btn->setMinimumHeight(44);
    save_btn->setMinimumWidth(160);
    save_btn->setStyleSheet(
        "QPushButton { background: #0f3460; color: #fff; border: none; "
        "border-radius: 6px; font-weight: bold; font-size: 14px; padding: 10px 32px; }"
        "QPushButton:hover { background: #1a4a7a; }");
    button_row->addWidget(save_btn);
    layout->addLayout(button_row);

    QObject::connect(save_btn, &QPushButton::clicked, widget,
                     [theme, config, theme_combo, refresh_spin, tips_check, widget]() {
        config->set_theme(theme_combo->currentText());
        config->set_refresh_interval(refresh_spin->value());
        config->set_show_tips(tips_check->isChecked());
        theme->apply_theme(theme_combo->currentText());

        QMessageBox::information(widget,
            QCoreApplication::translate("SettingsScreen", "Settings Saved"),
            QCoreApplication::translate("SettingsScreen", "Your settings have been saved successfully."));
    });

    layout->addStretch();

    return widget;
}

} // namespace euxis::etx
