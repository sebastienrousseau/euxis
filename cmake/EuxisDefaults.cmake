# Warnings: -Werror controlled via CMAKE_COMPILE_WARNING_AS_ERROR (set in root CMakeLists.txt)
add_compile_options(-Wall -Wextra -Wpedantic -Wshadow)

# Security hardening flags
add_compile_options(-fstack-protector-strong)
add_compile_options(-Wformat-security)
if(NOT WIN32)
  add_compile_options(-fPIE)
  # macOS uses ld64 which does not recognise the GNU `-z relro`,
  # `-z now`, or `-pie` ld options — these are Linux-only ld
  # conventions. On Apple targets PIE is the default and RELRO is
  # not applicable, so simply skip the flags.
  if(NOT APPLE)
    add_link_options(-Wl,-z,relro -Wl,-z,now)
    add_link_options(-pie)
  endif()
  # _FORTIFY_SOURCE requires optimization; only set for Release/RelWithDebInfo
  if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug" AND NOT CMAKE_BUILD_TYPE STREQUAL "")
    # Use _FORTIFY_SOURCE=3 on GCC 12+ / Clang 14+, fallback to 2
    if((CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "12")
       OR (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "14"))
      add_compile_definitions(_FORTIFY_SOURCE=3)
    else()
      add_compile_definitions(_FORTIFY_SOURCE=2)
    endif()
  endif()
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "14")
  # GCC 14 false-positive in nlohmann_json iter_impl.hpp; same family of
  # warning as the GCC 15 <regex>/<functional> case. Suppressing both
  # -Wuninitialized and -Wmaybe-uninitialized is the upstream-recommended
  # workaround for the json iterator chains.
  add_compile_options(-Wno-uninitialized -Wno-maybe-uninitialized)
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "16")
  # GCC 16 emits false-positive -Warray-bounds on std::shared_ptr allocator
  # paths and nlohmann::json::dump() template inlining when combined with
  # _FORTIFY_SOURCE=3 LTO. Disable globally on GCC 16+ to unblock builds.
  add_compile_options(-Wno-array-bounds)
endif()

# Fast linker auto-detection (mold > lld > default)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  find_program(MOLD_LINKER mold)
  find_program(LLD_LINKER ld.lld)
  if(MOLD_LINKER)
    add_link_options(-fuse-ld=mold)
    message(STATUS "Euxis: using mold linker")
  elseif(LLD_LINKER)
    add_link_options(-fuse-ld=lld)
    message(STATUS "Euxis: using lld linker")
  endif()
endif()

# Sanitizers (debug/dev builds)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  if(NOT EUXIS_DISABLE_SANITIZERS)
    add_compile_options(-fsanitize=address,undefined)
    add_link_options(-fsanitize=address,undefined)

    # LeakSanitizer uses ptrace(PTRACE_ATTACH) on itself to walk the heap at exit.
    # On hosts with Yama LSM hardening (kernel.yama.ptrace_scope >= 1) — common on
    # CachyOS, hardened Arch, Ubuntu hardened, WSL2 — that ptrace is denied and LSan
    # aborts the process even when no leak occurred. Detect at configure time and
    # have euxis_add_library() set ASAN_OPTIONS=detect_leaks=0 on each test target
    # so ctest still reflects real test outcomes.
    set(EUXIS_LSAN_NEEDS_DISABLE OFF CACHE INTERNAL "Yama ptrace_scope blocks LSan")
    if(EXISTS "/proc/sys/kernel/yama/ptrace_scope")
      file(READ "/proc/sys/kernel/yama/ptrace_scope" _yama_ptrace_scope LIMIT 4)
      string(STRIP "${_yama_ptrace_scope}" _yama_ptrace_scope)
      if(NOT _yama_ptrace_scope STREQUAL "0")
        set(EUXIS_LSAN_NEEDS_DISABLE ON CACHE INTERNAL "Yama ptrace_scope blocks LSan")
        message(STATUS "Euxis: kernel.yama.ptrace_scope=${_yama_ptrace_scope} blocks LeakSanitizer; tests will run with ASAN_OPTIONS=detect_leaks=0")
        message(STATUS "Euxis: to enable leak detection, run: sudo sysctl kernel.yama.ptrace_scope=0")
      else()
        message(STATUS "Euxis: LeakSanitizer enabled (ptrace_scope=0)")
      endif()
    endif()
  endif()
endif()

# Coverage support (opt-in via -DEUXIS_COVERAGE=ON)
if(EUXIS_COVERAGE)
  add_compile_options(--coverage -fprofile-arcs -ftest-coverage)
  add_link_options(--coverage)
endif()

# AppleClang 21+ promoted -Wcharacter-conversion to error. The
# warning fires inside upstream googletest's pretty-printers
# (intentional char8_t -> char32_t coercion). The header lands in
# our test TUs so suppressing on the gtest target alone does not
# help; relax the error promotion globally for this compiler band.
if(CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang" AND
   CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "21")
  add_compile_options(-Wno-error=character-conversion)
endif()

# Third-party FetchContent targets pinned at versions that predate
# stricter Clang 22 / AppleClang 21 enforcement (spdlog 1.15.0 +
# bundled fmt notably reject some consteval propagation).
# Relax -Werror on their own TUs so our first-party -Werror gate
# stays intact. Call from the root CMakeLists.txt AFTER the
# FetchContent_MakeAvailable blocks so the targets exist.
function(euxis_relax_thirdparty_warnings)
  foreach(_ext_target IN LISTS ARGN)
    if(TARGET ${_ext_target})
      target_compile_options(${_ext_target} PRIVATE -Wno-error)
    endif()
  endforeach()
endfunction()
