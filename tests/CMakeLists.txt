# ==============================================================================
# GenesisEmu - Tests Directory CMake Configuration (Refactored Structure)
# ==============================================================================
# This file compiles the unit test suite, linking GTest and testing the
# modular components of our pure emulation Core.
# ==============================================================================

# Enable CMake testing module (enables ctest command)
enable_testing()

# Define the Test Suite executable
add_executable(GenesisEmuTests
    Core/BusTests.cpp
    Core/VdpTests.cpp
    Core/CpuTests.cpp
    Core/DecoderTests.cpp
    Core/ArithmeticTests.cpp
    Core/VdpControlTests.cpp
    Core/FlowControlTests.cpp
    Core/CoreInstructionsTests.cpp
    Core/IoTests.cpp
    Core/CartridgeTests.cpp
)

# Link the Test Suite with:
# - GoogleTest Main (provides default main entry point)
# - GoogleTest Core Library
# - Our pure emulator Core (GenesisEmuCore) under test
target_link_libraries(GenesisEmuTests PRIVATE
    GTest::gtest
    GTest::gtest_main
    GenesisEmuCore
)

# Tell CMake's ctest runner to discover all tests inside GTest automatically
include(GoogleTest)
gtest_discover_tests(GenesisEmuTests)