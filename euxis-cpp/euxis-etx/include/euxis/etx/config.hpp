#pragma once
#include <QSettings>
#include <QString>

namespace euxis::etx {

class ETXConfig {
public:
    ETXConfig();

    [[nodiscard]] auto theme() const -> QString;
    void set_theme(const QString& name);

    [[nodiscard]] auto refresh_interval() const -> int;
    void set_refresh_interval(int ms);

    [[nodiscard]] auto show_tips() const -> bool;
    void set_show_tips(bool show);

private:
    QSettings settings_;
};

} // namespace euxis::etx
