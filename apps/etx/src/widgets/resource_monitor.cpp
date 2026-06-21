#include <QWidget>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QTimer>
#include <QFont>
#include <QPainter>
#include <QPainterPath>
#include <QFile>
#include <QTextStream>
#include <QVector>

namespace euxis::etx {

class SparklineMini : public QWidget {
    Q_OBJECT

public:
    explicit SparklineMini(const QColor& color, QWidget* parent = nullptr)
        : QWidget(parent)
        , color_(color)
    {
        setFixedSize(60, 20);
        values_.fill(0.0f, 30);
    }

    void add_value(float v) {
        values_.removeFirst();
        values_.append(v);
        update();
    }

protected:
    void paintEvent(QPaintEvent* /*event*/) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        if (values_.size() < 2) return;

        float max_val = 100.0f;
        double step_x = static_cast<double>(width()) / static_cast<double>(values_.size() - 1);

        QVector<QPointF> points;
        for (int i = 0; i < values_.size(); ++i) {
            double x = i * step_x;
            double y = height() - (static_cast<float>(values_[i]) / max_val * static_cast<float>(height()));
            points.append(QPointF(x, y));
        }

        // Fill
        QPainterPath path;
        path.moveTo(points.first().x(), height());
        for (const auto& pt : points) {
            path.lineTo(pt);
        }
        path.lineTo(points.last().x(), height());
        path.closeSubpath();

        QLinearGradient gradient(0, 0, 0, height());
        gradient.setColorAt(0, QColor(color_.red(), color_.green(), color_.blue(), 40));
        gradient.setColorAt(1, QColor(color_.red(), color_.green(), color_.blue(), 5));
        painter.fillPath(path, gradient);

        // Line
        QPen pen(color_, 1.0);
        painter.setPen(pen);
        for (int i = 0; i < points.size() - 1; ++i) {
            painter.drawLine(points[i], points[i + 1]);
        }
    }

private:
    QColor color_{};
    QVector<float> values_;
};

class ResourceMonitorWidget : public QWidget {
    Q_OBJECT

public:
    explicit ResourceMonitorWidget(QWidget* parent = nullptr)
        : QWidget(parent)
        , cpu_percent_(0.0f)
        , memory_percent_(0.0f)
        , thermal_celsius_(0.0f)
        , prev_total_(0)
        , prev_idle_(0)
    {
        setObjectName("resource_monitor");
        setFixedHeight(48);

        auto* layout = new QHBoxLayout(this);
        layout->setContentsMargins(16, 4, 16, 4);
        layout->setSpacing(24);

        QFont label_font;
        label_font.setPointSize(9);
        label_font.setFamily("monospace");

        QFont value_font;
        value_font.setPointSize(10);
        value_font.setBold(true);
        value_font.setFamily("monospace");

        // CPU section
        {
            auto* section = new QHBoxLayout();
            section->setSpacing(6);

            auto* label = new QLabel(tr("CPU"), this);
            label->setFont(label_font);
            label->setStyleSheet("color: #666; background: transparent; border: none;");
            section->addWidget(label);

            cpu_value_ = new QLabel(tr("---%"), this);
            cpu_value_->setFont(value_font);
            cpu_value_->setStyleSheet("color: #4fc3f7; background: transparent; border: none;");
            section->addWidget(cpu_value_);

            cpu_sparkline_ = new SparklineMini(QColor("#4fc3f7"), this);
            section->addWidget(cpu_sparkline_);

            layout->addLayout(section);
        }

        // Memory section
        {
            auto* section = new QHBoxLayout();
            section->setSpacing(6);

            auto* label = new QLabel(tr("MEM"), this);
            label->setFont(label_font);
            label->setStyleSheet("color: #666; background: transparent; border: none;");
            section->addWidget(label);

            mem_value_ = new QLabel(tr("---%"), this);
            mem_value_->setFont(value_font);
            mem_value_->setStyleSheet("color: #81c784; background: transparent; border: none;");
            section->addWidget(mem_value_);

            mem_sparkline_ = new SparklineMini(QColor("#81c784"), this);
            section->addWidget(mem_sparkline_);

            layout->addLayout(section);
        }

        // Thermal section
        {
            auto* section = new QHBoxLayout();
            section->setSpacing(6);

            auto* label = new QLabel(tr("TEMP"), this);
            label->setFont(label_font);
            label->setStyleSheet("color: #666; background: transparent; border: none;");
            section->addWidget(label);

            thermal_value_ = new QLabel(tr("---C"), this);
            thermal_value_->setFont(value_font);
            thermal_value_->setStyleSheet("color: #ffab00; background: transparent; border: none;");
            section->addWidget(thermal_value_);

            thermal_sparkline_ = new SparklineMini(QColor("#ffab00"), this);
            section->addWidget(thermal_sparkline_);

            layout->addLayout(section);
        }

        layout->addStretch();

        // Poll every 2 seconds
        auto* timer = new QTimer(this);
        connect(timer, &QTimer::timeout, this, &ResourceMonitorWidget::poll);
        timer->start(2000);

        // Initial read
        poll();
    }

