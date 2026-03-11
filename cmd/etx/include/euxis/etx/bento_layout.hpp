#pragma once
#include <QWidget>
#include <QGridLayout>
#include <QVector>
#include <QPair>

namespace euxis::etx {

/// Adaptive Bento Box grid layout that redistributes cards based on window width.
///
/// Default breakpoints: <600px → 1 col, <900px → 2 cols, ≥900px → 3 cols.
class BentoLayout : public QWidget {
    Q_OBJECT

public:
    explicit BentoLayout(QWidget* parent = nullptr);

    /// Add a card widget with an optional minimum width hint.
    void add_card(QWidget* widget, int min_width = 300);

    /// Set custom breakpoints as (width_threshold, column_count) pairs.
    /// Must be sorted ascending by width.
    void set_breakpoints(QVector<QPair<int, int>> breakpoints);

    /// Current column count (for testing).
    int column_count() const;

    /// Number of cards added.
    int card_count() const;

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    void relayout();

    struct Card {
        QWidget* widget;
        int min_width;
    };

    QGridLayout* grid_;
    QVector<Card> cards_;
    QVector<QPair<int, int>> breakpoints_;
    int current_cols_{1};
};

} // namespace euxis::etx
