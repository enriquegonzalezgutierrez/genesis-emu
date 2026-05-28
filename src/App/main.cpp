// ==============================================================================
// GenesisEmu - Application Bootstrapper and Sync Loop (Application Layer)
// ==============================================================================
// This file initializes the virtual console motherboard, connects physical
// peripherals to the MainBus, and runs the cycle-accurate execution loop.
//
// Synchronization Model (Scanline-Sync):
// Instead of executing a full frame and rendering at the end, the system now
// synchronizes the CPU and VDP on a strict line-by-line basis. This allows
// raster effects (mid-screen palette/scroll changes) to work accurately.
// ==============================================================================

#include <iostream>
#include <array>
#include <chrono>
#include <thread>
#include "../Core/Domain/Bus/MainBus.h"
#include "../Core/Domain/M68k/M68k.h"
#include "../Core/Domain/Vdp/Vdp.h"
#include "../Core/Domain/Cartridge/Cartridge.h"
#include "../Core/Domain/Cartridge/SegaMapperDevice.h" 
#include "../Core/Domain/WorkRAM/WorkRAM.h"
#include "../Core/Domain/Io/IoPorts.h"
#include "../Adapters/SdlVideoAdapter.h"
#include "../Adapters/RomLoaderAdapter.h"

using namespace GenesisEmu::Core::Domain::Common; 
using namespace GenesisEmu::Core::Domain::Bus;
using namespace GenesisEmu::Core::Domain::M68k;
using namespace GenesisEmu::Core::Domain::Vdp;
using namespace GenesisEmu::Core::Domain::Cartridge;
using namespace GenesisEmu::Core::Domain::WorkRAM;
using namespace GenesisEmu::Core::Domain::Io;
using namespace GenesisEmu::Adapters;

// Standard NTSC Sega Genesis display resolution
constexpr int SCREEN_WIDTH  = 320;
constexpr int SCREEN_HEIGHT = 224;
constexpr int WINDOW_SCALE  = 2; // Scaled up for better visibility on modern monitors

// Hardware Clock Timings (NTSC)
// Total lines per frame including blanking periods: 262
// Total CPU cycles per frame: ~127,840 (7.67 MHz CPU)
// Cycles per scanline: 127840 / 262 = ~488 cycles
constexpr int TOTAL_SCANLINES_NTSC = 262;
constexpr int CYCLES_PER_SCANLINE  = 488;

