#include <euxis/etx/theme.hpp>

#include <QApplication>
#include <QFile>
#include <QTextStream>

namespace euxis::etx {

ThemeEngine::ThemeEngine(QObject* parent)
    : QObject(parent)
    , current_("liquid-glass")
    , themes_({"liquid-glass", "calm", "focused"})
{
}

void ThemeEngine::apply_theme(const QString& name) {
    if (!themes_.contains(name)) return;

    QString stylesheet = load_stylesheet(name);
    if (!stylesheet.isEmpty()) {
        qApp->setStyleSheet(stylesheet);
        current_ = name;
        emit theme_applied(name);
    }
}

void ThemeEngine::cycle_theme() {
    int idx = themes_.indexOf(current_);
    idx = (idx + 1) % themes_.size();
    apply_theme(themes_.at(idx));
}

auto ThemeEngine::current_theme() const -> QString {
    return current_;
}

auto ThemeEngine::available_themes() const -> QStringList {
    return themes_;
}

auto ThemeEngine::load_stylesheet(const QString& name) const -> QString {
    QString path = QString(":/resources/styles/%1.qss").arg(name);
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    QTextStream stream(&file);
    return stream.readAll();
}

auto ThemeEngine::semantic_overrides() const -> QHash<QString, QColor> {
    // Extension point: themes can override severity colors.
    // Returns empty by default (use built-in semantic_colors defaults).
    return {};
}

} // namespace euxis::etx
