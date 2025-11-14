cmake_minimum_required(VERSION 3.10)
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

find_package(nlohmann_json CONFIG REQUIRED)

target_link_libraries(signal-triangulation
    PRIVATE nlohmann_json::nlohmann_json
)

# Install target (optional)
install(TARGETS signal-triangulation RUNTIME DESTINATION bin)

# Helpful configure-time output
message(STATUS "Configuring signal-triangulation")
message(STATUS "Source dir: ${SRC_DIR}")
message(STATUS "Found ${PROJECT_SOURCES}")
