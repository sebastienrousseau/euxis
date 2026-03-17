#pragma once

#include <QAccessible>
#include <QAccessibleWidget>
#include <QList>
#include <QString>
#include <QWidget>

namespace euxis::etx::a11y {

/// Set accessible name and optional description on a widget.
void label(QWidget* w, const QString& name, const QString& desc = {});

/// Mark a widget as interactive with TabFocus policy and keyboard activation.
void interactive(QWidget* w, const QString& name,
                 QAccessible::Role role = QAccessible::Button);

/// Install an event filter that activates the widget on Enter/Space keys.
void enable_keyboard_activation(QWidget* w);

/// Apply a visible focus indicator style to a widget.
void apply_focus_style(QWidget* w);

/// Create a status indicator widget with a colored dot and text label.
/// Replaces color-only indicators for accessibility.
QWidget* create_status_indicator(const QString& text, const QString& color,
                                 QWidget* parent = nullptr);

/// Set up a Tab focus chain across the given widgets in order.
void install_focus_chain(const QList<QWidget*>& widgets);

/// Register custom QAccessible factories for ETX widgets.
void register_accessibility_factories();

} // namespace euxis::etx::a11y
