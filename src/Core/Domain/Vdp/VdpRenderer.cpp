// ==============================================================================
// GenesisEmu - VDP Graphics Renderer Implementation (Core Domain)
// ==============================================================================
// This file implements Sega BGR scaling and background plane scanline composition.
// Includes a high-performance 16-bit scanline-based sprite rasterizer.
// ==============================================================================

#include "VdpRenderer.h"

namespace GenesisEmu::Core::Domain::Vdp {

using namespace GenesisEmu::Core::Domain::Common;

// ------------------------------------------------------------------------------
// BGR to RGBA Color Converter
// ------------------------------------------------------------------------------
std::uint32_t VdpRenderer::ConvertColor(Byte cramHigh, Byte cramLow) {
    // Sega original color layout: 0000 BBB0 GGG0 RRR0 (3 bits per color channel)
    Byte r3 = (cramLow & 0x0E) >> 1;
    Byte g3 = (cramLow & 0xE0) >> 5;
    Byte b3 = (cramHigh & 0x0E) >> 1;

    // Scale 3-bit values to standard 8-bit color channels (range 0 to 252)
    Byte r8 = r3 * 36; 
    Byte g8 = g3 * 36;
    Byte b8 = b3 * 36;

    // Return as a 32-bit RGBA8888 color (Red, Green, Blue, Alpha)
    return (static_cast<std::uint32_t>(r8) << 24) |
           (static_cast<std::uint32_t>(g8) << 16) |
           (static_cast<std::uint32_t>(b8) << 8)  |
           0x000000FF; // Fully opaque alpha channel
}

// ------------------------------------------------------------------------------
// Plane Scanline Renderer Engine
// ------------------------------------------------------------------------------
void VdpRenderer::RenderPlaneScanline(const Vdp& vdp, int planeIndex, int scanline, 
                                    int screenWidth, std::uint32_t* lineBuffer) {
    Address planeBaseVramAddress = 0;
    if (planeIndex == 0) {
        planeBaseVramAddress = (vdp.GetRegister(2) & 0x38) << 10;
    } else {
        planeBaseVramAddress = (vdp.GetRegister(4) & 0x07) << 13;
    }

    int planeWidthTiles = 64; 
    Byte widthBits = vdp.GetRegister(16) & 0x03;
    if (widthBits == 0x00) planeWidthTiles = 32;
    else if (widthBits == 0x03) planeWidthTiles = 128;

    int tileRowIndex = scanline / 8;
    int pixelRowOffset = scanline % 8;

    for (int colPixel = 0; colPixel < screenWidth; ++colPixel) {
        int tileColIndex = colPixel / 8;
        int pixelColOffset = colPixel % 8;

        Address descriptorAddress = planeBaseVramAddress + 
                                    ((tileRowIndex * planeWidthTiles) + tileColIndex) * 2;

        Byte descHigh = vdp.ReadVramDirect(descriptorAddress);
        Byte descLow  = vdp.ReadVramDirect(descriptorAddress + 1);
        Word descriptor = (static_cast<Word>(descHigh) << 8) | descLow;

        Byte paletteLine  = (descriptor >> 13) & 0x03;
        bool hFlip         = (descriptor & 0x0800) != 0;
        bool vFlip         = (descriptor & 0x1000) != 0;
        Word tileIndex     = descriptor & 0x07FF;

        int targetPixelRow = vFlip ? (7 - pixelRowOffset) : pixelRowOffset;
        int targetPixelCol = hFlip ? (7 - pixelColOffset) : pixelColOffset;

        Address tileBaseVram = tileIndex * 32;
        Address rowVramAddress = tileBaseVram + (targetPixelRow * 4);

        Byte pixelPair = vdp.ReadVramDirect(rowVramAddress + (targetPixelCol / 2));
        Byte colorIndex = 0;

        if (targetPixelCol % 2 == 0) {
            colorIndex = (pixelPair >> 4) & 0x0F; 
        } else {
            colorIndex = pixelPair & 0x0F;        
        }

        if (colorIndex != 0) {
            Address cramAddress = (paletteLine * 32) + (colorIndex * 2);
            Byte colorHigh = vdp.ReadCramDirect(cramAddress);
            Byte colorLow  = vdp.ReadCramDirect(cramAddress + 1);

            lineBuffer[colPixel] = ConvertColor(colorHigh, colorLow);
        }
    }
}

// ------------------------------------------------------------------------------
// Sprites Scanline Renderer Engine (16-bit hardware-level parsing)
// ==============================================================================
// traverses the linked Sprite Attribute Table (SAT) in VRAM, extracting coordinates,
// sizes, flips, and rendering pixels sequentially Column-by-Column.
// ------------------------------------------------------------------------------
void VdpRenderer::RenderSpritesScanline(const Vdp& vdp, int scanline, 
                                      int screenWidth, std::uint32_t* lineBuffer) {
    // VDP Reg 5: Sprite Attribute Table (SAT) base address in VRAM (Shifted left by 9)
    Address spriteBaseVram = (vdp.GetRegister(5) & 0x7F) << 9;

    // Sega Genesis supports a linked list of up to 80 sprites.
    // We follow the 'link' index of each sprite descriptor block.
    Byte spriteIndex = 0;
    
    for (int s = 0; s < 80; ++s) {
        Address addr = spriteBaseVram + (spriteIndex * 8);

        // Read Y coordinate (offset by 128 pixels in Genesis hardware)
        Word yPos = (vdp.ReadVramDirect(addr) << 8) | vdp.ReadVramDirect(addr + 1);
        yPos = (yPos & 0x03FF) - 128; 

        Byte size = vdp.ReadVramDirect(addr + 2);
        Byte link = vdp.ReadVramDirect(addr + 3);

        Word descriptor = (vdp.ReadVramDirect(addr + 4) << 8) | vdp.ReadVramDirect(addr + 5);
        
        // Read X coordinate (offset by 128 pixels)
        Word xPos = (vdp.ReadVramDirect(addr + 6) << 8) | vdp.ReadVramDirect(addr + 7);
        xPos = (xPos & 0x03FF) - 128;

        // Decode sprite dimensions in tile units (bits 3-2 width, bits 1-0 height)
        int spriteWidthTiles  = ((size >> 2) & 0x03) + 1;
        int spriteHeightTiles = (size & 0x03) + 1;

        int spriteHeightPixels = spriteHeightTiles * 8;
        int spriteWidthPixels  = spriteWidthTiles * 8;

        // Render this sprite only if it intersects the active raster scanline row
        if (scanline >= yPos && scanline < (yPos + spriteHeightPixels)) {
            Byte paletteLine = (descriptor >> 13) & 0x03;
            bool hFlip        = (descriptor & 0x0800) != 0;
            bool vFlip        = (descriptor & 0x1000) != 0;
            Word baseTile     = descriptor & 0x07FF;

            int pixelRowOffset = scanline - yPos;
            int targetPixelRow = vFlip ? (spriteHeightPixels - 1 - pixelRowOffset) : pixelRowOffset;

            int tileRowIndex = targetPixelRow / 8;
            int tilePixelRow = targetPixelRow % 8;

            // Draw horizontal pixels
            for (int x = 0; x < spriteWidthPixels; ++x) {
                int screenX = xPos + x;
                
                if (screenX >= 0 && screenX < screenWidth) {
                    int targetPixelCol = hFlip ? (spriteWidthPixels - 1 - x) : x;
                    int tileColIndex = targetPixelCol / 8;
                    int tilePixelCol = targetPixelCol % 8;

                    // Sega hardware layouts multi-tile sprites Column-by-Column (vertically first)
                    Word tileOffset = (tileColIndex * spriteHeightTiles) + tileRowIndex;
                    Word tileIndex = baseTile + tileOffset;

                    Address tileBaseVram = tileIndex * 32;
                    Address rowVramAddress = tileBaseVram + (tilePixelRow * 4);

                    Byte pixelPair = vdp.ReadVramDirect(rowVramAddress + (tilePixelCol / 2));
                    Byte colorIndex = (tilePixelCol % 2 == 0) ? ((pixelPair >> 4) & 0x0F) : (pixelPair & 0x0F);

                    // Sprite Color index 0 is strictly transparent
                    if (colorIndex != 0) {
                        Address cramAddress = (paletteLine * 32) + (colorIndex * 2);
                        Byte colorHigh = vdp.ReadCramDirect(cramAddress);
                        Byte colorLow  = vdp.ReadCramDirect(cramAddress + 1);

                        lineBuffer[screenX] = ConvertColor(colorHigh, colorLow);
                    }
                }
            }
        }

        // Link index 0 indicates we reached the physical end of the active sprite chain
        if (link == 0) {
            break;
        }
        spriteIndex = link;
    }
}

} // namespace GenesisEmu::Core::Domain::Vdp