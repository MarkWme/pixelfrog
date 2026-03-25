cmake_minimum_required(VERSION 3.22)
project(pixelfrog VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# DEMO_VULN_MODE / DEMO_XRAY_SECRET are set via Conan toolchain (see conanfile.py) or -D on the CMake CLI.

option(DEMO_XRAY_SECRET
    "Embed fake API-key-shaped literals in the pixelfrog binary for Xray secret-scanning demos only."
    OFF)

find_package(CLI11 REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(spdlog REQUIRED)
find_package(stb REQUIRED)

add_library(pixelfrog_lib STATIC
    src/processor.cpp
    src/filters.cpp
    src/logger.cpp
)
target_include_directories(pixelfrog_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(pixelfrog_lib
    PUBLIC
        nlohmann_json::nlohmann_json
        spdlog::spdlog
        stb::stb
)
target_compile_definitions(pixelfrog_lib PRIVATE
    $<$<STREQUAL:${DEMO_VULN_MODE},ON>:PIXELFROG_DEMO_VULN_MODE>
)

add_executable(pixelfrog src/main.cpp)
target_link_libraries(pixelfrog PRIVATE pixelfrog_lib CLI11::CLI11)
if(DEMO_XRAY_SECRET)
    target_compile_definitions(pixelfrog PRIVATE PIXELFROG_DEMO_XRAY_SECRET)
endif()

enable_testing()
find_package(Catch2 3 REQUIRED)
add_executable(pixelfrog_tests
    test/test_filters.cpp
    test/test_processor.cpp
    test/test_cli.cpp
)
target_link_libraries(pixelfrog_tests PRIVATE pixelfrog_lib Catch2::Catch2WithMain CLI11::CLI11)
add_test(NAME pixelfrog_tests COMMAND pixelfrog_tests)
