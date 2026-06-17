#include <QWidget>
#include <QPainter>
#include <QVector>
#include <QColor>

namespace euxis::etx {

class ScrollMinimapWidget : public QWidget {
    Q_OBJECT

public:
    explicit ScrollMinimapWidget(QWidget* parent = nullptr)
        : QWidget(parent)
        , viewport_start_(0.0f)
        , viewport_size_(0.2f)
    {
        setObjectName("scroll_minimap");
        setFixedWidth(20);
        setMinimumHeight(100);
    }

    void update_viewport(float start, float size) {
        viewport_start_ = qBound(0.0f, start, 1.0f);
        viewport_size_ = qBound(0.0f, size, 1.0f - viewport_start_);
        update();
    }

    void set_active_positions(const QVector<float>& positions) {
        active_positions_ = positions;
        update();
    }

protected:
    void paintEvent(QPaintEvent* /*event*/) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        int w = width();
        int h = height();

        // Track background
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(30, 30, 35));
        painter.drawRoundedRect(0, 0, w, h, 4, 4);

        // Viewport indicator
        int vp_y = static_cast<int>(viewport_start_ * h);
        int vp_h = qMax(8, static_cast<int>(viewport_size_ * h));
        painter.setBrush(QColor(255, 255, 255, 30));
        painter.drawRoundedRect(2, vp_y, w - 4, vp_h, 3, 3);

        // Agent position markers
        for (float pos : active_positions_) {
            float clamped = qBound(0.0f, pos, 1.0f);
            int marker_y = static_cast<int>(clamped * h);
            painter.setBrush(QColor("#4fc3f7"));
            painter.drawEllipse(QPointF(w / 2.0, marker_y), 3, 3);
        }
    }

private:
    float viewport_start_ = 0.0F;
    float viewport_size_ = 0.0F;
    QVector<float> active_positions_;
};

QWidget* create_scroll_minimap_widget(QWidget* parent) {
    return new ScrollMinimapWidget(parent);
}

} // namespace euxis::etx

#include "scroll_minimap.moc"
