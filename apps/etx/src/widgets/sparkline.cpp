#include <QWidget>
#include <QPainter>
#include <QPainterPath>
#include <QVector>
#include <QFont>
#include <QColor>
#include <QString>

namespace euxis::etx {

class SparklineChartWidget : public QWidget {
    Q_OBJECT

public:
    explicit SparklineChartWidget(QWidget* parent = nullptr)
        : QWidget(parent)
        , accent_color_(79, 195, 247)
    {
        setObjectName("sparkline_chart");
        setMinimumSize(60, 20);
    }

    void add_value(float v) {
        values_.append(v);
        if (values_.size() > 100) {
            values_.removeFirst();
        }
        update();
    }

    void set_values(const QVector<float>& vals) {
        values_ = vals;
        if (values_.size() > 100) {
            values_ = values_.mid(values_.size() - 100);
        }
        update();
    }

    void set_label(const QString& label) {
        label_ = label;
        update();
    }

protected:
    void paintEvent(QPaintEvent* /*event*/) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        int w = width();
        int h = height();

        if (values_.size() < 2) {
            // Draw label even with insufficient data
            if (!label_.isEmpty()) {
                draw_label(painter);
            }
            return;
        }

        // Determine value range
        float min_val = values_.first();
        float max_val = values_.first();
        for (float v : values_) {
            if (v < min_val) min_val = v;
            if (v > max_val) max_val = v;
        }

        // Avoid division by zero
        float range = max_val - min_val;
        if (range < 0.001f) range = 1.0f;

        // Build points
        double step_x = static_cast<double>(w) / (values_.size() - 1);
        QVector<QPointF> points;
        for (int i = 0; i < values_.size(); ++i) {
            double x = i * step_x;
            double normalized = (values_[i] - min_val) / range;
            double y = h - (normalized * h);
            points.append(QPointF(x, y));
        }

        // Filled area with gradient
        QPainterPath path;
        path.moveTo(points.first().x(), h);
        for (const auto& pt : points) {
            path.lineTo(pt);
        }
        path.lineTo(points.last().x(), h);
        path.closeSubpath();

        QLinearGradient gradient(0, 0, 0, h);
        gradient.setColorAt(0, QColor(accent_color_.red(), accent_color_.green(),
                                       accent_color_.blue(), 60));
        gradient.setColorAt(1, QColor(accent_color_.red(), accent_color_.green(),
                                       accent_color_.blue(), 0));
        painter.fillPath(path, gradient);

        // Line
        QPen pen(accent_color_, 1.5);
        painter.setPen(pen);
        painter.drawPolyline(points.data(), points.size());

        // Label
        if (!label_.isEmpty()) {
            draw_label(painter);
        }
    }

private:
    void draw_label(QPainter& painter) {
        QFont label_font;
        label_font.setPointSize(8);
        label_font.setFamily("monospace");
        painter.setFont(label_font);
        painter.setPen(QColor(200, 200, 200, 180));
        painter.drawText(4, 12, label_);
    }

    QVector<float> values_;
    QString label_;
    QColor accent_color_{};
};

QWidget* create_sparkline_chart_widget(QWidget* parent) {
    return new SparklineChartWidget(parent);
}

} // namespace euxis::etx

#include "sparkline.moc"
