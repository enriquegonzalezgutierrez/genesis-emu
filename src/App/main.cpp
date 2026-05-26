// ==============================================================================
// GenesisEmu - Real-Time System Frame Loop Entry Point (App Layer - Diagnostics)
// ==============================================================================
// This file initializes the motherboard bus, registers memories and ports, 
// and executes instructions inside a real-time cycle-sync frame loop.
// Upgraded with a real-time Telemetry Monitor to print system status every 1s.
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

    // Telemetry tracking variables
    int frameCount = 0;
    int instructionsThisSecond = 0;
    auto lastDiagnosticTime = std::chrono::steady_clock::now();

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
                instructionsThisSecond++;
                
                // If a step triggered a diagnostic halt, exit the instruction loop immediately
                if (cpu.IsHalted()) {
                    break;
                }
            }
        }

        // Render current background planes
        for (int scanline = 0; scanline < SCREEN_HEIGHT; ++scanline) {
            std::uint32_t planeBLine[SCREEN_WIDTH] = {0};
            std::uint32_t planeALine[SCREEN_WIDTH] = {0};

            // Render Plane B (Background scenario layer)
            VdpRenderer::RenderPlaneScanline(vdp, 1, scanline, SCREEN_WIDTH, planeBLine);
            
            // Render Plane A (Foreground UI/active scenario layer)
            VdpRenderer::RenderPlaneScanline(vdp, 0, scanline, SCREEN_WIDTH, planeALine);

            // Blend the layers with transparency priority logic
            for (int x = 0; x < SCREEN_WIDTH; ++x) {
                int pixelIndex = scanline * SCREEN_WIDTH + x;
                
                if (planeALine[x] != 0) {
                    screenBuffer[pixelIndex] = planeALine[x];
                } else if (planeBLine[x] != 0) {
                    screenBuffer[pixelIndex] = planeBLine[x];
                } else {
                    screenBuffer[pixelIndex] = 0x000000FF; // Fallback to opaque black
                }
            }
        }

        // Output to GPU window
        videoAdapter.RenderFrame(screenBuffer.data());
        frameCount++;

        // --- Real-Time Telemetry Monitor (Fires every 1000ms / 1s) ---
        auto currentTime = std::chrono::steady_clock::now();
        auto elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - lastDiagnosticTime).count();
        if (elapsedTime >= 1000) {
            // Count active non-zero VRAM bytes
            int nonZeroVram = 0;
            for (int i = 0; i < 0x10000; ++i) {
                if (vdp.ReadVramDirect(i) != 0) nonZeroVram++;
            }
            
            // Count active non-zero CRAM bytes
            int nonZeroCram = 0;
            for (int i = 0; i < 128; ++i) {
                if (vdp.ReadCramDirect(i) != 0) nonZeroCram++;
            }

            std::cout << "\n--- [REAL-TIME ENGINE DIAGNOSTICS] ---" << std::endl;
            std::cout << "Presentation Speed:  " << frameCount << " FPS" << std::endl;
            std::cout << "Core Execution Speed:" << instructionsThisSecond << " Instructions/sec" << std::endl;
            std::cout << "CPU State:           PC=0x" << std::hex << std::uppercase << cpu.GetPC() 
                      << "  SP=0x" << cpu.GetARegister(7) << "  SR=0x" << cpu.GetSR() << std::dec << std::endl;
            std::cout << "VDP Register 2 (PlA):0x" << std::hex << (int)vdp.GetRegister(2) 
                      << " (Addr: 0x" << ((vdp.GetRegister(2) & 0x38) << 10) << ")" << std::dec << std::endl;
            std::cout << "VDP Register 4 (PlB):0x" << std::hex << (int)vdp.GetRegister(4) 
                      << " (Addr: 0x" << ((vdp.GetRegister(4) & 0x07) << 13) << ")" << std::dec << std::endl;
            std::cout << "VDP Register 15 (Inc):" << (int)vdp.GetRegister(15) << std::endl;
            std::cout << "VDP Target Address:  0x" << std::hex << vdp.GetTargetAddress() << std::dec << std::endl;
            std::cout << "VRAM Filled Bytes:   " << nonZeroVram << " / 65536 bytes" << std::endl;
            std::cout << "CRAM Active Colors:  " << (nonZeroCram / 2) << " / 64 colors" << std::endl;
            std::cout << "--------------------------------------\n" << std::endl;

            // Reset diagnostics counters
            frameCount = 0;
            instructionsThisSecond = 0;
            lastDiagnosticTime = currentTime;
        }
    }

    std::cout << "System shutting down. Goodbye." << std::endl;
    return 0;
}