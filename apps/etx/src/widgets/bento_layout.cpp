#include <euxis/etx/bento_layout.hpp>

#include <QResizeEvent>

namespace euxis::etx {

BentoLayout::BentoLayout(QWidget* parent)
    : QWidget(parent)
    , grid_(new QGridLayout(this))
{
    setObjectName("bento_layout");
    grid_->setContentsMargins(0, 0, 0, 0);
    grid_->setSpacing(12);

    // Default breakpoints: width → column count
    breakpoints_ = {
        {600, 1},
        {900, 2},
        {1200, 3},
    };
}

void BentoLayout::add_card(QWidget* widget, int min_width) {
    cards_.append({widget, min_width});
    relayout();
}

void BentoLayout::set_breakpoints(QVector<QPair<int, int>> breakpoints) {
    breakpoints_ = std::move(breakpoints);
    relayout();
}

int BentoLayout::column_count() const {
    return current_cols_;
}

int BentoLayout::card_count() const {
    return static_cast<int>(cards_.size());
}

void BentoLayout::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    relayout();
}

void BentoLayout::relayout() {
    // Determine column count from breakpoints
    int cols = 1;
    int w = width();
    for (const auto& [threshold, count] : breakpoints_) {
        if (w >= threshold) {
            cols = count;
        }
    }
    current_cols_ = cols;

    // Remove all items from grid (without deleting widgets)
    while (grid_->count() > 0) {
        auto* item = grid_->takeAt(0);
        delete item;
    }

    // Re-add cards in row-major order
    for (int i = 0; i < cards_.size(); ++i) {
        int row = i / cols;
        int col = i % cols;
        grid_->addWidget(cards_[i].widget, row, col);
    }
}

} // namespace euxis::etx
