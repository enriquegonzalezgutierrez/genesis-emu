# ==============================================================================
# GenesisEmu - Tests Directory CMake Configuration (Updated)
# ==============================================================================
# This file compiles the unit test suite, linking GTest and testing the
# modular components of our pure emulation Core.
# Added 'Core/VdpControlTests.cpp' to verify VDP Control Unit isolation.
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
)

# Link the Test Suite with:
# - GoogleTest Main (provides a default main() entry point for running tests)
# - GoogleTest Core Library
# - Our pure emulator Core (GenesisEmuCore) which we are testing
target_link_libraries(GenesisEmuTests PRIVATE
    GTest::gtest
    GTest::gtest_main
    GenesisEmuCore
)

# Tell CMake's ctest runner to discover all tests inside GTest automatically
include(GoogleTest)
gtest_discover_tests(GenesisEmuTests)