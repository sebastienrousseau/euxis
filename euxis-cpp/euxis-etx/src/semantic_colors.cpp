#include <euxis/etx/semantic_colors.hpp>

#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>

namespace euxis::etx {

auto severity_color(Severity sev) -> QColor {
    switch (sev) {
    case Severity::Info:     return QColor(0x21, 0x96, 0xf3);  // #2196f3
    case Severity::Success:  return QColor(0x4c, 0xaf, 0x50);  // #4caf50
    case Severity::Warning:  return QColor(0xff, 0x98, 0x00);  // #ff9800
    case Severity::Error:    return QColor(0xf4, 0x43, 0x36);  // #f44336
    case Severity::Critical: return QColor(0xd3, 0x2f, 0x2f);  // #d32f2f
    }
    return QColor(0x88, 0x88, 0x88);
}

auto severity_qss(Severity sev) -> QString {
    QColor c = severity_color(sev);
    return QString(
        "background: rgba(%1,%2,%3,0.15); "
        "border: 1px solid rgba(%1,%2,%3,0.3); "
        "border-radius: 4px; padding: 2px 8px; "
        "color: %4;")
        .arg(c.red()).arg(c.green()).arg(c.blue())
        .arg(c.name());
}

void apply_severity_badge(QWidget* widget, Severity sev) {
    widget->setStyleSheet(severity_qss(sev));
}

void apply_pulse_animation(QWidget* widget, const QColor& color, int duration_ms) {
    auto* effect = new QGraphicsOpacityEffect(widget);
    widget->setGraphicsEffect(effect);

    auto* anim = new QPropertyAnimation(effect, "opacity", widget);
    anim->setDuration(duration_ms);
    anim->setStartValue(0.5);
    anim->setEndValue(1.0);
    anim->setLoopCount(-1);  // infinite
    anim->start();

    // Also tint the widget
    widget->setStyleSheet(
        severity_qss(Severity::Critical) +
        QString(" color: %1;").arg(color.name()));
}

auto status_to_severity(const QString& status) -> Severity {
    QString s = status.toLower();
    if (s == "running" || s == "active" || s == "ok" || s == "healthy") {
        return Severity::Success;
    }
    if (s == "idle" || s == "pending" || s == "ready") {
        return Severity::Info;
    }
    if (s == "warning" || s == "degraded" || s == "slow") {
        return Severity::Warning;
    }
    if (s == "error" || s == "failed" || s == "down") {
        return Severity::Error;
    }
    if (s == "critical" || s == "fatal" || s == "panic") {
        return Severity::Critical;
    }
    return Severity::Info;
}

} // namespace euxis::etx
