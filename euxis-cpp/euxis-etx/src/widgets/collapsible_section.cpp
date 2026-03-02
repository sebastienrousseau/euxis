#include <euxis/etx/collapsible_section.hpp>

#include <QEasingCurve>
#include <QFont>

namespace euxis::etx {

CollapsibleSection::CollapsibleSection(const QString& title, QWidget* parent)
    : QWidget(parent)
    , title_(title)
{
    setObjectName("collapsible_section");

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    // Clickable header
    header_ = new QLabel(this);
    header_->setObjectName("collapsible_header");
    header_->setCursor(Qt::PointingHandCursor);
    QFont font;
    font.setPointSize(13);
    font.setBold(true);
    header_->setFont(font);
    header_->setStyleSheet(
        "padding: 8px 12px; color: #e0e0e0; background: rgba(255,255,255,0.03); "
        "border: 1px solid rgba(255,255,255,0.08); border-radius: 6px;");
    update_header_icon();
    outer->addWidget(header_);

    // Content container
    content_container_ = new QWidget(this);
    content_container_->setObjectName("collapsible_content");
    content_container_->setStyleSheet("background: transparent;");
    auto* content_layout = new QVBoxLayout(content_container_);
    content_layout->setContentsMargins(0, 4, 0, 0);
    outer->addWidget(content_container_);

    // Animation
    animation_ = new QPropertyAnimation(this, "content_height", this);
    animation_->setDuration(200);
    animation_->setEasingCurve(QEasingCurve::OutCubic);

    // Header click → toggle
    header_->installEventFilter(this);
}

void CollapsibleSection::set_content(QWidget* content) {
    content_container_->layout()->addWidget(content);
    natural_height_ = content->sizeHint().height() + 4;
}

void CollapsibleSection::toggle() {
    expanded_ = !expanded_;
    update_header_icon();

    if (natural_height_ <= 0) {
        natural_height_ = content_container_->sizeHint().height();
    }

    animation_->stop();
    if (expanded_) {
        animation_->setStartValue(0);
        animation_->setEndValue(natural_height_);
    } else {
        animation_->setStartValue(natural_height_);
        animation_->setEndValue(0);
    }
    animation_->start();

    emit toggled(expanded_);
}

bool CollapsibleSection::is_expanded() const {
    return expanded_;
}

int CollapsibleSection::content_height() const {
    return content_container_->maximumHeight();
}

void CollapsibleSection::set_content_height(int h) {
    content_container_->setMaximumHeight(h);
}

void CollapsibleSection::update_header_icon() {
    QString chevron = expanded_ ? "\xe2\x96\xbe " : "\xe2\x96\xb8 ";  // ▾ or ▸
    header_->setText(chevron + title_);
}

} // namespace euxis::etx
