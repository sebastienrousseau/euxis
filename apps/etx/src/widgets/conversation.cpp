#include <euxis/etx/conversation_widget.hpp>
#include <euxis/etx/chat_engine.hpp>

#include <QHBoxLayout>
#include <QFrame>
#include <QFont>
#include <QScrollBar>
#include <QPropertyAnimation>
#include <QEasingCurve>
#include <QCoreApplication>

namespace euxis::etx {

// --- ThinkingIndicator ---

ThinkingIndicator::ThinkingIndicator(QWidget* parent)
    : QLabel(parent)
{
    setObjectName("thinking_indicator");
    setStyleSheet(
        "color: #888; font-style: italic; padding: 8px 16px; "
        "background: transparent; border: none;");
    timer_ = new QTimer(this);
    connect(timer_, &QTimer::timeout, this, &ThinkingIndicator::animate);
}

void ThinkingIndicator::start(const QString& agent_id) {
    agent_id_ = agent_id;
    dot_count_ = 1;
    setText(tr("%1 is thinking.").arg(agent_id_));
    show();
    timer_->start(400);
}

void ThinkingIndicator::stop() {
    timer_->stop();
    hide();
}

void ThinkingIndicator::animate() {
    dot_count_ = (dot_count_ % 3) + 1;
    setText(tr("%1 is thinking%2").arg(agent_id_, QString(".").repeated(dot_count_)));
}

// --- MessageBubble (file-local) ---

static QWidget* make_bubble(ChatMessage::Role role, const QString& text,
                             const QString& agent_id, const QString& model,
                             double duration_ms, QWidget* parent) {
    auto* frame = new QFrame(parent);
    frame->setFrameShape(QFrame::NoFrame);

    auto* outer = new QVBoxLayout(frame);
    outer->setContentsMargins(0, 4, 0, 4);
    outer->setSpacing(2);

    auto* align = new QHBoxLayout();
    align->setContentsMargins(0, 0, 0, 0);

    auto* bubble = new QFrame(frame);
    bubble->setObjectName("message_bubble");
    auto* blayout = new QVBoxLayout(bubble);
    blayout->setContentsMargins(12, 8, 12, 8);
    blayout->setSpacing(4);

    if (role == ChatMessage::User) {
        align->addStretch();
        bubble->setStyleSheet(
            "QFrame#message_bubble { background: rgba(15,52,96,0.6); "
            "border-radius: 12px; border: none; "
            "border-bottom-right-radius: 4px; }");
        bubble->setMaximumWidth(600);

        auto* label = new QLabel(text, bubble);
        label->setWordWrap(true);
        label->setTextInteractionFlags(Qt::TextSelectableByMouse);
        label->setStyleSheet(
            "color: #e8e8e8; background: transparent; border: none; "
            "font-size: 13px;");
        blayout->addWidget(label);
        align->addWidget(bubble);

    } else if (role == ChatMessage::Assistant) {
        bubble->setStyleSheet(
            "QFrame#message_bubble { background: rgba(255,255,255,0.04); "
            "border-radius: 12px; border: none; "
            "border-bottom-left-radius: 4px; }");
        bubble->setMaximumWidth(700);

        if (!agent_id.isEmpty()) {
            auto* hdr = new QLabel(agent_id, bubble);
            QFont hf;
            hf.setPointSize(10);
            hf.setBold(true);
            hdr->setFont(hf);
            hdr->setStyleSheet("color: #6db3f2; background: transparent; border: none;");
            blayout->addWidget(hdr);
        }

        auto* label = new QLabel(text, bubble);
        label->setWordWrap(true);
        label->setTextInteractionFlags(Qt::TextSelectableByMouse);
        QFont mf;
        mf.setFamily("Monospace");
        mf.setStyleHint(QFont::Monospace);
        mf.setPointSize(12);
        label->setFont(mf);
        label->setStyleSheet("color: #d0d0d0; background: transparent; border: none;");
        blayout->addWidget(label);

        if (!model.isEmpty()) {
            auto* meta = new QHBoxLayout();
            auto* badge = new QLabel(model, bubble);
            badge->setStyleSheet(
                "color: #666; background: rgba(255,255,255,0.05); "
                "border: 1px solid rgba(255,255,255,0.08); "
                "border-radius: 3px; padding: 1px 6px; font-size: 9px;");
            meta->addWidget(badge);
            if (duration_ms > 0) {
                auto* dur = new QLabel(
                    QCoreApplication::translate("ConversationWidget", "%1s")
                        .arg(QString::number(duration_ms / 1000.0, 'f', 1)), bubble);
                dur->setStyleSheet(
                    "color: #555; font-size: 9px; background: transparent; border: none;");
                meta->addWidget(dur);
            }
            meta->addStretch();
            blayout->addLayout(meta);
        }
        align->addWidget(bubble);
        align->addStretch();

    } else {
        // System
        align->addStretch();
        bubble->setStyleSheet("QFrame#message_bubble { background: transparent; border: none; }");

        auto* label = new QLabel(text, bubble);
        label->setWordWrap(true);
        label->setAlignment(Qt::AlignCenter);
        QFont sf;
        sf.setItalic(true);
        sf.setPointSize(11);
        label->setFont(sf);
        label->setStyleSheet("color: #888; background: transparent; border: none;");
        blayout->addWidget(label);
        align->addWidget(bubble);
        align->addStretch();
    }

    outer->addLayout(align);
    return frame;
}

// --- ConversationWidget ---

ConversationWidget::ConversationWidget(QWidget* parent)
    : QScrollArea(parent)
{
    setObjectName("conversation_widget");
    setWidgetResizable(true);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setMinimumWidth(400);
    setStyleSheet(
        "QScrollArea { background: rgba(0,0,0,0.15); border: none; "
        "border-right: 1px solid rgba(255,255,255,0.06); }"
        "QScrollBar:vertical { background: rgba(255,255,255,0.02); width: 8px; }"
        "QScrollBar::handle:vertical { background: rgba(255,255,255,0.1); "
        "border-radius: 4px; min-height: 30px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }");

    container_ = new QWidget(this);
    container_->setObjectName("conversation_container");
    container_->setStyleSheet("background: transparent; border: none;");
    layout_ = new QVBoxLayout(container_);
    layout_->setContentsMargins(16, 16, 16, 8);
    layout_->setSpacing(4);
    layout_->addStretch();

    thinking_ = new ThinkingIndicator(container_);
    thinking_->hide();
    layout_->addWidget(thinking_);

    setWidget(container_);
}

void ConversationWidget::add_user_message(const QString& text) {
    insert_bubble(make_bubble(ChatMessage::User, text, {}, {}, 0, container_));
}

void ConversationWidget::add_assistant_message(const QString& agent_id,
                                                const QString& text,
                                                const QString& model,
                                                double duration_ms) {
    insert_bubble(make_bubble(ChatMessage::Assistant, text, agent_id,
                               model, duration_ms, container_));
}

void ConversationWidget::add_system_message(const QString& text) {
    insert_bubble(make_bubble(ChatMessage::System, text, {}, {}, 0, container_));
}

void ConversationWidget::show_thinking(const QString& agent_id) {
    thinking_->start(agent_id);
    scroll_to_bottom();
}

void ConversationWidget::hide_thinking() {
    thinking_->stop();
}

void ConversationWidget::clear_conversation() {
    while (layout_->count() > 2) {
        auto* item = layout_->takeAt(0);
        if (item->widget() && item->widget() != thinking_) {
            item->widget()->deleteLater();
        }
        delete item;
    }
}

void ConversationWidget::insert_bubble(QWidget* bubble) {
    int pos = layout_->count() - 1;
    if (pos < 1) pos = 1;
    layout_->insertWidget(pos, bubble);
    scroll_to_bottom();
}

void ConversationWidget::scroll_to_bottom() {
    QTimer::singleShot(10, this, [this]() {
        auto* bar = verticalScrollBar();
        int target = bar->maximum();
        if (target <= bar->value()) { return; }

        auto* anim = new QPropertyAnimation(bar, "value", this);
        anim->setDuration(200);
        anim->setEasingCurve(QEasingCurve::OutCubic);
        anim->setStartValue(bar->value());
        anim->setEndValue(target);
        connect(anim, &QPropertyAnimation::finished, anim, &QObject::deleteLater);
        anim->start();
    });
}

} // namespace euxis::etx
