// ==============================================================================
// GenesisEmu - VDP Graphics Renderer Implementation (Core Domain)
// ==============================================================================
// This file implements Sega BGR scaling and background plane scanline composition.
//
// SOLID Compliance:
// 1. Single Responsibility Principle (SRP):
//    All calculations are pure, stateless, and focused on graphics compiling.
// ==============================================================================

#include "VdpRenderer.h"

namespace GenesisEmu::Core::Domain::Vdp {

using namespace GenesisEmu::Core::Domain::Common;

// ------------------------------------------------------------------------------
// BGR to RGBA Color Converter
// ------------------------------------------------------------------------------
std::uint32_t VdpRenderer::ConvertColor(Byte cramHigh, Byte cramLow) {
    // Sega original color layout: 0000 BBB0 GGG0 RRR0 (3 bits per color channel)
    // Extract 3-bit values (range 0 to 7)
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
    
    // 1. Resolve Nametable base addresses from configuration registers
    // Reg #02: Plane A base address (Bits 5-3 multiplied by 0x400)
    // Reg #04: Plane B base address (Bits 2-0 multiplied by 0x2000)
    Address planeBaseVramAddress = 0;
    if (planeIndex == 0) {
        planeBaseVramAddress = (vdp.GetRegister(2) & 0x38) << 10;
    } else {
        planeBaseVramAddress = (vdp.GetRegister(4) & 0x07) << 13;
    }

    // 2. Parse Nametable dimensions from Register 16 ($90)
    // Bits 1-0: Plane Width (00 = 32 tiles, 01 = 64 tiles, 11 = 128 tiles)
    int planeWidthTiles = 64; // Default safe standard fallback
    Byte widthBits = vdp.GetRegister(16) & 0x03;
    if (widthBits == 0x00) planeWidthTiles = 32;
    else if (widthBits == 0x03) planeWidthTiles = 128;

    // 3. Compute row and pixel row offsets inside the active tile
    int tileRowIndex = scanline / 8;
    int pixelRowOffset = scanline % 8;

    // 4. Render 40 columns (320 pixels wide)
    for (int colPixel = 0; colPixel < screenWidth; ++colPixel) {
        int tileColIndex = colPixel / 8;
        int pixelColOffset = colPixel % 8;

        // Calculate offset of the 2-byte (1 Word) tile descriptor in Nametable
        Address descriptorAddress = planeBaseVramAddress + 
                                    ((tileRowIndex * planeWidthTiles) + tileColIndex) * 2;

        // Fetch Descriptor Word from VRAM (Big-Endian format)
        Byte descHigh = vdp.ReadVramDirect(descriptorAddress);
        Byte descLow  = vdp.ReadVramDirect(descriptorAddress + 1);
        Word descriptor = (static_cast<Word>(descHigh) << 8) | descLow;

        // Decode descriptor configurations:
        // Bit 15: Priority
        // Bits 14-13: Palette line selection (0 to 3)
        // Bit 12: V-Flip (Vertical Mirroring)
        // Bit 11: H-Flip (Horizontal Mirroring)
        // Bits 10-0: Tile pattern index inside VRAM
        Byte paletteLine  = (descriptor >> 13) & 0x03;
        bool hFlip         = (descriptor & 0x0800) != 0;
        bool vFlip         = (descriptor & 0x1000) != 0;
        Word tileIndex     = descriptor & 0x07FF;

        // 5. Calculate physical pixel offset inside the 8x8 tile pattern
        int targetPixelRow = vFlip ? (7 - pixelRowOffset) : pixelRowOffset;
        int targetPixelCol = hFlip ? (7 - pixelColOffset) : pixelColOffset;

        // Each tile pattern is exactly 32 bytes (8 rows * 4 bytes).
        Address tileBaseVram = tileIndex * 32;
        Address rowVramAddress = tileBaseVram + (targetPixelRow * 4);

        // Each byte in the row contains 2 pixels (4 bits per pixel, upper and lower nibbles)
        Byte pixelPair = vdp.ReadVramDirect(rowVramAddress + (targetPixelCol / 2));
        Byte colorIndex = 0;

        if (targetPixelCol % 2 == 0) {
            colorIndex = (pixelPair >> 4) & 0x0F; // Left pixel (upper nibble)
        } else {
            colorIndex = pixelPair & 0x0F;        // Right pixel (lower nibble)
        }

        // Color index 0 represents standard transparency (allows layers underneath to show)
        if (colorIndex != 0) {
            // Retrieve palette color from CRAM. Each palette line is 16 colors (32 bytes).
            Address cramAddress = (paletteLine * 32) + (colorIndex * 2);
            Byte colorHigh = vdp.ReadCramDirect(cramAddress);
            Byte colorLow  = vdp.ReadCramDirect(cramAddress + 1);

            lineBuffer[colPixel] = ConvertColor(colorHigh, colorLow);
        }
    }
}

// ------------------------------------------------------------------------------
// Sprites Scanline Renderer (Priority Composition Stub)
// ------------------------------------------------------------------------------
void VdpRenderer::RenderSpritesScanline(const Vdp& vdp, int scanline, 
                                      int screenWidth, std::uint32_t* lineBuffer) {
    (void)vdp;
    (void)scanline;
    (void)screenWidth;
    (void)lineBuffer;
}

} // namespace GenesisEmu::Core::Domain::Vdp