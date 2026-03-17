#include <euxis/etx/config.hpp>
#include <QDir>
#include <QStandardPaths>
#include <cstdlib>

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

auto ETXConfig::euxis_home() -> QString {
    if (const char* env = std::getenv("EUXIS_HOME")) {
        return QString::fromUtf8(env);
    }
    return QDir::homePath() + "/.euxis";
}

auto ETXConfig::data_dir() -> QString {
    return euxis_home() + "/data";
}

auto ETXConfig::runtime_dir() -> QString {
    return euxis_home() + "/data/runtime";
}

} // namespace euxis::etx
