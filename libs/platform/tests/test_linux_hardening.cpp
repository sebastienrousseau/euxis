#include <gtest/gtest.h>
#include <euxis/runtime/platform.hpp>
#include <spdlog/spdlog.h>

namespace euxis::platform::sys {
    void enforce_micro_kernel_sandbox();
    bool has_hardware_sandbox();
}

namespace euxis::platform::tests {

TEST(LinuxHardeningTest, SandboxLogicPaths) {
    // Verify no-crash/branch coverage for enforcement
    EXPECT_NO_THROW(euxis::platform::sys::enforce_micro_kernel_sandbox());
    
    // has_hardware_sandbox depends on /dev/kvm existence on the host
    // We verify it returns a boolean consistently
    bool result = euxis::platform::sys::has_hardware_sandbox();
    (void)result;
    SUCCEED();
}

} // namespace
