cmake_minimum_required(VERSION 3.10)
project(triangulation_tests LANGUAGES CXX)

include(FetchContent)
enable_testing()

# GoogleTest
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.15.0.zip
)
FetchContent_MakeAvailable(googletest)

# Collect project sources (exclude main.cpp)
file(GLOB_RECURSE TRIANGULATION_CORE_SOURCES "${CMAKE_SOURCE_DIR}/src/*.cpp")
list(FILTER TRIANGULATION_CORE_SOURCES EXCLUDE REGEX ".*/main\\.cpp$")

# Plane-fit tests (separate executable)
add_executable(plane_fit_tests
  test_plane_fit.cpp
  ${TRIANGULATION_CORE_SOURCES}
)
target_include_directories(plane_fit_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(plane_fit_tests PRIVATE
  gtest_main
  nlohmann_json::nlohmann_json
)
add_test(NAME plane_fit COMMAND plane_fit_tests)
set_tests_properties(plane_fit PROPERTIES LABELS "plane")

# Triangulation/location tests (separate executable)
add_executable(triangulation_tests
  test_triangulation.cpp
  ${TRIANGULATION_CORE_SOURCES}
)
target_include_directories(triangulation_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(triangulation_tests
  PRIVATE
  gtest_main
  nlohmann_json::nlohmann_json
)

# Define APP_BIN_PATH correctly per platform
if(WIN32)
  set(APP_BIN "${CMAKE_BINARY_DIR}/${APP_TARGET_NAME}.exe")
else()
  set(APP_BIN "${CMAKE_BINARY_DIR}/${APP_TARGET_NAME}")
endif()

target_compile_definitions(triangulation_tests PRIVATE APP_BIN_PATH=\"${APP_BIN}\")

# One CTest test per JSON file for the triangulation suite
file(GLOB RECORDING_JSONS "${CMAKE_SOURCE_DIR}/recordings/*.json")

foreach(json_file IN LISTS RECORDING_JSONS)
  get_filename_component(stem "${json_file}" NAME_WE)
  string(REGEX REPLACE "[^A-Za-z0-9]" "_" test_name "${stem}")

  add_test(NAME "location_${test_name}"
    COMMAND triangulation_tests --gtest_filter=Triangulation.DistanceCheckForFile)
  set_tests_properties("location_${test_name}" PROPERTIES
    ENVIRONMENT "JSON_FILE=${json_file}"
    LABELS "location"
  )
endforeach()