cmake_minimum_required(VERSION 3.10)
project(triangulation_tests LANGUAGES CXX)

include(FetchContent)

# GoogleTest
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.15.0.zip
)
FetchContent_MakeAvailable(googletest)

add_executable(triangulation_tests test_triangulation.cpp)

target_link_libraries(triangulation_tests
    PRIVATE
      gtest_main
)

target_compile_definitions(triangulation_tests
    PRIVATE
      APP_BIN_PATH=\"${CMAKE_BINARY_DIR}/${APP_TARGET_NAME}\"
)

# One CTest test per JSON file, named after the file
file(GLOB RECORDING_JSONS "${CMAKE_SOURCE_DIR}/Recordings/*.json")

foreach(json_file IN LISTS RECORDING_JSONS)
  get_filename_component(stem "${json_file}" NAME_WE)
  string(REGEX REPLACE "[^A-Za-z0-9]" "_" test_name "${stem}")

  add_test(NAME "${test_name}" COMMAND triangulation_tests)
  set_tests_properties("${test_name}" PROPERTIES
    ENVIRONMENT "JSON_FILE=${json_file}"
  )
endforeach()