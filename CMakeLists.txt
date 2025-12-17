cmake_minimum_required(VERSION 3.14)
project(signal_triangulation LANGUAGES CXX)

# Project-wide settings
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE OFF)

add_compile_options(-Wall -Wextra -Wpedantic -Wconversion)

# Profiling build type
if(CMAKE_BUILD_TYPE STREQUAL "Profiling")
    add_compile_options(-pg -Ofast)
    add_link_options(-pg)
endif() # Source layout: all sources live under ./src
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

# Find OpenMP
find_package(OpenMP)

FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.2
)
FetchContent_MakeAvailable(nlohmann_json)

FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.14.1
)

FetchContent_MakeAvailable(spdlog)

target_link_libraries(signal-triangulation
    PRIVATE
    nlohmann_json::nlohmann_json
    spdlog::spdlog
)

# Link OpenMP if found
if(OpenMP_CXX_FOUND)
    target_link_libraries(signal-triangulation PRIVATE OpenMP::OpenMP_CXX)
    target_compile_definitions(signal-triangulation PRIVATE USE_OPENMP)
endif()

# Install target (optional)
install(TARGETS signal-triangulation RUNTIME DESTINATION bin)

set(APP_TARGET_NAME signal-triangulation)

enable_testing()

add_subdirectory(tests EXCLUDE_FROM_ALL)

# Helpful configure-time output
message(STATUS "Configuring signal-triangulation")
message(STATUS "Source dir: ${SRC_DIR}")
message(STATUS "Found ${PROJECT_SOURCES}")
