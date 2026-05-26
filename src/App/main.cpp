// ==============================================================================
// GenesisEmu - Main Entry Point with Scaled Plane Graphics Explorer
// ==============================================================================
// This file implements a real-time ROM Scanner using the newly decoupled
// VDP Scanline Composition Engine. It sequentially maps tiles to Plane A
// and renders the composited scene upscaled with hardware acceleration.
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
// VRAM Dynamic Graphic Injector (Patterns + Nametable Layout)
// ------------------------------------------------------------------------------
void InjectRomGraphicsToVram(MainBus& bus, Cartridge& cartridge, Address romOffset) {
    // 1. Configure VDP Auto-increment to 2
    bus.WriteWord(0xC00004, 0x8F02);

    // 2. Load 1120 tile patterns into VRAM starting at address $0000
    bus.WriteWord(0xC00004, 0x4000); // VRAM Write Setup (Word 1)
    bus.WriteWord(0xC00004, 0x0000); // VRAM Write Setup (Word 2)

    size_t totalBytesToInject = TOTAL_TILES * 32; 
    for (size_t i = 0; i < totalBytesToInject; i += 2) {
        Word wordData = cartridge.ReadWord(romOffset + i);
        bus.WriteWord(0xC00000, wordData);
    }

    // 3. Write sequential tile descriptors to the Plane A Nametable at $E000
    // Each descriptor is a Word (Palette line 0, Priority 0, Tile index i)
    bus.WriteWord(0xC00004, 0x4000 | (0xE000 & 0x3FFF));
    bus.WriteWord(0xC00004, 0x0000 | (0xE000 >> 14));

    // Plane A is 64 tiles wide (Register 16 configuration). We write 40 sequential
    // descriptors per row, and skip the remaining 24 invisible columns to keep alignment.
    for (int r = 0; r < GRID_ROWS; ++r) {
        for (int c = 0; c < GRID_COLS; ++c) { // Corrected loop counter variables
            Word descriptor = static_cast<Word>((r * GRID_COLS) + c);
            bus.WriteWord(0xC00000, descriptor);
        }
        // Write dummy transparent descriptors for the out-of-screen nametable margin (24 columns)
        for (int margin = 0; margin < (64 - GRID_COLS); ++margin) {
            bus.WriteWord(0xC00000, 0x0000);
        }
    }
}

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) {
    std::cout << "====================================================" << std::endl;
    std::cout << " GenesisEmu - Scaled Plane Graphics Explorer       " << std::endl;
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

    SdlVideoAdapter videoAdapter("GenesisEmu [4x Scaled Plane Explorer]", SCREEN_WIDTH, SCREEN_HEIGHT, WINDOW_SCALE);
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

    // Configure VDP Register 2 (Plane A nametable base) to 0x38 (VRAM address $E000)
    bus.WriteWord(0xC00004, 0x8238);

    // Configure VDP Register 16 (Plane Nametable dimensions) to 0x01 (Width: 64 tiles, Height: 32 tiles)
    bus.WriteWord(0xC00004, 0x9001);

    Address romGraphicsOffset = 0x20000; 
    std::cout << "[Scanner] Initializing ROM Graphics scan at offset: 0x" 
              << std::hex << std::uppercase << romGraphicsOffset << std::dec << std::endl;

    InjectRomGraphicsToVram(bus, cartridge, romGraphicsOffset);

    // Framebuffer representing the internal 320x224 emulated screen
    std::array<std::uint32_t, SCREEN_WIDTH * SCREEN_HEIGHT> screenBuffer;

    std::cout << "====================================================" << std::endl;
    std::cout << " Scanner Active! Use UP/DOWN Arrows to scroll ROM.  " << std::endl;
    std::cout << "====================================================" << std::endl;

    bool running = true;

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

        // Fill background with solid black
        screenBuffer.fill(0x000000FF);

        // Render Scroll Plane A scanline-by-scanline
        for (int scanline = 0; scanline < SCREEN_HEIGHT; ++scanline) {
            VdpRenderer::RenderPlaneScanline(
                vdp, 0, scanline, SCREEN_WIDTH, 
                &screenBuffer[scanline * SCREEN_WIDTH]
            );
        }

        videoAdapter.RenderFrame(screenBuffer.data());
    }

    std::cout << "Scanner shut down cleanly." << std::endl;
    return 0;
}