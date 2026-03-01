#include <euxis/etx/config.hpp>

namespace euxis::etx {

ETXConfig::ETXConfig()
    : settings_("Euxis", "ETX")
{
}

auto ETXConfig::theme() const -> QString {
    return settings_.value("theme", "liquid-glass").toString();
}

void ETXConfig::set_theme(const QString& name) {
    settings_.setValue("theme", name);
}

auto ETXConfig::refresh_interval() const -> int {
    return settings_.value("refresh_interval", 5000).toInt();
}

void ETXConfig::set_refresh_interval(int ms) {
    settings_.setValue("refresh_interval", ms);
}

auto ETXConfig::show_tips() const -> bool {
    return settings_.value("show_tips", true).toBool();
}

void ETXConfig::set_show_tips(bool show) {
    settings_.setValue("show_tips", show);
}

} // namespace euxis::etx
