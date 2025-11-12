cmake_minimum_required(VERSION 3.10)
project(signal_triangulation)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find SQLite package
find_package(SQLite3 REQUIRED)

# Add source files
add_executable(${PROJECT_NAME}
    src/main.cpp
    src/network/Server.cpp
    src/core/DatabaseHandler.cpp
    src/core/TriangulationService.cpp
)

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${SQLite3_INCLUDE_DIRS}
)

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    ${SQLite3_LIBRARIES}
)

# Platform specific settings
if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE wsock32 ws2_32)
endif()