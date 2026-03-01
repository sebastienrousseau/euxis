#include <euxis/etx/app.hpp>
#include <euxis/etx/registry.hpp>

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
        setMaximumHeight(160);

        auto* layout = new QVBoxLayout(this);
        layout->setSpacing(8);
        layout->setContentsMargins(16, 12, 16, 12);

        // Top row: status dot + name + type badge
        auto* top_row = new QHBoxLayout();
        top_row->setSpacing(8);

        // Status dot
        auto* status_dot = new QLabel(this);
        status_dot->setFixedSize(12, 12);
        QString dot_color = "#888888";
        if (agent.status == "running") dot_color = "#4caf50";
        else if (agent.status == "error") dot_color = "#f44336";
        else if (agent.status == "idle") dot_color = "#2196f3";
        status_dot->setStyleSheet(
            QString("background: %1; border-radius: 6px; border: none;").arg(dot_color));
        top_row->addWidget(status_dot);

        // Name
        auto* name = new QLabel(agent.name, this);
        QFont name_font;
        name_font.setPointSize(13);
        name_font.setBold(true);
        name->setFont(name_font);
        name->setStyleSheet("background: transparent; border: none;");
        top_row->addWidget(name);
        top_row->addStretch();

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

        // Status text at bottom
        auto* status_text = new QLabel(agent.status, this);
        status_text->setStyleSheet(
            QString("color: %1; font-size: 10px; font-weight: bold; "
                    "background: transparent; border: none;").arg(dot_color));
        status_text->setAlignment(Qt::AlignRight);
        layout->addWidget(status_text);
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
