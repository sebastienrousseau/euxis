# cmake/EuxisStdModule.cmake
#
# Detects whether the active compiler supports `import std;` and
# provides a `euxis::std_module` INTERFACE target that consumers
# can link to opt into the precompiled std module.
#
# Detection logic
# ---------------
# Two conditions must hold:
#   1. The C++ compiler is Clang or AppleClang at version >= 19
#      (LLVM 19 is the first release that shipped an upstream
#      std.cppm module file alongside libc++).
#   2. The libc++ install ships `share/libc++/v1/std.cppm` — we
#      use that file path as the canonical detection probe.
#
# When both conditions pass, the function `euxis_enable_std_module()`
# precompiles std.cppm into a .pcm at configure time and creates
# an INTERFACE target `euxis::std_module` carrying the right
# `-fmodule-file=std=<path>` flag plus -stdlib=libc++.
#
# Consumers
# ---------
# Targets that want `import std;` add the INTERFACE link:
#
#   target_link_libraries(my_target PRIVATE euxis::std_module)
#   target_compile_options(my_target PRIVATE -fmodules)
#
# and replace `#include <…>` with `import std;` inside the
# translation units.
#
# Why opt-in
# ----------
# Apple Clang's libc++ does not yet ship std.cppm (as of
# AppleClang 21). Forcing import std; project-wide would break
# every consumer on macOS Xcode toolchains. Keeping it as an
# INTERFACE target means individual libraries can pilot the
# migration without affecting the rest of the build.

include_guard(GLOBAL)

function(euxis_enable_std_module)
    if(NOT (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR
            CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang"))
        message(STATUS "Euxis: import std; not enabled (compiler is ${CMAKE_CXX_COMPILER_ID})")
        return()
    endif()
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "19")
        message(STATUS "Euxis: import std; not enabled (need Clang >= 19, have ${CMAKE_CXX_COMPILER_VERSION})")
        return()
    endif()

    # Resolve the libc++ prefix from the compiler. Homebrew installs
    # at /opt/homebrew/opt/llvm; vendor distributions vary.
    get_filename_component(_compiler_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
    get_filename_component(_llvm_prefix  "${_compiler_dir}" DIRECTORY)
    set(_std_cppm "${_llvm_prefix}/share/libc++/v1/std.cppm")

    if(NOT EXISTS "${_std_cppm}")
        message(STATUS "Euxis: import std; not enabled (no ${_std_cppm})")
        return()
    endif()

    set(_pcm "${CMAKE_BINARY_DIR}/euxis_std.pcm")

    # Precompile the std module at configure time. The `--precompile`
    # output is a BMI consumers feed back in via -fmodule-file=std=...
    add_custom_command(
        OUTPUT "${_pcm}"
        COMMAND "${CMAKE_CXX_COMPILER}"
            -std=c++23 -stdlib=libc++
            -Wno-reserved-module-identifier
            --precompile "${_std_cppm}"
            -o "${_pcm}"
        DEPENDS "${_std_cppm}"
        COMMENT "Precompiling C++23 std module (${_pcm})"
        VERBATIM
    )
    add_custom_target(euxis_std_module_target ALL DEPENDS "${_pcm}")

    add_library(euxis_std_module INTERFACE)
    add_library(euxis::std_module ALIAS euxis_std_module)
    target_compile_options(euxis_std_module INTERFACE
        -stdlib=libc++
        "-fmodule-file=std=${_pcm}"
    )
    add_dependencies(euxis_std_module euxis_std_module_target)

    message(STATUS "Euxis: import std; enabled — link `euxis::std_module` to opt in")
endfunction()
