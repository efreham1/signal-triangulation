cmake_minimum_required(VERSION 3.10)
project(triangulation_tests LANGUAGES CXX)

include(FetchContent)
include(GoogleTest)
enable_testing()

# Find OpenMP
find_package(OpenMP)

# GoogleTest
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.15.0.zip
  URL_HASH SHA256=ed98258b96c6dc8af33eeb03c8d94a05b185866604382e12980f576595bdc509
)
FetchContent_MakeAvailable(googletest)

# Collect project sources (exclude main.cpp)
file(GLOB_RECURSE TRIANGULATION_CORE_SOURCES "${CMAKE_SOURCE_DIR}/src/*.cpp")
list(FILTER TRIANGULATION_CORE_SOURCES EXCLUDE REGEX ".*/main\\.cpp$")

# Create a library from core sources for tests to link against
add_library(core_test_lib STATIC ${TRIANGULATION_CORE_SOURCES})
target_include_directories(core_test_lib PUBLIC "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(core_test_lib PUBLIC spdlog::spdlog nlohmann_json::nlohmann_json)

# Link OpenMP to core_test_lib if found
if(OpenMP_CXX_FOUND)
    target_link_libraries(core_test_lib PUBLIC OpenMP::OpenMP_CXX)
    target_compile_definitions(core_test_lib PUBLIC USE_OPENMP)
endif()

add_executable(unit_tests
    test_main.cpp
    test_datapoint.cpp
    test_cluster.cpp
    test_distance_cache.cpp
    test_CTA_base.cpp
    test_json_parser.cpp
    test_CTA1.cpp
    test_CTA2.cpp
)

add_executable(integration_tests
    test_triangulation.cpp
)

target_link_libraries(unit_tests GTest::gtest GTest::gtest_main core_test_lib)
target_link_libraries(integration_tests GTest::gtest GTest::gtest_main core_test_lib)

target_compile_definitions(unit_tests PRIVATE
    RECORDINGS_DIR=\"${CMAKE_SOURCE_DIR}/recordings\"
)

# Discover individual tests (each TEST() becomes a separate ctest)
gtest_discover_tests(unit_tests
    PROPERTIES LABELS "unit"
)

gtest_discover_tests(integration_tests
    PROPERTIES LABELS "integration"
)

# Define APP_BIN_PATH correctly per platform
if(WIN32)
  set(APP_BIN "${CMAKE_BINARY_DIR}/${APP_TARGET_NAME}.exe")
else()
  set(APP_BIN "${CMAKE_BINARY_DIR}/${APP_TARGET_NAME}")
endif()

target_compile_definitions(integration_tests PRIVATE 
    APP_BIN_PATH=\"${APP_BIN}\"
    RECORDINGS_DIR=\"${CMAKE_SOURCE_DIR}/recordings\"
)