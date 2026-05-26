// ==============================================================================
// GenesisEmu - VDP Pattern/Tile Renderer (Core Domain)
// ==============================================================================
// This utility class decodes Sega Genesis VDP standard 8x8 pixel tiles (patterns)
// from VRAM and renders them onto the 32-bit RGBA host framebuffer.
// ==============================================================================

#pragma once

#include "Vdp.h"
#include <cstdint>

namespace GenesisEmu::Core {

class VdpRenderer {
public:
    VdpRenderer() = delete;
    ~VdpRenderer() = delete;

    // --------------------------------------------------------------------------
    // 8x8 Tile Rendering Engine
    // --------------------------------------------------------------------------
    // Reads a single 8x8 tile from VRAM and draws it onto the screen buffer.
    // - vdp: Reference to the VDP containing the VRAM.
    // - tileIndex: The index of the tile in VRAM (each tile is 32 bytes).
    // - posX, posY: Pixel coordinates on the 320x224 screen where the tile starts.
    // - screenWidth: Width of the destination screen buffer (320).
    // - screenBuffer: Pointer to the 32-bit RGBA pixel array.
    // --------------------------------------------------------------------------
    static void RenderTile(const Vdp& vdp, Address tileIndex, int posX, int posY, 
                           int screenWidth, std::uint32_t* screenBuffer) {
        
        // Each Sega Genesis tile occupies exactly 32 bytes of VRAM
        Address tileVramAddress = tileIndex * 32;

        // Simple hardcoded fallback palette (16 colors) for testing.
        // In the future, these colors will be read directly from CRAM.
        static const std::uint32_t fallbackPalette[16] = {
            0x000000FF, // 0: Transparent/Black
            0xFF0000FF, // 1: Pure Red
            0x00FF00FF, // 2: Pure Green
            0x0000FFFF, // 3: Pure Blue
            0xFFFF00FF, // 4: Yellow
            0xFF00FFFF, // 5: Magenta
            0x00FFFFFF, // 6: Cyan
            0xFFFFFFFF, // 7: White
            0x888888FF, // 8: Dark Gray
            0x444444FF, // 9: Light Gray
            0xFF8800FF, // 10: Orange
            0x880088FF, // 11: Purple
            0x008888FF, // 12: Teal
            0x888800FF, // 13: Olive
            0xDD5555FF, // 14: Salmon pink
            0x55DDDDFF  // 15: Soft cyan
        };

        // Render the 8x8 grid of pixels
        for (int row = 0; row < 8; ++row) {
            // Each row of 8 pixels is stored in 4 bytes (32 bits) of VRAM
            // Since each pixel is 4 bits (nibble), 1 byte contains 2 pixels.
            Address rowAddress = tileVramAddress + (row * 4);

            for (int col = 0; col < 8; col += 2) {
                // Read 1 byte from VRAM (contains 2 pixels: left and right)
                Byte pixelPair = vdp.ReadVramDirect(rowAddress + (col / 2));

                // Extract left pixel (upper 4 bits)
                Byte leftPixelColorIndex = (pixelPair >> 4) & 0x0F;
                // Extract right pixel (lower 4 bits)
                Byte rightPixelColorIndex = pixelPair & 0x0F;

                // Calculate screen buffer indices
                int screenX_Left  = posX + col;
                int screenX_Right = posX + col + 1;
                int screenY       = posY + row;

                // Draw the left pixel if within screen boundaries
                if (screenX_Left < screenWidth && screenY < 224) {
                    screenBuffer[screenY * screenWidth + screenX_Left] = 
                        fallbackPalette[leftPixelColorIndex];
                }

                // Draw the right pixel if within screen boundaries
                if (screenX_Right < screenWidth && screenY < 224) {
                    screenBuffer[screenY * screenWidth + screenX_Right] = 
                        fallbackPalette[rightPixelColorIndex];
                }
            }
        }
    }
};

} // namespace GenesisEmu::Core