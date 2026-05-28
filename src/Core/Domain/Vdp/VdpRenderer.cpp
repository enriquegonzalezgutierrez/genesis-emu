// ==============================================================================
// GenesisEmu - VDP Graphics Renderer Implementation (Core Domain)
// ==============================================================================
// This file implements Sega BGR scaling, dynamic background plane scanline 
// composition, and a highly accurate 16-bit scanline-based sprite rasterizer.
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

    // Perfect scaling from 3-bit (0-7) to 8-bit (0-255)
    Byte r8 = (r3 * 255) / 7; 
    Byte g8 = (g3 * 255) / 7;
    Byte b8 = (b3 * 255) / 7;

    // Return as a 32-bit RGBA8888 color (Red, Green, Blue, Alpha)
    return (static_cast<std::uint32_t>(r8) << 24) |
           (static_cast<std::uint32_t>(g8) << 16) |
           (static_cast<std::uint32_t>(b8) << 8)  |
           0x000000FF; // Fully opaque alpha channel
}

// ------------------------------------------------------------------------------
// Plane Scanline Renderer Engine (With Dynamic Scrolling Support)
// ------------------------------------------------------------------------------
void VdpRenderer::RenderPlaneScanline(const Vdp& vdp, int planeIndex, int scanline, 
                                    int screenWidth, std::uint32_t* lineBuffer) {
    // 1. Get Plane VRAM Base Address
    Address planeBaseVramAddress = 0;
    if (planeIndex == 0) {
        planeBaseVramAddress = (vdp.GetRegister(2) & 0x38) << 10;
    } else {
        planeBaseVramAddress = (vdp.GetRegister(4) & 0x07) << 13;
    }

    // 2. Get Plane Dimensions (Width/Height in tiles) from Register 16
    Byte sizeBits = vdp.GetRegister(16);
    int planeWidthTiles = (sizeBits & 0x03) == 0 ? 32 : ((sizeBits & 0x03) == 1 ? 64 : 128);
    int planeHeightTiles = ((sizeBits >> 4) & 0x03) == 0 ? 32 : (((sizeBits >> 4) & 0x03) == 1 ? 64 : 128);

    int planeWidthPixels  = planeWidthTiles * 8;
    int planeHeightPixels = planeHeightTiles * 8;

    // 3. Horizontal Scroll Analysis (Reg 11 bits 1-0)
    Byte hScrollMode = vdp.GetRegister(11) & 0x03;
    Address hScrollBase = (vdp.GetRegister(13) & 0x3F) << 10;
    
    // 4. Vertical Scroll Analysis (Reg 11 bit 2)
    bool is2CellVScroll = (vdp.GetRegister(11) & 0x04) != 0;

    // Render Scanline Pixels
    for (int colPixel = 0; colPixel < screenWidth; ++colPixel) {
        
        // --- Calculate H-Scroll Offset for this pixel ---
        Address hScrollAddr = hScrollBase;
        if (hScrollMode == 0x00 || hScrollMode == 0x01) {
            // Full Screen Scroll (1 offset per plane)
            hScrollAddr += (planeIndex * 2);
        } else if (hScrollMode == 0x02) {
            // Block/Cell Scroll (8-pixel horizontal bands)
            // Each 8-pixel band has a 4-byte descriptor (2 bytes per plane)
            hScrollAddr += ((scanline / 8) * 4) + (planeIndex * 2); // FIXED: 4 bytes offset per block
        } else if (hScrollMode == 0x03) {
            // Line Scroll (Individual offset per scanline)
            hScrollAddr += (scanline * 4) + (planeIndex * 2);
        }

        std::int16_t hScroll = static_cast<std::int16_t>((vdp.ReadVramDirect(hScrollAddr) << 8) | 
                                                         vdp.ReadVramDirect(hScrollAddr + 1));

        // --- Calculate V-Scroll Offset for this pixel ---
        Address vScrollAddr = planeIndex * 2;
        if (is2CellVScroll) {
            // 2-Cell V-Scroll uses a different vertical offset every 16 pixels
            vScrollAddr += ((colPixel / 16) * 4);
        }
        
        std::int16_t vScroll = static_cast<std::int16_t>((vdp.ReadVsramDirect(vScrollAddr) << 8) | 
                                                         vdp.ReadVsramDirect(vScrollAddr + 1));

        // --- Calculate Virtual Scrolled Coordinates ---
        // Sega hardware subtracts horizontal scroll but adds vertical scroll
        int scrolledX = (colPixel - hScroll) & (planeWidthPixels - 1);
        int scrolledY = (scanline + vScroll) & (planeHeightPixels - 1);

        int tileColIndex = scrolledX / 8;
        int tileRowIndex = scrolledY / 8;
        int pixelColOffset = scrolledX % 8;
        int pixelRowOffset = scrolledY % 8;

        // Fetch Tile Descriptor (Nametable)
        Address descriptorAddress = planeBaseVramAddress + ((tileRowIndex * planeWidthTiles) + tileColIndex) * 2;
        Word descriptor = (vdp.ReadVramDirect(descriptorAddress) << 8) | vdp.ReadVramDirect(descriptorAddress + 1);

        // Decode Tile Attributes
        Byte paletteLine  = (descriptor >> 13) & 0x03;
        bool hFlip         = (descriptor & 0x0800) != 0;
        bool vFlip         = (descriptor & 0x1000) != 0;
        Word tileIndex     = descriptor & 0x07FF;

        int targetPixelRow = vFlip ? (7 - pixelRowOffset) : pixelRowOffset;
        int targetPixelCol = hFlip ? (7 - pixelColOffset) : pixelColOffset;

        // Fetch Pixel Color Data
        Address tileBaseVram = tileIndex * 32;
        Address rowVramAddress = tileBaseVram + (targetPixelRow * 4);

        Byte pixelPair = vdp.ReadVramDirect(rowVramAddress + (targetPixelCol / 2));
        Byte colorIndex = (targetPixelCol % 2 == 0) ? ((pixelPair >> 4) & 0x0F) : (pixelPair & 0x0F);

        // Color index 0 is transparent (let backdrop or lower plane show through)
        if (colorIndex != 0) {
            Address cramAddress = (paletteLine * 32) + (colorIndex * 2);
            Byte colorHigh = vdp.ReadCramDirect(cramAddress);
            Byte colorLow  = vdp.ReadCramDirect(cramAddress + 1);

            lineBuffer[colPixel] = ConvertColor(colorHigh, colorLow);
        }
    }
}