int main(int argc, char* argv[]) {
    std::cout << "====================================================" << std::endl;
    std::cout << " GenesisEmu - Cycle-Accurate Core Console Engine     " << std::endl;
    std::cout << "====================================================" << std::endl;

    std::string romPath = "roms/sonic.bin"; 
    if (argc > 1) {
        romPath = argv[1];
    }

    // 1. Load ROM via the Host Adapter (Outer Hexagon)
    auto romData = RomLoaderAdapter::LoadFile(romPath);
    if (romData.empty()) {
        std::cerr << "[Fatal Error] ROM loader returned an empty buffer: " << romPath << std::endl;
        return 1;
    }

    Cartridge cartridge;
    if (!cartridge.LoadROM(romData)) {
        std::cerr << "[Fatal Error] ROM header parsing failed. Invalid Sega format." << std::endl;
        return 1;
    }

    std::cout << "[Cartridge] Title:  " << cartridge.GetGameTitle() << std::endl;
    std::cout << "[Cartridge] Serial: " << cartridge.GetSerialCode() << std::endl;

    // 2. Initialize Video & Input Presentation Adapter
    SdlVideoAdapter videoAdapter("GenesisEmu [Scanline-Sync Mode]", SCREEN_WIDTH, SCREEN_HEIGHT, WINDOW_SCALE);
    if (!videoAdapter.Initialize()) {
        std::cerr << "[Fatal Error] Failed to initialize SDL Video Adapter." << std::endl;
        return 1;
    }

    // 3. Assemble the Console Motherboard (Inner Hexagon)
    MainBus  bus;
    M68k     cpu(&bus);
    Vdp      vdp(&bus); 
    WorkRAM  wram;
    IoPorts  ioPorts;
    SegaMapperDevice mapperDevice(&cartridge);

    // Map hardware to the standard 24-bit physical address space
    bus.AttachDevice(&cartridge,    0x000000, 0x3FFFFF);
    bus.AttachDevice(&ioPorts,      0xA10000, 0xA1001F);
    bus.AttachDevice(&mapperDevice, 0xA13000, 0xA130FF); 
    bus.AttachDevice(&vdp,          0xC00000, 0xC0001F);
    bus.AttachDevice(&wram,         0xE00000, 0xFFFFFF);

    // Hard reset the CPU to read the vector table
    cpu.Reset();

    // Host Framebuffer (RGBA8888 32-bit format)
    std::array<std::uint32_t, SCREEN_WIDTH * SCREEN_HEIGHT> screenBuffer;
    screenBuffer.fill(0x000000FF); // Clear to black

    bool running = true;
    auto frameStartTime = std::chrono::steady_clock::now();

    std::cout << "====================================================" << std::endl;
    std::cout << " Motherboard synchronizations active. Controls: Arrow Keys + Z/X/C" << std::endl;
    std::cout << "====================================================" << std::endl;

    // 4. Main Execution Sync-Loop
    while (running) {
        running = videoAdapter.ProcessEvents(ioPorts);

        // Process exactly one full NTSC frame (262 scanlines)
        for (int scanline = 0; scanline < TOTAL_SCANLINES_NTSC; ++scanline) {
            int cyclesExecutedThisLine = 0;

            // --- A. Processor Execution Phase ---
            // Execute the CPU until the cycle budget for this specific scanline is exhausted.
            while (cyclesExecutedThisLine < CYCLES_PER_SCANLINE) {
                int consumed = 0;

                // Hardware feature: If the VDP is executing a DMA copy, the 68000 CPU is frozen.
                // Cycles are consumed entirely by the VDP transfer.
                if (vdp.IsDmaActive()) {
                    consumed = vdp.ProcessDma(CYCLES_PER_SCANLINE - cyclesExecutedThisLine);
                } 
                else {
                    // Normal CPU execution
                    consumed = cpu.Step();
                }

                cyclesExecutedThisLine += consumed;

                // Update the controller multiplexer timeout (1.5ms for 6-button pads)
                // Time passes regardless of whether the CPU is running or frozen by DMA.
                ioPorts.UpdateTimers(consumed);
            }

            // Update VDP internal timing mechanism to reflect line completion
            vdp.SetFrameCycles(scanline * CYCLES_PER_SCANLINE);

            // --- B. VDP Rendering Phase ---
            // Render the screen line-by-line as the beam travels down the CRT.
            // This is vital for Raster Effects (like water in Sonic) to work.
            if (scanline < SCREEN_HEIGHT) {
                vdp.RenderScanline(scanline, screenBuffer.data());
            }

            // --- C. Hardware Interrupts Dispatching ---
            
            // H-Int triggers during active display and the first blanking line (up to line 224)
            if (scanline <= SCREEN_HEIGHT) {
                if (vdp.DecrementHintCounter()) {
                    cpu.TriggerInterrupt(4); // Level 4: H-Blank Interrupt
                }
            }

            // V-Int triggers exactly when the beam hits the first Vertical Blanking line
            if (scanline == SCREEN_HEIGHT) {
                vdp.SetVblankActive(true);
                cpu.TriggerInterrupt(6); // Level 6: V-Blank Interrupt
                
                // Present the completely rasterized frame to the host GPU
                videoAdapter.RenderFrame(screenBuffer.data());
            }

            // End of V-Blank interval
            if (scanline == TOTAL_SCANLINES_NTSC - 1) {
                vdp.SetVblankActive(false);
            }
        }

        // --- 60Hz Frame Rate Clamping ---
        // Prevents the emulator from running infinitely fast on modern hardware
        frameStartTime += std::chrono::microseconds(16666); // ~16.6ms per frame
        std::this_thread::sleep_until(frameStartTime);
    }

    std::cout << "System shutting down. Goodbye." << std::endl;
    return 0;
}