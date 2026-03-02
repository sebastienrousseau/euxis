#include <euxis/etx/app.hpp>
#include <euxis/etx/registry.hpp>
#include <euxis/etx/accessibility.hpp>
#include <euxis/etx/semantic_colors.hpp>

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QFont>
#include <QWidget>
#include <QMouseEvent>

namespace euxis::etx {

class AgentCardWidget : public QFrame {
    Q_OBJECT

public:
    explicit AgentCardWidget(const AgentInfo& agent, EuxisApp* app,
                             QWidget* parent = nullptr)
        : QFrame(parent)
        , agent_id_(agent.id)
        , app_(app)
    {
        setObjectName("agent_card");
        setProperty("agent_id", agent.id);
        setFrameShape(QFrame::StyledPanel);
        setCursor(Qt::PointingHandCursor);
        setMinimumHeight(120);
        setMaximumHeight(180);

        // Accessibility: keyboard-navigable card
        a11y::interactive(this, agent.name + " - " + agent.status,
                          QAccessible::ListItem);

        auto* layout = new QVBoxLayout(this);
        layout->setSpacing(6);
        layout->setContentsMargins(16, 12, 16, 12);

        // Top row: status dot + name + tier badge + type badge
        auto* top_row = new QHBoxLayout();
        top_row->setSpacing(6);

        // Status indicator with semantic severity color
        QString dot_color = severity_color(status_to_severity(agent.status)).name();
        auto* status_indicator = a11y::create_status_indicator(
            agent.status, dot_color, this);
        top_row->addWidget(status_indicator);

        // Name
        auto* name = new QLabel(agent.name, this);
        QFont name_font;
        name_font.setPointSize(13);
        name_font.setBold(true);
        name->setFont(name_font);
        name->setStyleSheet("background: transparent; border: none;");
        top_row->addWidget(name);
        top_row->addStretch();

        // Tier badge (CORE/FLEET)
        if (!agent.tier.isEmpty()) {
            auto* tier_badge = new QLabel(agent.tier.toUpper(), this);
            QFont tier_font;
            tier_font.setPointSize(8);
            tier_font.setBold(true);
            tier_badge->setFont(tier_font);
            QString tier_bg = agent.tier == "core"
                ? "rgba(15,52,96,0.5)"
                : "rgba(255,255,255,0.06)";
            QString tier_fg = agent.tier == "core" ? "#6db3f2" : "#888";
            tier_badge->setStyleSheet(
                QString("background: %1; color: %2; padding: 2px 6px; "
                        "border-radius: 3px; border: none;").arg(tier_bg, tier_fg));
            top_row->addWidget(tier_badge);
        }

        // Type badge
        auto* badge = new QLabel(agent.type.toUpper(), this);
        badge->setObjectName("type_badge");
        QFont badge_font;
        badge_font.setPointSize(9);
        badge->setFont(badge_font);
        QString badge_bg = "rgba(255,255,255,0.1)";
        if (agent.type == "squad") badge_bg = "rgba(76,175,80,0.2)";
        else if (agent.type == "combo") badge_bg = "rgba(255,171,0,0.2)";
        badge->setStyleSheet(
            QString("background: %1; color: #aaa; padding: 2px 8px; "
                    "border-radius: 4px; border: none;").arg(badge_bg));
        top_row->addWidget(badge);

        layout->addLayout(top_row);

        // Description
        auto* desc = new QLabel(agent.description, this);
        desc->setWordWrap(true);
        desc->setStyleSheet("color: #999; border: none; background: transparent;");
        QFont desc_font;
        desc_font.setPointSize(11);
        desc->setFont(desc_font);
        layout->addWidget(desc, 1);

        // Bottom row: activation + status
        auto* bottom_row = new QHBoxLayout();

        if (!agent.activation.isEmpty()) {
            auto* act_label = new QLabel(agent.activation, this);
            act_label->setStyleSheet(
                "color: #666; font-size: 9px; background: transparent; border: none;");
            bottom_row->addWidget(act_label);
        }

        bottom_row->addStretch();

        auto* status_text = new QLabel(agent.status, this);
        status_text->setStyleSheet(
            QString("color: %1; font-size: 10px; font-weight: bold; "
                    "background: transparent; border: none;").arg(dot_color));
        status_text->setAlignment(Qt::AlignRight);
        bottom_row->addWidget(status_text);

        layout->addLayout(bottom_row);
    }

protected:
    void mousePressEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton && app_) {
            app_->show_agent(agent_id_.toStdString());
        }
        QFrame::mousePressEvent(event);
    }

    void enterEvent(QEnterEvent* event) override {
        setStyleSheet(styleSheet()); // Trigger repaint
        QFrame::enterEvent(event);
    }

private:
    QString agent_id_;
    EuxisApp* app_;
};

QWidget* create_agent_card_widget(const AgentInfo& agent, EuxisApp* app,
                                   QWidget* parent) {
    return new AgentCardWidget(agent, app, parent);
}

} // namespace euxis::etx

#include "agent_card.moc"
