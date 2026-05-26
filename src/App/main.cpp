// ==============================================================================
// GenesisEmu - Main Entry Point with Scaled ROM Graphics Explorer
// ==============================================================================
// This file implements a real-time ROM Scanner. It reads raw game graphics
// from Final Fight MD, injects it dynamically into VRAM, and renders the tiles
// upscaled with hardware acceleration (Nearest-Neighbor).
// ==============================================================================

#include <iostream>
#include <array>
#include "MainBus.h"
#include "M68k.h"
#include "Vdp.h"
#include "Cartridge.h"
#include "SdlVideoAdapter.h"
#include "RomLoaderAdapter.h"
#include "VdpRenderer.h"

using namespace GenesisEmu::Core;
using namespace GenesisEmu::Adapters;

// Sega Genesis standard NTSC High-Resolution mode resolution
constexpr int SCREEN_WIDTH  = 320;
constexpr int SCREEN_HEIGHT = 224;

// Integer Scaling Factor (e.g., 4x scales the window to 1280x896)
constexpr int WINDOW_SCALE = 4;

// Grid calculations: 8x8 pixels per tile
constexpr int TILE_SIZE   = 8;
constexpr int GRID_COLS   = SCREEN_WIDTH / TILE_SIZE;  // 40 columns
constexpr int GRID_ROWS   = SCREEN_HEIGHT / TILE_SIZE; // 28 rows
constexpr int TOTAL_TILES = GRID_COLS * GRID_ROWS;     // 1120 tiles on screen

// ------------------------------------------------------------------------------
// VRAM Dynamic Graphic Injector
// ------------------------------------------------------------------------------
void InjectRomGraphicsToVram(MainBus& bus, Cartridge& cartridge, Address romOffset) {
    bus.WriteWord(0xC00004, 0x8F02); // Auto-increment 2
    bus.WriteWord(0xC00004, 0x4000); // VRAM Write Setup (Word 1)
    bus.WriteWord(0xC00004, 0x0000); // VRAM Write Setup (Word 2 - Address $0000)

    size_t totalBytesToInject = TOTAL_TILES * 32; 
    for (size_t i = 0; i < totalBytesToInject; i += 2) {
        Word wordData = cartridge.ReadWord(romOffset + i);
        bus.WriteWord(0xC00000, wordData);
    }
}

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) {
    std::cout << "====================================================" << std::endl;
    std::cout << " GenesisEmu - Scaled ROM Graphics Explorer         " << std::endl;
    std::cout << "====================================================" << std::endl;

    std::string romPath = "roms/final_fight_md.bin";
    auto romData = RomLoaderAdapter::LoadFile(romPath);
    if (romData.empty()) {
        std::cerr << "[Fatal Error] Could not load ROM: " << romPath << std::endl;
        return 1;
    }

    Cartridge cartridge;
    if (!cartridge.LoadROM(romData)) {
        std::cerr << "[Fatal Error] Invalid Sega ROM format." << std::endl;
        return 1;
    }

    // 1. Initialize the Video Presentation Adapter with logical dimensions and Window Scale
    SdlVideoAdapter videoAdapter("GenesisEmu [4x Scaled ROM Explorer]", SCREEN_WIDTH, SCREEN_HEIGHT, WINDOW_SCALE);
    if (!videoAdapter.Initialize()) {
        std::cerr << "[Fatal Error] Failed to initialize SDL Video Adapter." << std::endl;
        return 1;
    }

    MainBus bus;
    Vdp vdp;
    M68k cpu(&bus);

    bus.AttachDevice(&cartridge, 0x000000, 0x3FFFFF);
    bus.AttachDevice(&vdp, 0xC00000, 0xC0001F);
    cpu.Reset();

    Address romGraphicsOffset = 0x20000; 
    std::cout << "[Scanner] Initializing ROM Graphics scan at offset: 0x" 
              << std::hex << std::uppercase << romGraphicsOffset << std::dec << std::endl;

    InjectRomGraphicsToVram(bus, cartridge, romGraphicsOffset);

    // Frame-buffer representing the internal 320x224 emulated screen
    std::array<std::uint32_t, SCREEN_WIDTH * SCREEN_HEIGHT> screenBuffer;

    std::cout << "====================================================" << std::endl;
    std::cout << " Scanner Active! Use UP/DOWN Arrows to scroll ROM.  " << std::endl;
    std::cout << "====================================================" << std::endl;

    bool running = true;

    // --------------------------------------------------------------------------
    // Main Emulator Loop
    // --------------------------------------------------------------------------
    while (running) {
        int offsetChange = 0;
        running = videoAdapter.ProcessEvents(offsetChange);

        if (offsetChange != 0) {
            Address proposedOffset = romGraphicsOffset + offsetChange;
            size_t totalBytesToInject = TOTAL_TILES * 32;

            if (offsetChange < 0 && romGraphicsOffset < static_cast<Address>(abs(offsetChange))) {
                romGraphicsOffset = 0; 
            } else if (proposedOffset + totalBytesToInject <= cartridge.GetROMSize()) {
                romGraphicsOffset = proposedOffset;
            }

            InjectRomGraphicsToVram(bus, cartridge, romGraphicsOffset);

            std::cout << "[Scanner] ROM Offset updated to: 0x" 
                      << std::hex << std::uppercase << romGraphicsOffset 
                      << " (" << std::dec << (romGraphicsOffset / 1024) << " KB)" << std::endl;
        }

        screenBuffer.fill(0x000000FF);

        int tileIndex = 0;
        for (int row = 0; row < GRID_ROWS; ++row) {
            for (int col = 0; col < GRID_COLS; ++col) {
                VdpRenderer::RenderTile(
                    vdp, tileIndex, 
                    col * TILE_SIZE, row * TILE_SIZE, 
                    SCREEN_WIDTH, screenBuffer.data()
                );
                tileIndex++;
            }
        }

        // Upload the 320x224 buffer to the GPU. The Video Adapter will automatically
        // scale it 4x using nearest-neighbor logic.
        videoAdapter.RenderFrame(screenBuffer.data());
    }

    std::cout << "Scanner shut down cleanly." << std::endl;
    return 0;
}