// ------------------------------------------------------------------------------
// Sprites Scanline Renderer Engine (With Hardware Limits & Masking)
// ------------------------------------------------------------------------------
void VdpRenderer::RenderSpritesScanline(const Vdp& vdp, int scanline, 
                                      int screenWidth, std::uint32_t* lineBuffer) {
    // VDP Reg 5: Sprite Attribute Table (SAT) base address
    Address spriteBaseVram = (vdp.GetRegister(5) & 0x7F) << 9;

    // Detect Display Mode (H40 vs H32) to set hardware limits
    bool isH40 = (vdp.GetRegister(12) & 0x81) != 0;
    int maxSpritesPerLine = isH40 ? 20 : 16;
    int maxPixelsPerLine  = isH40 ? 320 : 256;

    int spritesDrawnThisLine = 0;
    int pixelsDrawnThisLine = 0;
    bool spriteMaskingActive = false;

    // The Sega Genesis uses a linked list for sprites, starting at index 0.
    Byte spriteIndex = 0;
    
    // Safety check: The hardware evaluates a maximum of 80 sprites.
    for (int s = 0; s < 80; ++s) {
        Address addr = spriteBaseVram + (spriteIndex * 8);

        // Read Y coordinate (Strict 9-bit mask to prevent corruption)
        Word rawY = (vdp.ReadVramDirect(addr) << 8) | vdp.ReadVramDirect(addr + 1);
        int yPos = (rawY & 0x01FF) - 128; 

        Byte size = vdp.ReadVramDirect(addr + 2);
        Byte link = vdp.ReadVramDirect(addr + 3);

        Word descriptor = (vdp.ReadVramDirect(addr + 4) << 8) | vdp.ReadVramDirect(addr + 5);
        
        // Read X coordinate
        Word rawX = (vdp.ReadVramDirect(addr + 6) << 8) | vdp.ReadVramDirect(addr + 7);
        int xPos = (rawX & 0x01FF) - 128;

        // Decode sprite dimensions in tile units (bits 3-2 width, bits 1-0 height)
        int spriteWidthTiles  = ((size >> 2) & 0x03) + 1;
        int spriteHeightTiles = (size & 0x03) + 1;

        int spriteHeightPixels = spriteHeightTiles * 8;
        int spriteWidthPixels  = spriteWidthTiles * 8;

        // Check if the sprite intersects the current raster scanline
        if (scanline >= yPos && scanline < (yPos + spriteHeightPixels)) {
            
            spritesDrawnThisLine++;
            if (spritesDrawnThisLine > maxSpritesPerLine) break; // Hardware sprite limit reached

            // Hardware Quirk: Sprite Masking. 
            // If the raw X coordinate is 0, this sprite is not drawn AND it masks
            // (disables) all subsequent lower-priority sprites on this scanline.
            if ((rawX & 0x01FF) == 0) {
                spriteMaskingActive = true;
            }

            if (spriteMaskingActive) {
                // Advance to the next sprite without drawing
                if (link == 0) break;
                spriteIndex = link;
                continue;
            }

            Byte paletteLine = (descriptor >> 13) & 0x03;
            bool hFlip        = (descriptor & 0x0800) != 0;
            bool vFlip        = (descriptor & 0x1000) != 0;
            Word baseTile     = descriptor & 0x07FF;

            int pixelRowOffset = scanline - yPos;
            int targetPixelRow = vFlip ? (spriteHeightPixels - 1 - pixelRowOffset) : pixelRowOffset;

            int tileRowIndex = targetPixelRow / 8;
            int tilePixelRow = targetPixelRow % 8;

            // Draw horizontal pixels for this sprite
            for (int x = 0; x < spriteWidthPixels; ++x) {
                int screenX = xPos + x;
                
                // Hardware pixel limit reached for this scanline (Sprite Drop-out)
                if (pixelsDrawnThisLine >= maxPixelsPerLine) break;

                // Transparent areas of sprites DO NOT count towards the pixel limit,
                // but we increment the counter if it's an opaque pixel.
                
                if (screenX >= 0 && screenX < screenWidth) {
                    int targetPixelCol = hFlip ? (spriteWidthPixels - 1 - x) : x;
                    int tileColIndex = targetPixelCol / 8;
                    int tilePixelCol = targetPixelCol % 8;

                    // Genesis hardware layout: sprites are drawn vertically column by column
                    Word tileOffset = (tileColIndex * spriteHeightTiles) + tileRowIndex;
                    Word tileIndex = baseTile + tileOffset;

                    Address rowVramAddress = (tileIndex * 32) + (tilePixelRow * 4);
                    Byte pixelPair = vdp.ReadVramDirect(rowVramAddress + (tilePixelCol / 2));
                    Byte colorIndex = (tilePixelCol % 2 == 0) ? ((pixelPair >> 4) & 0x0F) : (pixelPair & 0x0F);

                    if (colorIndex != 0) {
                        pixelsDrawnThisLine++; // Opaque pixel counts towards the limit
                        
                        // Prevent sprites from overwriting each other on the same line
                        // (Since we process higher priority sprites first)
                        if (lineBuffer[screenX] == 0) {
                            Address cramAddress = (paletteLine * 32) + (colorIndex * 2);
                            Byte colorHigh = vdp.ReadCramDirect(cramAddress);
                            Byte colorLow  = vdp.ReadCramDirect(cramAddress + 1);

                            lineBuffer[screenX] = ConvertColor(colorHigh, colorLow);
                        }
                    }
                }
            }
        }

        // The SAT link of 0 explicitly marks the end of the sprite list
        if (link == 0) break;
        spriteIndex = link;
    }
}

} // namespace GenesisEmu::Core::Domain::Vdp