# CMake generated Testfile for 
# Source directory: /home/seb/.euxis/euxis-cpp/euxis-etx
# Build directory: /home/seb/.euxis/euxis-cpp/build-cov/euxis-etx
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test([=[euxis-etx-tests]=] "/home/seb/.euxis/euxis-cpp/build-cov/euxis-etx/euxis-etx-tests")
set_tests_properties([=[euxis-etx-tests]=] PROPERTIES  ENVIRONMENT "LSAN_OPTIONS=suppressions=/home/seb/.euxis/euxis-cpp/euxis-etx/tests/lsan_suppressions.txt" _BACKTRACE_TRIPLES "/home/seb/.euxis/euxis-cpp/euxis-etx/CMakeLists.txt;211;add_test;/home/seb/.euxis/euxis-cpp/euxis-etx/CMakeLists.txt;0;")
