cmake_minimum_required(VERSION 3.14)
project(signal_triangulation LANGUAGES CXX)

# Project-wide settings
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE OFF)

# Optional: enable extra warnings
if(MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Wconversion)
endif()

# Source layout: all sources live under ./src
set(SRC_DIR ${CMAKE_SOURCE_DIR}/src)

# Gather sources
file(GLOB_RECURSE PROJECT_SOURCES
    "${SRC_DIR}/*.cpp"
)

# Create executable
add_executable(signal-triangulation ${PROJECT_SOURCES})

target_include_directories(signal-triangulation
    PRIVATE
        ${SRC_DIR}
)

include(FetchContent)

FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.2
)
FetchContent_MakeAvailable(nlohmann_json)

target_link_libraries(signal-triangulation
    PRIVATE nlohmann_json::nlohmann_json
)

# Install target (optional)
install(TARGETS signal-triangulation RUNTIME DESTINATION bin)

set(APP_TARGET_NAME signal-triangulation)

enable_testing()

add_subdirectory(tests)

# Helpful configure-time output
message(STATUS "Configuring signal-triangulation")
message(STATUS "Source dir: ${SRC_DIR}")
message(STATUS "Found ${PROJECT_SOURCES}")
