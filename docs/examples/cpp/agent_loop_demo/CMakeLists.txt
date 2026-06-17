add_executable(euxis_example_agent_loop_demo main.cpp)
target_link_libraries(euxis_example_agent_loop_demo
    PRIVATE
        euxis-runtime-cpp
        euxis-core-cpp
        euxis-metrics-cpp
        nlohmann_json::nlohmann_json
)
