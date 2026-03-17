#pragma once
#include <QColor>
#include <QString>
#include <QWidget>

namespace euxis::etx {

/// Severity levels for semantic highlighting.
enum class Severity {
    Info,
    Success,
    Warning,
    Error,
    Critical,
};

/// Get the QColor for a severity level.
auto severity_color(Severity sev) -> QColor;

/// Get a QSS-compatible stylesheet fragment for a severity badge.
auto severity_qss(Severity sev) -> QString;

/// Apply a colored badge style to a widget based on severity.
void apply_severity_badge(QWidget* widget, Severity sev);

/// Apply a pulsing opacity animation to a widget (for Critical severity).
void apply_pulse_animation(QWidget* widget, const QColor& color, int duration_ms = 1500);

/// Map a status string (running/idle/error/etc.) to a Severity.
auto status_to_severity(const QString& status) -> Severity;

} // namespace euxis::etx
