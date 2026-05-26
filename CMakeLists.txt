# ==============================================================================
# GenesisEmu - Root CMake Configuration (Corrected)
# ==============================================================================
# This file orchestrates the C++ compilation process, linking dependencies,
# setting strict compiler flags, and organizing subdirectories.
# ==============================================================================

cmake_minimum_required(VERSION 3.22)
project(GenesisEmu VERSION 1.0.0 LANGUAGES CXX)

# Force C++20 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable strict compiler warnings to catch emulation/memory bugs early
if(MSVC)
    add_compile_options(/W4 /WX)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

# Build options
option(BUILD_TESTING "Build the unit test suite" ON)

# Define Output Directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# ==============================================================================
# Dependency Management
# ==============================================================================

# 1. Look for SDL2 (Required for Outer Hexagon - Graphics/Audio Adapters)
find_package(SDL2 REQUIRED)
include_directories(${SDL2_INCLUDE_DIRS})

# 2. Look for GoogleTest (Required for TDD tests)
if(BUILD_TESTING)
    find_package(GTest REQUIRED)
    # Modern CMake handles GTest includes automatically when linking to target.
    # The obsolete 'include(${GTEST_USE_FILE})' has been removed to avoid configuration errors.
endif()

# ==============================================================================
# Subdirectory Organization
# ==============================================================================

# Target directory for the emulator Core and App
add_subdirectory(src)

# Target directory for the TDD Unit Tests
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()