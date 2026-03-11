#include <euxis/etx/app.hpp>
#include <euxis/etx/registry.hpp>

#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QFont>
#include <QWidget>
#include <QScrollArea>

// Forward declaration
namespace euxis::etx {
    QWidget* create_agent_card_widget(const AgentInfo& agent, EuxisApp* app,
                                      QWidget* parent);
}

namespace euxis::etx {

QWidget* create_fleet_grid_widget(FleetRegistry* registry, EuxisApp* app,
                                   QWidget* parent) {
    auto* scroll = new QScrollArea(parent);
    scroll->setObjectName("fleet_grid");
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setStyleSheet(
        "QScrollArea { background: transparent; border: none; }"
        "QScrollBar:vertical { background: rgba(255,255,255,0.02); width: 8px; }"
        "QScrollBar::handle:vertical { background: rgba(255,255,255,0.1); "
        "border-radius: 4px; min-height: 30px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }");

    auto* container = new QWidget(scroll);
    auto* grid = new QGridLayout(container);
    grid->setObjectName("grid_layout");
    grid->setSpacing(12);
    grid->setContentsMargins(0, 0, 0, 0);

    // Populate with agent cards
    const auto& agents = registry->agents();
    int col = 0, row = 0;
    const int cols = 3;

    for (const auto& agent : agents) {
        auto* card = create_agent_card_widget(agent, app, container);
        grid->addWidget(card, row, col);
        col++;
        if (col >= cols) { col = 0; row++; }
    }

    // Fill remaining columns in last row with spacers
    while (col < cols && col > 0) {
        grid->addWidget(new QWidget(container), row, col);
        col++;
    }

    // Add vertical stretch at the bottom
    grid->setRowStretch(row + 1, 1);

    scroll->setWidget(container);
    return scroll;
}

} // namespace euxis::etx
