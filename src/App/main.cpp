// ==============================================================================
// GenesisEmu - Main Entry Point with VDP Tile Rendering Integration
// ==============================================================================
// This file boots the emulator, injects a custom binary 8x8 Space Invader sprite
// into VRAM through the MainBus, and renders it bouncing in real-time at 60 FPS.
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

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) {
    std::cout << "====================================================" << std::endl;
    std::cout << " GenesisEmu - Bootstrapping Hardware and VDP Renderer" << std::endl;
    std::cout << "====================================================" << std::endl;

    // 1. Initialize the Video Presentation Adapter (Outer Hexagon)
    SdlVideoAdapter videoAdapter("GenesisEmu [Active VSync Tile Renderer]", SCREEN_WIDTH, SCREEN_HEIGHT);
    if (!videoAdapter.Initialize()) {
        std::cerr << "[Fatal Error] Failed to initialize SDL Video Adapter." << std::endl;
        return 1;
    }

    // 2. Instantiate the central Bus and Core components
    MainBus bus;
    Vdp vdp;
    M68k cpu(&bus);

    // 3. Connect hardware components
    bus.AttachDevice(&vdp, 0xC00000, 0xC0001F);
    cpu.Reset();

    // --------------------------------------------------------------------------
    // VRAM Sprite Injection (Integrating Bus -> VDP Pipelines)
    // --------------------------------------------------------------------------
    // We will inject a custom 8x8 Space Invader sprite into VRAM address $0000.
    // Each pixel is 4 bits (nibble). We use color 5 (Magenta) and 7 (White/Eyes).
    // --------------------------------------------------------------------------
    std::cout << "[VDP] Injecting custom Space Invader sprite into VRAM..." << std::endl;

    // A. Set Auto-increment register ($8F) to 2
    bus.WriteWord(0xC00004, 0x8F02);
    
    // B. Set VDP write address to VRAM $0000
    bus.WriteWord(0xC00004, 0x4000);
    bus.WriteWord(0xC00004, 0x0000);

    // C. Write 32 bytes (16 Words) representing the Space Invader sprite
    bus.WriteWord(0xC00000, 0x0055); bus.WriteWord(0xC00000, 0x5500); // Row 0: . . M M M M . .
    bus.WriteWord(0xC00000, 0x0555); bus.WriteWord(0xC00000, 0x5550); // Row 1: . M M M M M M .
    bus.WriteWord(0xC00000, 0x5505); bus.WriteWord(0xC00000, 0x5055); // Row 2: M M . M M . M M
    bus.WriteWord(0xC00000, 0x5555); bus.WriteWord(0xC00000, 0x5555); // Row 3: M M M M M M M M
    bus.WriteWord(0xC00000, 0x5055); bus.WriteWord(0xC00000, 0x5505); // Row 4: M M . M M . M M
    bus.WriteWord(0xC00000, 0x5005); bus.WriteWord(0xC00000, 0x5005); // Row 5: M . . M M . . M
    bus.WriteWord(0xC00000, 0x0575); bus.WriteWord(0xC00000, 0x5750); // Row 6: . M W M M W M . (White eyes!)
    bus.WriteWord(0xC00000, 0x0050); bus.WriteWord(0xC00000, 0x0500); // Row 7: . . M . . M . .

    // 4. Create the raw 32-bit pixel frame-buffer (format: RGBA8888)
    std::array<std::uint32_t, SCREEN_WIDTH * SCREEN_HEIGHT> screenBuffer;

    std::cout << "====================================================" << std::endl;
    std::cout << " Booting Emulator Bouncing Sprite Demo! Press ESC... " << std::endl;
    std::cout << "====================================================" << std::endl;

    bool running = true;
    std::uint32_t frameCount = 0;

    // Sprite physics state
    int posX = 150;
    int posY = 100;
    int velX = 2;
    int velY = 2;

    // --------------------------------------------------------------------------
    // Main Emulator Loop (Locked to Monitor Refresh Rate via VSync)
    // --------------------------------------------------------------------------
    while (running) {
        // A. Process Host window inputs/events
        running = videoAdapter.ProcessEvents();

        // B. Clear the framebuffer with a nice dark retro-blue background
        screenBuffer.fill(0x0B1D3AFF);

        // C. Update Bouncing Sprite physics
        posX += velX;
        posY += velY;

        // Boundary checks (stretches sprite collision at 8x8 pixels)
        if (posX <= 0 || posX >= SCREEN_WIDTH - 8) {
            velX = -velX;
        }
        if (posY <= 0 || posY >= SCREEN_HEIGHT - 8) {
            velY = -velY;
        }

        // D. Draw our standard Sega 8x8 Tile (Tile Index 0) from VRAM onto the framebuffer
        VdpRenderer::RenderTile(vdp, 0, posX, posY, SCREEN_WIDTH, screenBuffer.data());

        // E. Present the compiled frame onto the monitor via our GPU Video Adapter
        videoAdapter.RenderFrame(screenBuffer.data());
        
        frameCount++;
    }

    std::cout << "Demo shut down cleanly. Total frames rendered: " << frameCount << std::endl;
    return 0;
}