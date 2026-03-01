#include <euxis/etx/app.hpp>
#include <euxis/etx/registry.hpp>
#include <euxis/etx/theme.hpp>
#include <euxis/etx/config.hpp>

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QGridLayout>
#include <QFrame>
#include <QTimer>
#include <QPushButton>

// Forward declarations from widget files
namespace euxis::etx {
    QWidget* create_header_widget(ThemeEngine* theme, QWidget* parent);
    QWidget* create_search_bar_widget(QWidget* parent);
    QWidget* create_fleet_grid_widget(FleetRegistry* registry, EuxisApp* app,
                                      QWidget* parent);
    QWidget* create_tip_bar_widget(ETXConfig* config, QWidget* parent);
    QWidget* create_shortcut_bar_widget(QWidget* parent);
}

namespace euxis::etx {

class DashboardScreen : public QWidget {
    Q_OBJECT

public:
    explicit DashboardScreen(EuxisApp* app, FleetRegistry* registry,
                             ThemeEngine* theme, ETXConfig* config,
                             QWidget* parent = nullptr)
        : QWidget(parent)
    {
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);

        // Header
        auto* header = create_header_widget(theme, this);
        layout->addWidget(header);

        // Content area
        auto* content_layout = new QVBoxLayout();
        content_layout->setContentsMargins(16, 8, 16, 8);
        content_layout->setSpacing(8);

        // Search bar
        auto* search = create_search_bar_widget(this);
        content_layout->addWidget(search);

        // Fleet grid (main area)
        auto* fleet_grid = create_fleet_grid_widget(registry, app, this);
        fleet_grid->setObjectName("fleet_grid");
        content_layout->addWidget(fleet_grid, 1);

        layout->addLayout(content_layout, 1);

        // Tip bar
        auto* tip_bar = create_tip_bar_widget(config, this);
        layout->addWidget(tip_bar);

        // Shortcut bar
        auto* shortcut_bar = create_shortcut_bar_widget(this);
        layout->addWidget(shortcut_bar);

        // Connect search to fleet grid filtering
        auto* search_input = search->findChild<QLineEdit*>("search_input");
        if (search_input) {
            connect(search_input, &QLineEdit::textChanged,
                    this, [fleet_grid, registry](const QString& text) {
                // Trigger re-filter on the fleet grid
                auto* grid_layout = fleet_grid->findChild<QGridLayout*>("grid_layout");
                if (!grid_layout) return;

                auto filtered = registry->filter(text);

                // Hide/show cards based on filter
                for (int i = 0; i < grid_layout->count(); ++i) {
                    auto* item = grid_layout->itemAt(i);
                    if (!item || !item->widget()) continue;
                    auto* card = item->widget();
                    QString card_id = card->property("agent_id").toString();

                    bool visible = false;
                    for (const auto& a : filtered) {
                        if (a.id == card_id) {
                            visible = true;
                            break;
                        }
                    }
                    card->setVisible(visible);
                }
            });
        }

        // Refresh when registry updates
        connect(registry, &FleetRegistry::refreshed, this, [fleet_grid, registry, app]() {
            // Rebuild grid on refresh
            auto* grid_layout = fleet_grid->findChild<QGridLayout*>("grid_layout");
            if (!grid_layout) return;

            // Clear existing items
            QLayoutItem* child;
            while ((child = grid_layout->takeAt(0)) != nullptr) {
                if (child->widget()) {
                    child->widget()->deleteLater();
                }
                delete child;
            }

            // Rebuild
            const auto& agents = registry->agents();
            int col = 0, row = 0;
            const int cols = 3;
            for (const auto& agent : agents) {
                // Inline card creation for rebuild
                auto* card = new QFrame(fleet_grid);
                card->setObjectName("agent_card");
                card->setProperty("agent_id", agent.id);
                card->setFrameShape(QFrame::StyledPanel);
                card->setCursor(Qt::PointingHandCursor);

                auto* card_layout = new QVBoxLayout(card);
                card_layout->setSpacing(6);

                // Status + Name row
                auto* top_row = new QHBoxLayout();
                auto* status_dot = new QLabel(card);
                status_dot->setFixedSize(12, 12);
                QString dot_color = "#888888";
                if (agent.status == "running") dot_color = "#4caf50";
                else if (agent.status == "error") dot_color = "#f44336";
                else if (agent.status == "idle") dot_color = "#2196f3";
                status_dot->setStyleSheet(
                    QString("background: %1; border-radius: 6px; border: none;")
                    .arg(dot_color));
                top_row->addWidget(status_dot);

                auto* name = new QLabel(agent.name, card);
                QFont name_font;
                name_font.setPointSize(13);
                name_font.setBold(true);
                name->setFont(name_font);
                top_row->addWidget(name);
                top_row->addStretch();

                // Type badge
                auto* badge = new QLabel(agent.type.toUpper(), card);
                badge->setObjectName("type_badge");
                QFont badge_font;
                badge_font.setPointSize(9);
                badge->setFont(badge_font);
                badge->setStyleSheet(
                    "background: rgba(255,255,255,0.1); color: #aaa; "
                    "padding: 2px 8px; border-radius: 4px; border: none;");
                top_row->addWidget(badge);

                card_layout->addLayout(top_row);

                auto* desc = new QLabel(agent.description, card);
                desc->setWordWrap(true);
                desc->setStyleSheet("color: #999; border: none; background: transparent;");
                card_layout->addWidget(desc);

                grid_layout->addWidget(card, row, col);
                col++;
                if (col >= cols) { col = 0; row++; }
            }
        });
    }
};

QWidget* create_dashboard_screen(EuxisApp* app, FleetRegistry* registry,
                                  ThemeEngine* theme, ETXConfig* config,
                                  QWidget* parent) {
    return new DashboardScreen(app, registry, theme, config, parent);
}

} // namespace euxis::etx

#include "dashboard.moc"
