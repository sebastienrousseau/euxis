#include <euxis/etx/registry.hpp>

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QFont>
#include <QFrame>
#include <QWidget>
#include <QStackedWidget>
#include <QShortcut>
#include <QCoreApplication>
#include <functional>

namespace euxis::etx {

// Deterministic color from string hash
static QString color_for(const QString& name) {
    static const char* palette[] = {
        "#4caf50", "#2196f3", "#f44336", "#ff9800", "#9c27b0",
        "#00bcd4", "#e91e63", "#8bc34a", "#ff5722", "#607d8b",
        "#3f51b5", "#795548", "#009688", "#cddc39", "#ffc107"
    };
    uint hash = qHash(name);
    return palette[hash % 15];
}

QWidget* create_squad_detail_screen(FleetRegistry* registry, QWidget* parent) {
    auto* widget = new QWidget(parent);
    auto* layout = new QVBoxLayout(widget);
    layout->setContentsMargins(32, 32, 32, 32);
    layout->setSpacing(16);

    // Back button
    auto* top_bar = new QHBoxLayout();
    auto* back_btn = new QPushButton(QCoreApplication::translate("SquadDetailScreen", "< Back"), widget);
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
    auto* title = new QLabel(QCoreApplication::translate("SquadDetailScreen", "Squads & Combos"), widget);
    QFont title_font;
    title_font.setPointSize(20);
    title_font.setBold(true);
    title->setFont(title_font);
    layout->addWidget(title);

    auto* subtitle = new QLabel(QCoreApplication::translate("SquadDetailScreen", "Squad composition and combo chain viewer"), widget);
    subtitle->setStyleSheet("color: #888; font-size: 12px;");
    layout->addWidget(subtitle);

    // Scroll area for cards
    auto* scroll = new QScrollArea(widget);
    scroll->setObjectName("squad_scroll");
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setStyleSheet(
        "QScrollArea { background: transparent; border: none; }"
        "QScrollArea > QWidget > QWidget { background: transparent; }");

    auto* scroll_content = new QWidget(scroll);
    auto* content_layout = new QVBoxLayout(scroll_content);
    content_layout->setSpacing(16);

    QFont section_font;
    section_font.setPointSize(16);
    section_font.setBold(true);

    // Build function to populate squads and combos
    auto build_content = [content_layout, scroll_content, section_font, registry]() {
        // Clear existing content
        QLayoutItem* child;
        while ((child = content_layout->takeAt(0)) != nullptr) {
            if (child->widget()) child->widget()->deleteLater();
            if (child->layout()) {
                QLayoutItem* inner;
                while ((inner = child->layout()->takeAt(0)) != nullptr) {
                    if (inner->widget()) inner->widget()->deleteLater();
                    delete inner;
                }
            }
            delete child;
        }

        // --- Squads ---
        auto* squads_label = new QLabel(QCoreApplication::translate("SquadDetailScreen", "Squads"), scroll_content);
        squads_label->setFont(section_font);
        content_layout->addWidget(squads_label);

        const auto& squads = registry->squads();
        for (const auto& squad : squads) {
            auto* card = new QFrame(scroll_content);
            card->setObjectName("squad_card");
            card->setFrameShape(QFrame::StyledPanel);
            card->setStyleSheet(
                "QFrame#squad_card { background: rgba(255,255,255,0.03); "
                "border: 1px solid rgba(255,255,255,0.1); border-radius: 8px; "
                "padding: 4px; }");
            auto* card_layout = new QVBoxLayout(card);
            card_layout->setContentsMargins(20, 16, 20, 16);
            card_layout->setSpacing(10);

            // Name
            auto* name = new QLabel(squad.name, card);
            QFont name_font;
            name_font.setPointSize(15);
            name_font.setBold(true);
            name->setFont(name_font);
            card_layout->addWidget(name);

            // Purpose
            auto* purpose = new QLabel(squad.purpose, card);
            purpose->setWordWrap(true);
            purpose->setStyleSheet("color: #aaa; font-size: 12px;");
            card_layout->addWidget(purpose);

            // Lead badge
            auto* lead_row = new QHBoxLayout();
            auto* lead_label = new QLabel(QCoreApplication::translate("SquadDetailScreen", "Lead:"), card);
            lead_label->setStyleSheet("color: #888; font-weight: bold;");
            lead_row->addWidget(lead_label);

            auto* lead_badge = new QLabel(squad.lead, card);
            lead_badge->setStyleSheet(
                "background: rgba(15,52,96,0.6); color: #fff; "
                "padding: 4px 12px; border-radius: 4px; font-weight: bold; "
                "font-size: 12px;");
            lead_row->addWidget(lead_badge);
            lead_row->addStretch();
            card_layout->addLayout(lead_row);

            // Members
            auto* members_row = new QHBoxLayout();
            auto* members_label = new QLabel(QCoreApplication::translate("SquadDetailScreen", "Members:"), card);
            members_label->setStyleSheet("color: #888; font-weight: bold;");
            members_row->addWidget(members_label);

            for (const auto& member : squad.members) {
                auto* member_label = new QLabel(member, card);
                member_label->setStyleSheet(
                    QString("background: %1; color: #fff; "
                            "padding: 3px 10px; border-radius: 4px; "
                            "font-size: 11px; font-weight: bold;")
                    .arg(color_for(member)));
                members_row->addWidget(member_label);
            }
            members_row->addStretch();
            card_layout->addLayout(members_row);

            content_layout->addWidget(card);
        }

        // Separator
        auto* separator = new QFrame(scroll_content);
        separator->setFrameShape(QFrame::HLine);
        separator->setStyleSheet("color: rgba(255,255,255,0.1);");
        content_layout->addWidget(separator);

        // --- Combos ---
        auto* combos_label = new QLabel(QCoreApplication::translate("SquadDetailScreen", "Combos"), scroll_content);
        combos_label->setFont(section_font);
        content_layout->addWidget(combos_label);

        const auto& combos = registry->combos();
        for (const auto& combo : combos) {
            auto* card = new QFrame(scroll_content);
            card->setObjectName("combo_card");
            card->setFrameShape(QFrame::StyledPanel);
            card->setStyleSheet(
                "QFrame#combo_card { background: rgba(255,255,255,0.03); "
                "border: 1px solid rgba(255,255,255,0.1); border-radius: 8px; "
                "padding: 4px; }");
            auto* card_layout = new QVBoxLayout(card);
            card_layout->setContentsMargins(20, 16, 20, 16);
            card_layout->setSpacing(10);

            // Name
            auto* name = new QLabel(combo.name, card);
            QFont name_font;
            name_font.setPointSize(15);
            name_font.setBold(true);
            name->setFont(name_font);
            card_layout->addWidget(name);

            // Description
            auto* desc = new QLabel(combo.description, card);
            desc->setWordWrap(true);
            desc->setStyleSheet("color: #aaa; font-size: 12px;");
            card_layout->addWidget(desc);

            // Chain display
            auto* chain_row = new QHBoxLayout();
            auto* chain_label = new QLabel(QCoreApplication::translate("SquadDetailScreen", "Chain:"), card);
            chain_label->setStyleSheet("color: #888; font-weight: bold;");
            chain_row->addWidget(chain_label);

            for (int i = 0; i < combo.chain.size(); ++i) {
                auto* agent_label = new QLabel(combo.chain[i], card);
                agent_label->setStyleSheet(
                    QString("background: %1; color: #fff; "
                            "padding: 3px 10px; border-radius: 4px; "
                            "font-size: 11px; font-weight: bold;")
                    .arg(color_for(combo.chain[i])));
                chain_row->addWidget(agent_label);

                if (i < combo.chain.size() - 1) {
                    auto* arrow = new QLabel("->", card);
                    arrow->setStyleSheet("color: #666; font-weight: bold; font-size: 13px;");
                    chain_row->addWidget(arrow);
                }
            }
            chain_row->addStretch();
            card_layout->addLayout(chain_row);

            content_layout->addWidget(card);
        }

        content_layout->addStretch();
    };

    // Initial build
    build_content();

    // Rebuild on registry refresh
    QObject::connect(registry, &FleetRegistry::refreshed, widget, build_content);

    scroll->setWidget(scroll_content);
    layout->addWidget(scroll, 1);

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