    [[nodiscard]] float cpu_percent() const { return cpu_percent_; }
    [[nodiscard]] float memory_percent() const { return memory_percent_; }
    [[nodiscard]] float thermal_celsius() const { return thermal_celsius_; }

private slots:
    void poll() {
        read_cpu();
        read_memory();
        read_thermal();
    }

private:
    void read_cpu() {
#ifdef Q_OS_LINUX
        QFile file("/proc/stat");
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            cpu_value_->setText(tr("N/A"));
            return;
        }

        QTextStream stream(&file);
        QString line = stream.readLine();
        file.close();

        if (!line.startsWith("cpu ")) {
            cpu_value_->setText(tr("N/A"));
            return;
        }

        auto parts = line.split(' ', Qt::SkipEmptyParts);
        if (parts.size() < 8) {
            cpu_value_->setText(tr("N/A"));
            return;
        }

        long long user   = parts[1].toLongLong();
        long long nice   = parts[2].toLongLong();
        long long system = parts[3].toLongLong();
        long long idle   = parts[4].toLongLong();
        long long iowait = parts[5].toLongLong();
        long long irq    = parts[6].toLongLong();
        long long softirq = parts[7].toLongLong();

        long long total = user + nice + system + idle + iowait + irq + softirq;
        long long total_diff = total - prev_total_;
        long long idle_diff = idle - prev_idle_;

        if (total_diff > 0 && prev_total_ > 0) {
            cpu_percent_ = 100.0f * (1.0f - static_cast<float>(idle_diff) / total_diff);
            cpu_value_->setText(tr("%1%").arg(cpu_percent_, 0, 'f', 1));
            cpu_sparkline_->add_value(cpu_percent_);
        }

        prev_total_ = total;
        prev_idle_ = idle;
#else
        cpu_value_->setText(tr("N/A"));
#endif
    }

    void read_memory() {
#ifdef Q_OS_LINUX
        QFile file("/proc/meminfo");
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            mem_value_->setText(tr("N/A"));
            return;
        }

        QTextStream stream(&file);
        long long mem_total = 0;
        long long mem_available = 0;

        while (!stream.atEnd()) {
            QString line = stream.readLine();
            if (line.startsWith("MemTotal:")) {
                mem_total = line.split(' ', Qt::SkipEmptyParts)[1].toLongLong();
            } else if (line.startsWith("MemAvailable:")) {
                mem_available = line.split(' ', Qt::SkipEmptyParts)[1].toLongLong();
            }
            if (mem_total > 0 && mem_available > 0) break;
        }
        file.close();

        if (mem_total > 0) {
            memory_percent_ = 100.0f * (1.0f - static_cast<float>(mem_available) / mem_total);
            mem_value_->setText(tr("%1%").arg(memory_percent_, 0, 'f', 1));
            mem_sparkline_->add_value(memory_percent_);
        } else {
            mem_value_->setText(tr("N/A"));
        }
#else
        mem_value_->setText(tr("N/A"));
#endif
    }

    void read_thermal() {
#ifdef Q_OS_LINUX
        QFile file("/sys/class/thermal/thermal_zone0/temp");
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            thermal_value_->setText(tr("N/A"));
            return;
        }

        QTextStream stream(&file);
        int millideg = stream.readLine().trimmed().toInt();
        file.close();

        thermal_celsius_ = millideg / 1000.0f;
        thermal_value_->setText(tr("%1\xc2\xb0""C").arg(thermal_celsius_, 0, 'f', 0));
        thermal_sparkline_->add_value(thermal_celsius_);

        // Color red when >= 85C
        if (thermal_celsius_ >= 85.0f) {
            thermal_value_->setStyleSheet(
                "color: #f44336; background: transparent; border: none;");
        } else {
            thermal_value_->setStyleSheet(
                "color: #ffab00; background: transparent; border: none;");
        }
#else
        thermal_value_->setText(tr("N/A"));
#endif
    }

    float cpu_percent_;
    float memory_percent_;
    float thermal_celsius_;
    long long prev_total_;
    long long prev_idle_;

    QLabel* cpu_value_;
    QLabel* mem_value_;
    QLabel* thermal_value_;
    SparklineMini* cpu_sparkline_;
    SparklineMini* mem_sparkline_;
    SparklineMini* thermal_sparkline_;
};

QWidget* create_resource_monitor_widget(QWidget* parent) {
    return new ResourceMonitorWidget(parent);
}

} // namespace euxis::etx

#include "resource_monitor.moc"
