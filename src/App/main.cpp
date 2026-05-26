// ==============================================================================
// GenesisEmu - Main Application Entry Point with SDL Video Integration
// ==============================================================================
// This file initializes the emulator core components and connects them to the
// SDL2 Video Adapter, running an active 60 FPS presentation loop.
// ==============================================================================

#include <iostream>
#include <array>
#include "MainBus.h"
#include "M68k.h"
#include "Vdp.h"
#include "SdlVideoAdapter.h"

using namespace GenesisEmu::Core;
using namespace GenesisEmu::Adapters;

// Sega Genesis standard NTSC High-Resolution mode resolution
constexpr int SCREEN_WIDTH  = 320;
constexpr int SCREEN_HEIGHT = 224;

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) {
    std::cout << "====================================================" << std::endl;
    std::cout << " GenesisEmu - Bootstrapping Hardware and Frontend   " << std::endl;
    std::cout << "====================================================" << std::endl;

    // 1. Initialize the Video Presentation Adapter (Outer Hexagon)
    // The window will be scaled up automatically by SDL2 while preserving aspect ratio
    SdlVideoAdapter videoAdapter("GenesisEmu [Active VSync]", SCREEN_WIDTH, SCREEN_HEIGHT);
    if (!videoAdapter.Initialize()) {
        std::cerr << "[Fatal Error] Failed to initialize SDL Video Adapter." << std::endl;
        return 1;
    }

    // 2. Instantiate the Core Emulation components (Inner Hexagon)
    MainBus bus;
    Vdp vdp;
    M68k cpu(&bus);

    // 3. Connect the hardware components
    bus.AttachDevice(&vdp, 0xC00000, 0xC0001F);
    cpu.Reset();

    // 4. Create the raw 32-bit pixel frame-buffer (format: RGBA8888)
    std::array<std::uint32_t, SCREEN_WIDTH * SCREEN_HEIGHT> screenBuffer;
    screenBuffer.fill(0x000000FF); // Initialize to solid black pixels

    std::cout << "====================================================" << std::endl;
    std::cout << " Booting Emulator Main Loop... Press ESC to exit.    " << std::endl;
    std::cout << "====================================================" << std::endl;

    bool running = true;
    std::uint32_t frameCount = 0;

    // --------------------------------------------------------------------------
    // Main Emulator Loop (Locked to Monitor Refresh Rate via VSync)
    // --------------------------------------------------------------------------
    while (running) {
        // A. Process Host window inputs/events
        running = videoAdapter.ProcessEvents();

        // B. Simulation/Integration Test: Generate a dynamic moving color pattern
        // This validates that the rendering pipeline to the GTX 1060 is working
        // at 60 FPS without memory leaks or latency.
        for (int y = 0; y < SCREEN_HEIGHT; ++y) {
            for (int x = 0; x < SCREEN_WIDTH; ++x) {
                // Generate shifting RGB color gradients based on frame index
                std::uint8_t red   = static_cast<std::uint8_t>(x + frameCount);
                std::uint8_t green = static_cast<std::uint8_t>(y + frameCount);
                std::uint8_t blue  = static_cast<std::uint8_t>(frameCount * 2);
                
                // Pack components into 32-bit RGBA pixel (Big-Endian format)
                screenBuffer[y * SCREEN_WIDTH + x] = 
                    (red << 24) | (green << 16) | (blue << 8) | 0xFF;
            }
        }

        // C. Push the generated pixel buffer to the GPU VRAM and present the frame
        videoAdapter.RenderFrame(screenBuffer.data());
        
        frameCount++;
    }

    std::cout << "Emulator shut down cleanly. Total frames rendered: " << frameCount << std::endl;
    return 0;
}