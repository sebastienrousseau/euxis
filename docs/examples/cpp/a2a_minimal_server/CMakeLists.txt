add_executable(euxis_example_a2a_minimal_server main.cpp)
target_link_libraries(euxis_example_a2a_minimal_server
    PRIVATE
        euxis-a2a-cpp
        nlohmann_json::nlohmann_json
        unofficial-sodium::sodium
)
