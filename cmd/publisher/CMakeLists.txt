add_executable(euxis-publisher
  src/main.cpp
)

target_link_libraries(euxis-publisher PRIVATE
  euxis-publisher-cpp
  euxis-runtime-cpp
  nlohmann_json::nlohmann_json
  spdlog::spdlog
)

install(TARGETS euxis-publisher DESTINATION bin)
