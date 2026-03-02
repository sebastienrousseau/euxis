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

    // Path resolution
    [[nodiscard]] static auto euxis_home() -> QString;
    [[nodiscard]] static auto data_dir() -> QString;
    [[nodiscard]] static auto runtime_dir() -> QString;

private:
    QSettings settings_;
};

} // namespace euxis::etx
