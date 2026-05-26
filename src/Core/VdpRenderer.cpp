// ==============================================================================
// GenesisEmu - VDP Consolidated Graphics Renderer Implementation (Core Domain)
// ==============================================================================
// This file implements the real-time background planes scanline rendering and 
// Sega BGR to RGBA conversion algorithms.
// ==============================================================================

#include "VdpRenderer.h"

namespace GenesisEmu::Core {

// ------------------------------------------------------------------------------
// BGR to RGBA Color Converter
// ------------------------------------------------------------------------------
std::uint32_t VdpRenderer::ConvertColor(Byte cramHigh, Byte cramLow) {
    // Sega Genesis color format: 0000 BBB0 GGG0 RRR0 (3 bits per channel)
    // Extract 3-bit values (range 0 to 7)
    Byte r3 = (cramLow & 0x0E) >> 1;
    Byte g3 = (cramLow & 0xE0) >> 5;
    Byte b3 = (cramHigh & 0x0E) >> 1;

    // Scale 3-bit channels to standard 8-bit channels (range 0 to 255)
    Byte r8 = r3 * 36; 
    Byte g8 = g3 * 36;
    Byte b8 = b3 * 36;

    // Return as 32-bit RGBA (Most Significant Byte first: R, G, B, A)
    return (static_cast<std::uint32_t>(r8) << 24) |
           (static_cast<std::uint32_t>(g8) << 16) |
           (static_cast<std::uint32_t>(b8) << 8)  |
           0x000000FF; // Alpha channel fully opaque
}

// ------------------------------------------------------------------------------
// Plane Scanline Renderer Engine
// ------------------------------------------------------------------------------
void VdpRenderer::RenderPlaneScanline(const Vdp& vdp, int planeIndex, int scanline, 
                                    int screenWidth, std::uint32_t* lineBuffer) {
    
    // 1. Resolve Nametable base addresses from VDP registers
    // Reg #02: Plane A Nametable address base (Bits 5-3 multiplied by 0x400)
    // Reg #04: Plane B Nametable address base (Bits 2-0 multiplied by 0x2000)
    Address planeBaseVramAddress = 0;
    if (planeIndex == 0) {
        planeBaseVramAddress = (vdp.GetRegister(2) & 0x38) << 10;
    } else {
        planeBaseVramAddress = (vdp.GetRegister(4) & 0x07) << 13;
    }

    // 2. Parse Nametable dimensions from Register 16 ($90)
    // Bits 1-0: Plane Width (00 = 32 tiles, 01 = 64 tiles, 11 = 128 tiles)
    // Bits 5-4: Plane Height (same encoding)
    int planeWidthTiles = 64; // Default standard size
    Byte widthBits = vdp.GetRegister(16) & 0x03;
    if (widthBits == 0x00) planeWidthTiles = 32;
    else if (widthBits == 0x03) planeWidthTiles = 128;

    // 3. Compute Row index for the current scanline (8 vertical pixels per tile)
    int tileRowIndex = scanline / 8;
    int pixelRowOffset = scanline % 8;

    // 4. Render 40 columns (320 pixels wide)
    for (int colPixel = 0; colPixel < screenWidth; ++colPixel) {
        int tileColIndex = colPixel / 8;
        int pixelColOffset = colPixel % 8;

        // Calculate offset of the 2-byte (1 Word) tile descriptor in Nametable
        Address descriptorAddress = planeBaseVramAddress + 
                                    ((tileRowIndex * planeWidthTiles) + tileColIndex) * 2;

        // Read Descriptor Word (Big-Endian format)
        Byte descHigh = vdp.ReadVramDirect(descriptorAddress);
        Byte descLow  = vdp.ReadVramDirect(descriptorAddress + 1);
        Word descriptor = (static_cast<Word>(descHigh) << 8) | descLow;

        // Extract description parameters:
        // Bit 15: Priority
        // Bits 14-13: Palette line selection (0 to 3)
        // Bit 12: V-Flip
        // Bit 11: H-Flip
        // Bits 10-0: Tile pattern index inside VRAM
        Byte paletteLine  = (descriptor >> 13) & 0x03;
        bool hFlip         = (descriptor & 0x0800) != 0;
        bool vFlip         = (descriptor & 0x1000) != 0;
        Word tileIndex     = descriptor & 0x07FF;

        // 5. Calculate physical pixel offset inside the 8x8 tile pattern
        int targetPixelRow = vFlip ? (7 - pixelRowOffset) : pixelRowOffset;
        int targetPixelCol = hFlip ? (7 - pixelColOffset) : pixelColOffset;

        // Each tile is 32 bytes. Calculate base address of selected tile.
        Address tileBaseVram = tileIndex * 32;
        Address rowVramAddress = tileBaseVram + (targetPixelRow * 4);

        // Each byte in the row contains 2 pixels (upper and lower 4-bit nibbles)
        Byte pixelPair = vdp.ReadVramDirect(rowVramAddress + (targetPixelCol / 2));
        Byte colorIndex = 0;

        if (targetPixelCol % 2 == 0) {
            colorIndex = (pixelPair >> 4) & 0x0F; // Left pixel (upper nibble)
        } else {
            colorIndex = pixelPair & 0x0F;        // Right pixel (lower nibble)
        }

        // Color index 0 is always transparent (allows lower layers to show through)
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
// Sprites Scanline Renderer (Stub for Priority Layer Composition)
// ------------------------------------------------------------------------------
void VdpRenderer::RenderSpritesScanline(const Vdp& vdp, int scanline, 
                                      int screenWidth, std::uint32_t* lineBuffer) {
    // Will be fully populated in subsequent steps to overlay sprite layers
    (void)vdp;
    (void)scanline;
    (void)screenWidth;
    (void)lineBuffer;
}

} // namespace GenesisEmu::Core