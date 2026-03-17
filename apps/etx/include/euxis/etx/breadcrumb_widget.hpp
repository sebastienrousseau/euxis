#pragma once
#include <QWidget>
#include <QHBoxLayout>
#include <QLabel>
#include <QString>
#include <QVector>

namespace euxis::etx {

/// Clickable breadcrumb navigation trail.
///
/// Shows the navigation path as "Home > Screen1 > Screen2" with clickable
/// past crumbs and bold current crumb.
class BreadcrumbWidget : public QWidget {
    Q_OBJECT

public:
    explicit BreadcrumbWidget(QWidget* parent = nullptr);

    /// Navigate to a screen — appends to trail if new, or truncates if revisiting.
    void navigate_to(int screen_index, const QString& screen_name);

    /// Reset trail to a single root entry.
    void reset(int screen_index, const QString& screen_name);

    /// Get number of crumbs in the trail.
    int count() const;

signals:
    /// Emitted when the user clicks a past crumb.
    void crumb_clicked(int screen_index);

private:
    void rebuild();

    struct Crumb {
        int screen_index;
        QString name;
    };

    QVector<Crumb> trail_;
    QHBoxLayout* crumb_layout_;
};

} // namespace euxis::etx
