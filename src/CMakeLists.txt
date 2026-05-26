# ==============================================================================
# GenesisEmu - Source Directory CMake Configuration (Updated)
# ==============================================================================
# This file compiles the emulator logic into a static library (Core) and links
# it to the main application executable alongside SDL2 (Outer Hexagon).
# Added 'Core/M68k/M68kCoreInstructions.cpp' to partition base core instructions.
# ==============================================================================

# ------------------------------------------------------------------------------
# 1. The Core Domain Library (The Inner Hexagon)
# ------------------------------------------------------------------------------
# Defines the static library containing only pure emulation logic.
# This target has ZERO hardware/OS dependencies.
# ------------------------------------------------------------------------------
add_library(GenesisEmuCore STATIC
    Core/MainBus.cpp
    Core/Vdp.cpp
    Core/VdpControlUnit.cpp
    Core/M68k.cpp
    Core/Cartridge.cpp
    Core/M68kDecoder.cpp
    Core/M68k/M68kArithmetic.cpp
    Core/M68k/M68kFlowControl.cpp
    Core/M68k/M68kCoreInstructions.cpp # Added Consolidated Core Instructions unit
)

# Export Core and M68k include directories so other targets can find the headers
target_include_directories(GenesisEmuCore PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/Core
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/M68k  
)

# ------------------------------------------------------------------------------
# 2. The Main Application Executable (The Outer Hexagon)
# ------------------------------------------------------------------------------
# Defines the final executable. It contains the main loop and the Adapters
# (SDL-based Graphics/Audio/Input).
# ------------------------------------------------------------------------------
add_executable(GenesisEmu
    App/main.cpp
    Adapters/SdlVideoAdapter.cpp
)

# Include directories for the App and Adapters
target_include_directories(GenesisEmu PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/Adapters
    ${CMAKE_CURRENT_SOURCE_DIR}/App
)

# Link the executable with:
# - Our pure emulator Core (GenesisEmuCore)
# - SDL2 (for GPU rendering and window management)
target_link_libraries(GenesisEmu PRIVATE 
    GenesisEmuCore
    ${SDL2_LIBRARIES}
)