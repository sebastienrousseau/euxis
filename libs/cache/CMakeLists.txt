find_package(SQLite3 REQUIRED)
if(NOT TARGET SQLite3::SQLite3)
    add_library(SQLite3::SQLite3 INTERFACE IMPORTED)
    if(DEFINED SQLite3_INCLUDE_DIRS)
        set_target_properties(SQLite3::SQLite3 PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${SQLite3_INCLUDE_DIRS}")
    endif()
    if(DEFINED SQLite3_LIBRARIES)
        set_target_properties(SQLite3::SQLite3 PROPERTIES
            INTERFACE_LINK_LIBRARIES "${SQLite3_LIBRARIES}")
    endif()
endif()

euxis_add_library(euxis-cache-cpp
  SOURCES
    src/hash.cpp
    src/key.cpp
    src/store.cpp
  DEPS
    unofficial-sodium::sodium
    SQLite3::SQLite3
  TEST_SOURCES
    tests/test_hash.cpp
    tests/test_store.cpp
)
