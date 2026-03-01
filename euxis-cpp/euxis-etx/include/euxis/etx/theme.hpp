#pragma once
#include <QObject>
#include <QString>
#include <QStringList>

namespace euxis::etx {

class ThemeEngine : public QObject {
    Q_OBJECT

public:
    explicit ThemeEngine(QObject* parent = nullptr);

    void apply_theme(const QString& name);
    void cycle_theme();
    [[nodiscard]] auto current_theme() const -> QString;
    [[nodiscard]] auto available_themes() const -> QStringList;
    [[nodiscard]] auto load_stylesheet(const QString& name) const -> QString;

signals:
    void theme_applied(const QString& name);

private:
    QString current_;
    QStringList themes_;
};

} // namespace euxis::etx
