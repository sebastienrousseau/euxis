#include <euxis/etx/theme.hpp>
#include <euxis/etx/breadcrumb_widget.hpp>

#include <QHBoxLayout>
#include <QLabel>
#include <QFont>
#include <QWidget>
#include <QPainter>
#include <QPainterPath>
#include <QTimer>
#include <QRandomGenerator>
#include <QCoreApplication>

namespace euxis::etx {

class SparklineWidget : public QWidget {
    Q_OBJECT

public:
    explicit SparklineWidget(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setFixedSize(120, 32);
        setToolTip(tr("Burn rate (simulated)"));

        // Initialize data points
        for (int i = 0; i < 20; ++i) {
            data_points_.append(QRandomGenerator::global()->bounded(10, 90));
        }

        // Update timer for animated sparkline
        auto* timer = new QTimer(this);
        connect(timer, &QTimer::timeout, this, [this]() {
            data_points_.removeFirst();
            data_points_.append(QRandomGenerator::global()->bounded(10, 90));
            update();
        });
        timer->start(2000);
    }

protected:
    void paintEvent(QPaintEvent* /*event*/) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        if (data_points_.size() < 2) return;

        // Draw sparkline
        QPen pen(QColor(79, 195, 247), 1.5);
        painter.setPen(pen);

        QVector<QPointF> points;
        double step_x = static_cast<double>(width()) / static_cast<double>(data_points_.size() - 1);
        for (int i = 0; i < data_points_.size(); ++i) {
            double x = i * step_x;
            double y = height() - (data_points_[i] / 100.0 * height());
            points.append(QPointF(x, y));
        }

        for (int i = 0; i < points.size() - 1; ++i) {
            painter.drawLine(points[i], points[i + 1]);
        }

        // Draw fill gradient
        if (points.size() >= 2) {
            QPainterPath path;
            path.moveTo(points.first().x(), height());
            for (const auto& pt : points) {
                path.lineTo(pt);
            }
            path.lineTo(points.last().x(), height());
            path.closeSubpath();

            QLinearGradient gradient(0, 0, 0, height());
            gradient.setColorAt(0, QColor(79, 195, 247, 40));
            gradient.setColorAt(1, QColor(79, 195, 247, 5));
            painter.fillPath(path, gradient);
        }
    }

private:
    QList<int> data_points_;
};

QWidget* create_header_widget(ThemeEngine* theme, QWidget* parent) {
    auto* widget = new QWidget(parent);
    widget->setObjectName("header_widget");
    widget->setFixedHeight(56);

    auto* layout = new QHBoxLayout(widget);
    layout->setContentsMargins(16, 0, 16, 0);
    layout->setSpacing(12);

    // Logo "E"
    auto* logo = new QLabel("E", widget);
    logo->setObjectName("header_logo");
    QFont logo_font;
    logo_font.setPointSize(24);
    logo_font.setBold(true);
    logo_font.setFamily("monospace");
    logo->setFont(logo_font);
    logo->setStyleSheet("color: #ffffff; background: transparent; border: none;");
    layout->addWidget(logo);

    // Title
    auto* title = new QLabel(QCoreApplication::translate("HeaderWidget", "Euxis ETX"), widget);
    title->setObjectName("header_title");
    QFont title_font;
    title_font.setPointSize(16);
    title_font.setBold(true);
    title->setFont(title_font);
    title->setStyleSheet("color: #e0e0e0; background: transparent; border: none;");
    layout->addWidget(title);

    // Breadcrumb navigation trail
    auto* breadcrumb = new BreadcrumbWidget(widget);
    breadcrumb->setObjectName("header_breadcrumb");
    layout->addWidget(breadcrumb);

    // Spacer
    layout->addStretch();

    // Burn rate sparkline
    auto* sparkline = new SparklineWidget(widget);
    layout->addWidget(sparkline);

    // Spacer
    layout->addSpacing(12);

    // Theme indicator
    auto* theme_indicator = new QLabel(widget);
    theme_indicator->setObjectName("theme_indicator");
    theme_indicator->setText(theme->current_theme());
    theme_indicator->setStyleSheet(
        "color: #888; background: rgba(255,255,255,0.05); "
        "border: 1px solid rgba(255,255,255,0.1); border-radius: 4px; "
        "padding: 4px 10px; font-size: 11px;");
    layout->addWidget(theme_indicator);

    // Update theme indicator when theme changes
    QObject::connect(theme, &ThemeEngine::theme_applied, theme_indicator,
                     [theme_indicator](const QString& name) {
        theme_indicator->setText(name);
    });

    return widget;
}

} // namespace euxis::etx

#include "header.moc"
