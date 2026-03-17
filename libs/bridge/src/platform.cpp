#include <euxis/bridge/platform.hpp>
#include <euxis/runtime/platform.hpp>

namespace euxis::bridge {

auto detect_platform() -> PlatformInfo {
    const auto& core_info = euxis::runtime::Platform::current().info();
    
    PlatformInfo info;
    info.os_name = core_info.os_name;
    info.arch = core_info.arch_name;
    info.has_nsjail = core_info.has_nsjail;
    info.has_sandbox_exec = core_info.has_sandbox;
    
    return info;
}

auto has_nsjail() -> bool {
    return euxis::runtime::Platform::current().info().has_nsjail;
}

}  // namespace euxis::bridge
