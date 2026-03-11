#include <euxis/etx/accessibility.hpp>

#include <QApplication>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QWidget>

namespace euxis::etx::a11y {

void label(QWidget* w, const QString& name, const QString& desc) {
    if (!w) return;
    w->setAccessibleName(name);
    if (!desc.isEmpty())
        w->setAccessibleDescription(desc);
}

void interactive(QWidget* w, const QString& name, QAccessible::Role) {
    if (!w) return;
    w->setAccessibleName(name);
    w->setFocusPolicy(Qt::TabFocus);
    enable_keyboard_activation(w);
    apply_focus_style(w);
}

/// Helper event filter that triggers click on Enter/Space.
class KeyboardActivationFilter : public QObject {
public:
    using QObject::QObject;

protected:
    bool eventFilter(QObject* obj, QEvent* event) override {
        if (event->type() == QEvent::KeyPress) {
            auto* ke = static_cast<QKeyEvent*>(event);
            if (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter ||
                ke->key() == Qt::Key_Space) {
                auto* w = qobject_cast<QWidget*>(obj);
                if (w) {
                    // Simulate a mouse click by sending a mouse press+release
                    QMouseEvent press(QEvent::MouseButtonPress, QPointF(0, 0),
                                      w->mapToGlobal(QPointF(0, 0)),
                                      Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
                    QApplication::sendEvent(w, &press);
                    QMouseEvent release(QEvent::MouseButtonRelease, QPointF(0, 0),
                                        w->mapToGlobal(QPointF(0, 0)),
                                        Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
                    QApplication::sendEvent(w, &release);
                    return true;
                }
            }
        }
        return QObject::eventFilter(obj, event);
    }
};

void enable_keyboard_activation(QWidget* w) {
    if (!w) return;
    // Avoid duplicate filters
    for (auto* child : w->children()) {
        if (dynamic_cast<KeyboardActivationFilter*>(child))
            return;
    }
    auto* filter = new KeyboardActivationFilter(w);
    w->installEventFilter(filter);
}

void apply_focus_style(QWidget* w) {
    if (!w) return;
    // Append focus indicator to existing stylesheet
    QString existing = w->styleSheet();
    if (!existing.contains(":focus")) {
        w->setStyleSheet(existing +
            " *:focus { outline: 2px solid #6db3f2; outline-offset: 2px; }");
    }
}

QWidget* create_status_indicator(const QString& text, const QString& color,
                                 QWidget* parent) {
    auto* container = new QWidget(parent);
    auto* layout = new QHBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    auto* dot = new QLabel(container);
    dot->setFixedSize(10, 10);
    dot->setStyleSheet(
        QString("background: %1; border-radius: 5px; border: none;").arg(color));

    auto* lbl = new QLabel(text, container);
    lbl->setStyleSheet("border: none; background: transparent;");

    layout->addWidget(dot);
    layout->addWidget(lbl);

    container->setAccessibleName(text);
    return container;
}

void install_focus_chain(const QList<QWidget*>& widgets) {
    for (int i = 0; i + 1 < widgets.size(); ++i) {
        QWidget::setTabOrder(widgets[i], widgets[i + 1]);
    }
}

void register_accessibility_factories() {
    // Register a factory for QFrame-based agent cards
    QAccessible::installFactory([](const QString& classname, QObject* obj)
                                    -> QAccessibleInterface* {
        if (classname == "euxis::etx::AgentCardWidget") {
            auto* w = qobject_cast<QWidget*>(obj);
            if (w)
                return new QAccessibleWidget(w, QAccessible::ListItem,
                                             w->accessibleName());
        }
        return nullptr;
    });
}

} // namespace euxis::etx::a11y
