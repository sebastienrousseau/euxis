# Apply a patch to a FetchContent source tree exactly once.
#
#   PATCH_COMMAND ${CMAKE_COMMAND} -DPATCH_FILE=<patch> -P ApplyPatch.cmake
#
# Runs in the dependency's source directory. If the patch already reverses
# cleanly it was applied earlier (a re-run of the patch step), so do nothing;
# otherwise it must apply cleanly, or configuration fails.
cmake_minimum_required(VERSION 3.28)

if(NOT PATCH_FILE OR NOT EXISTS "${PATCH_FILE}")
    message(FATAL_ERROR "ApplyPatch: PATCH_FILE '${PATCH_FILE}' not found")
endif()
find_package(Git REQUIRED QUIET)

execute_process(
    COMMAND "${GIT_EXECUTABLE}" apply --reverse --check "${PATCH_FILE}"
    RESULT_VARIABLE already_applied OUTPUT_QUIET ERROR_QUIET)
if(already_applied EQUAL 0)
    return()
endif()

execute_process(
    COMMAND "${GIT_EXECUTABLE}" apply "${PATCH_FILE}"
    RESULT_VARIABLE apply_result)
if(NOT apply_result EQUAL 0)
    message(FATAL_ERROR "ApplyPatch: ${PATCH_FILE} does not apply")
endif()
