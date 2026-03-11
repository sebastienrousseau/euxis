#pragma once
#include <QWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QPropertyAnimation>

namespace euxis::etx {

/// Collapsible section with animated expand/collapse for progressive disclosure.
///
/// Shows a clickable header with chevron indicator. Content slides open/closed
/// using QPropertyAnimation (200ms, OutCubic easing).
class CollapsibleSection : public QWidget {
    Q_OBJECT
    Q_PROPERTY(int content_height READ content_height WRITE set_content_height)

public:
    /// Construct with a section title.
    explicit CollapsibleSection(const QString& title, QWidget* parent = nullptr);

    /// Set the content widget (takes ownership via layout).
    void set_content(QWidget* content);

    /// Expand or collapse.
    void toggle();

    /// Whether the section is currently expanded.
    bool is_expanded() const;

    /// Current animated content height.
    int content_height() const;
    void set_content_height(int h);

signals:
    void toggled(bool expanded);

private:
    QLabel* header_;
    QWidget* content_container_;
    QPropertyAnimation* animation_;
    bool expanded_{true};
    int natural_height_{0};
    QString title_;

    void update_header_icon();
};

} // namespace euxis::etx
