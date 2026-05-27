# ==============================================================================
# GenesisEmu - Source Directory CMake Configuration (Refactored Structure)
# ==============================================================================
# This file compiles the emulator core domain into a static library and links
# it to the main presentation application executable.
# ==============================================================================

# ------------------------------------------------------------------------------
# 1. Core Domain Library (The Inner Hexagon)
# ------------------------------------------------------------------------------
# Defines a standalone static library with ZERO external operating system
# or graphical rendering dependencies.
# ------------------------------------------------------------------------------
add_library(GenesisEmuCore STATIC
    Core/Domain/Bus/MainBus.cpp
    Core/Domain/Vdp/Vdp.cpp
    Core/Domain/Vdp/VdpControlUnit.cpp
    Core/Domain/Vdp/VdpRenderer.cpp
    Core/Domain/Io/IoPorts.cpp
    Core/Domain/M68k/M68k.cpp
    Core/Domain/M68k/M68kDecoder.cpp
    Core/Domain/M68k/M68kAddressing.cpp
    Core/Domain/M68k/M68kArithmetic.cpp
    Core/Domain/M68k/M68kFlowControl.cpp
    Core/Domain/M68k/M68kCoreInstructions.cpp
    Core/Domain/Cartridge/Cartridge.cpp
)

# Export include directories target-centrically to downstream consumers
target_include_directories(GenesisEmuCore PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Domain
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Domain/Bus
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Domain/Cartridge
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Domain/Common
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Domain/Io
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Domain/M68k
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Domain/Vdp
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Domain/WorkRAM
)

# ------------------------------------------------------------------------------
# 2. Main Presentation Application Executable (The Outer Hexagon)
# ------------------------------------------------------------------------------
add_executable(GenesisEmu
    App/main.cpp
    Adapters/SdlVideoAdapter.cpp
)

target_include_directories(GenesisEmu PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/Adapters
    ${CMAKE_CURRENT_SOURCE_DIR}/App
)

# Link the executable to our core library and host presentation drivers
target_link_libraries(GenesisEmu PRIVATE 
    GenesisEmuCore
    ${SDL2_LIBRARIES}
)

# ------------------------------------------------------------------------------
# 3. Standalone Verification Tool Target
# ------------------------------------------------------------------------------
add_executable(GenesisRomCheck
    App/RomCheck.cpp
)