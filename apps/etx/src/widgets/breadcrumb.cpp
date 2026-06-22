#include <euxis/etx/breadcrumb_widget.hpp>

#include <QFont>

namespace euxis::etx {

BreadcrumbWidget::BreadcrumbWidget(QWidget* parent)
    : QWidget(parent)
    , crumb_layout_(new QHBoxLayout(this))
{
    setObjectName("breadcrumb_widget");
    crumb_layout_->setContentsMargins(0, 0, 0, 0);
    crumb_layout_->setSpacing(4);
    crumb_layout_->addStretch();
    setFixedHeight(24);
    setStyleSheet("background: transparent;");
}

void BreadcrumbWidget::navigate_to(int screen_index, const QString& screen_name) {
    // Check if we're revisiting an existing crumb — truncate trail
    for (int i = 0; i < trail_.size(); ++i) {
        if (trail_[i].screen_index == screen_index) {
            trail_.resize(i + 1);
            rebuild();
            return;
        }
    }
    // Append new crumb
    trail_.append({screen_index, screen_name});
    rebuild();
}

void BreadcrumbWidget::reset(int screen_index, const QString& screen_name) {
    trail_.clear();
    trail_.append({screen_index, screen_name});
    rebuild();
}

int BreadcrumbWidget::count() const {
    return static_cast<int>(trail_.size());
}

void BreadcrumbWidget::rebuild() {
    // Remove all widgets from layout (except the trailing stretch)
    while (crumb_layout_->count() > 0) {
        auto* item = crumb_layout_->takeAt(0);
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }

    for (int i = 0; i < trail_.size(); ++i) {
        bool is_last = (i == trail_.size() - 1);

        auto* label = new QLabel(trail_[i].name, this);
        label->setObjectName("breadcrumb_item");

        if (is_last) {
            // Current screen — bold, non-clickable
            QFont font;
            font.setBold(true);
            font.setPointSize(10);
            label->setFont(font);
            label->setStyleSheet(
                "color: #e0e0e0; background: transparent; border: none; "
                "padding: 0 2px;");
        } else {
            // Past crumb — clickable
            QFont font;
            font.setPointSize(10);
            label->setFont(font);
            label->setStyleSheet(
                "color: #888; background: transparent; border: none; "
                "padding: 0 2px;");
            label->setCursor(Qt::PointingHandCursor);

            int idx = trail_[i].screen_index;
            connect(label, &QLabel::linkActivated, this, [this, idx](const QString&) {
                emit crumb_clicked(idx);
            });
            // Use mouse press event since QLabel doesn't emit linkActivated without links
            label->installEventFilter(this);
            label->setProperty("screen_index", idx);
        }

        crumb_layout_->addWidget(label);

        // Add separator (except after last)
        if (!is_last) {
            auto* sep = new QLabel("\xe2\x80\xba", this);  // › character
            sep->setStyleSheet(
                "color: #555; background: transparent; border: none; "
                "padding: 0 2px; font-size: 10px;");
            crumb_layout_->addWidget(sep);
        }
    }

    crumb_layout_->addStretch();
}

} // namespace euxis::etx
