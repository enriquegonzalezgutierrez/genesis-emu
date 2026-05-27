# ==============================================================================
# GenesisEmu - Root CMake Configuration
# ==============================================================================
# This file orchestrates the C++ compilation process, linking dependencies,
# setting strict compiler flags, and organizing subdirectories.
# ==============================================================================

cmake_minimum_required(VERSION 3.22)
project(GenesisEmu VERSION 1.0.0 LANGUAGES CXX)

# Enforce C++20 Standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable strict compiler warnings to catch emulation and memory bugs early
if(MSVC)
    add_compile_options(/W4 /WX)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

# Build configurations options
option(BUILD_TESTING "Build the unit test suite" ON)

# Define output binary directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# ==============================================================================
# Dependency Management
# ==============================================================================

# 1. Locate SDL2 (Required for presentation adapters in the Outer Hexagon)
find_package(SDL2 REQUIRED)
include_directories(${SDL2_INCLUDE_DIRS})

# 2. Locate GoogleTest (Required for TDD tests)
if(BUILD_TESTING)
    find_package(GTest REQUIRED)
endif()

# ==============================================================================
# Subdirectory Organization
# ==============================================================================

# Target directory for the emulator Core and Presentation app
add_subdirectory(src)

# Target directory for the TDD Unit Tests
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()