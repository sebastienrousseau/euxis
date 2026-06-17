add_executable(euxis_example_tool_calling_loop main.cpp)
target_link_libraries(euxis_example_tool_calling_loop
    PRIVATE
        euxis-runtime-cpp
        nlohmann_json::nlohmann_json
)
