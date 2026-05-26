// ==============================================================================
// GenesisEmu - Real-Time System Frame Loop Entry Point (App Layer - Updated)
// ==============================================================================
// This file initializes the motherboard bus, registers memories and ports, 
// and executes instructions inside a real-time cycle-sync frame loop.
// Stops CPU execution cleanly on Diagnostic Halt without freezing the SDL window.
// ==============================================================================

#include <iostream>
#include <array>
#include <chrono>
#include <thread>
#include "MainBus.h"
#include "M68k.h"
#include "Vdp.h"
#include "Cartridge.h"
#include "WorkRAM.h"
#include "IoPorts.h"
#include "SdlVideoAdapter.h"
#include "RomLoaderAdapter.h"
#include "VdpRenderer.h"

using namespace GenesisEmu::Core;
using namespace GenesisEmu::Adapters;

// Sega Genesis standard NTSC High-Resolution mode parameters
constexpr int SCREEN_WIDTH  = 320;
constexpr int SCREEN_HEIGHT = 224;
constexpr int WINDOW_SCALE = 4;

// Standard NTSC clock sync parameters
// 7.67 MHz CPU Clock => ~127,840 CPU clock cycles per 60Hz video frame
constexpr int CYCLES_PER_FRAME = 127840; 

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) {
    std::cout << "====================================================" << std::endl;
    std::cout << " GenesisEmu - Real-Time Core Console Engine          " << std::endl;
    std::cout << "====================================================" << std::endl;

    // 1. Load the ROM file
    std::string romPath = "roms/final_fight_md.bin";
    auto romData = RomLoaderAdapter::LoadFile(romPath);
    if (romData.empty()) {
        std::cerr << "[Fatal Error] Could not load ROM: " << romPath << std::endl;
        return 1;
    }

    // 2. Instantiate and load the game cartridge
    Cartridge cartridge;
    if (!cartridge.LoadROM(romData)) {
        std::cerr << "[Fatal Error] Invalid Sega ROM format." << std::endl;
        return 1;
    }

    std::cout << "[Cartridge] Loaded: " << cartridge.GetGameTitle() << std::endl;
    std::cout << "[Cartridge] Serial: " << cartridge.GetSerialCode() << std::endl;

    // 3. Initialize physical presentation window
    SdlVideoAdapter videoAdapter("GenesisEmu [Real-Time Game Mode]", SCREEN_WIDTH, SCREEN_HEIGHT, WINDOW_SCALE);
    if (!videoAdapter.Initialize()) {
        std::cerr << "[Fatal Error] Failed to initialize SDL Video Adapter." << std::endl;
        return 1;
    }

    // 4. Instantiate motherboard devices
    MainBus  bus;
    M68k     cpu(&bus);
    Vdp      vdp;
    WorkRAM  wram;
    IoPorts  ioPorts;

    // 5. Connect devices to MainBus (Memory Map Routing)
    bus.AttachDevice(&cartridge, 0x000000, 0x3FFFFF);
    bus.AttachDevice(&ioPorts, 0xA10000, 0xA1001F);
    bus.AttachDevice(&vdp, 0xC00000, 0xC0001F);
    bus.AttachDevice(&wram, 0xE00000, 0xFFFFFF);

    // 6. Reset CPU
    cpu.Reset();
    std::cout << "[CPU] Reset complete. Stack Pointer (A7): 0x" 
              << std::hex << std::uppercase << cpu.GetARegister(7)
              << ", Program Counter (PC): 0x" << cpu.GetPC() << std::dec << std::endl;

    // Framebuffer representing the internal 320x224 emulated screen
    std::array<std::uint32_t, SCREEN_WIDTH * SCREEN_HEIGHT> screenBuffer;
    screenBuffer.fill(0x000000FF);

    bool running = true;

    std::cout << "====================================================" << std::endl;
    std::cout << " Engine active! Play using Arrow Keys + Z / X / C. " << std::endl;
    std::cout << "====================================================" << std::endl;

    // --------------------------------------------------------------------------
    // Real-Time System Frame Loop
    // --------------------------------------------------------------------------
    while (running) {
        // Poll window close events and update gamepad states
        running = videoAdapter.ProcessEvents(ioPorts);

        // Execute instruction clock cycle budget ONLY if CPU is not halted
        if (!cpu.IsHalted()) {
            int currentFrameCycles = 0;
            while (currentFrameCycles < CYCLES_PER_FRAME) {
                int consumedCycles = cpu.Step();
                currentFrameCycles += consumedCycles;
                
                // If a step triggered a diagnostic halt, exit the instruction loop immediately
                if (cpu.IsHalted()) {
                    break;
                }
            }
        }

        // Render current background planes even if halted (allows inspectable output)
        for (int scanline = 0; scanline < SCREEN_HEIGHT; ++scanline) {
            VdpRenderer::RenderPlaneScanline(
                vdp, 0, scanline, SCREEN_WIDTH, 
                &screenBuffer[scanline * SCREEN_WIDTH]
            );
        }

        // Output to GPU window
        videoAdapter.RenderFrame(screenBuffer.data());
    }

    std::cout << "System shutting down. Goodbye." << std::endl;
    return 0;
}