// ==============================================================================
// GenesisEmu - Main Application Entry Point with ROM Loading
// ==============================================================================
// This file orchestrates the complete initialization: loading a real game ROM,
// parsing its header metadata, attaching it to the memory-mapped bus, and
// running the interactive 60 FPS hardware loop.
// ==============================================================================

#include <iostream>
#include <array>
#include "MainBus.h"
#include "M68k.h"
#include "Vdp.h"
#include "Cartridge.h"
#include "SdlVideoAdapter.h"
#include "RomLoaderAdapter.h"

using namespace GenesisEmu::Core;
using namespace GenesisEmu::Adapters;

// Sega Genesis standard NTSC High-Resolution mode resolution
constexpr int SCREEN_WIDTH  = 320;
constexpr int SCREEN_HEIGHT = 224;

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) {
    std::cout << "====================================================" << std::endl;
    std::cout << " GenesisEmu - Bootstrapping Emulator and ROM Loader " << std::endl;
    std::cout << "====================================================" << std::endl;

    // 1. Define the ROM path (Default path inside /roms directory)
    // Note: If your unzipped file has a different name, rename it to 'final_fight_md.bin'
    // or modify this string to match the exact filename.
    std::string romPath = "roms/final_fight_md.bin";

    // 2. Load the raw binary ROM from the host disk using our Adapter (Outer Hexagon)
    auto romData = RomLoaderAdapter::LoadFile(romPath);
    if (romData.empty()) {
        std::cerr << "[Fatal Error] Could not load ROM. Please ensure your unzipped game binary "
                  << "is located at: " << romPath << std::endl;
        return 1;
    }

    // 3. Create the Cartridge Entity and load the ROM data (Inner Hexagon)
    Cartridge cartridge;
    if (!cartridge.LoadROM(romData)) {
        std::cerr << "[Fatal Error] Loaded file is too small to be a valid Sega Genesis ROM." << std::endl;
        return 1;
    }

    // Print parsed ROM metadata directly from the game binary's header
    std::cout << "----------------------------------------------------" << std::endl;
    std::cout << " ROM Header Metadata Parsed Successfully!" << std::endl;
    std::cout << " Game Title: " << cartridge.GetGameTitle() << std::endl;
    std::cout << " Serial No:  " << cartridge.GetSerialCode() << std::endl;
    std::cout << " Checksum:   0x" << std::hex << std::uppercase << cartridge.GetChecksum() << std::endl;
    std::cout << " ROM Size:   " << std::dec << (cartridge.GetROMSize() / 1024) << " KB" << std::endl;
    std::cout << "----------------------------------------------------" << std::endl;

    // 4. Initialize the Video Presentation Adapter (Outer Hexagon)
    SdlVideoAdapter videoAdapter("GenesisEmu [Active VSync]", SCREEN_WIDTH, SCREEN_HEIGHT);
    if (!videoAdapter.Initialize()) {
        std::cerr << "[Fatal Error] Failed to initialize SDL Video Adapter." << std::endl;
        return 1;
    }

    // 5. Instantiate the central Bus and CPU
    MainBus bus;
    Vdp vdp;
    M68k cpu(&bus);

    // 6. Map the physical hardware components to the central Bus (Mediator)
    // - Map Cartridge ROM to range $000000 - $3FFFFF (Up to 4MB of ROM space)
    bus.AttachDevice(&cartridge, 0x000000, 0x3FFFFF);
    // - Map VDP ports to range $C00000 - $C0001F
    bus.AttachDevice(&vdp, 0xC00000, 0xC0001F);
    
    std::cout << "[Bus] Successfully mapped Cartridge ($000000) and VDP ($C00000)" << std::endl;

    // 7. Perform a hardware RESET sequence on the CPU
    // The CPU will now read its initial SSP and PC vectors directly from our loaded Cartridge!
    std::cout << "[CPU] Executing hardware RESET sequence..." << std::endl;
    cpu.Reset();

    // 8. Create the raw 32-bit pixel frame-buffer (format: RGBA8888)
    std::array<std::uint32_t, SCREEN_WIDTH * SCREEN_HEIGHT> screenBuffer;
    screenBuffer.fill(0x000000FF); // Initialize with solid black

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

        // B. Simulation: Generate dynamic moving color pattern
        // (In future phases, the VDP rendering engine will fill this buffer)
        for (int y = 0; y < SCREEN_HEIGHT; ++y) {
            for (int x = 0; x < SCREEN_WIDTH; ++x) {
                std::uint8_t red   = static_cast<std::uint8_t>(x + frameCount);
                std::uint8_t green = static_cast<std::uint8_t>(y + frameCount);
                std::uint8_t blue  = static_cast<std::uint8_t>(frameCount * 2);
                
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