// ==============================================================================
// GenesisEmu - VDP and Graphics Renderer Unit Tests (Corrected with DDD Namespaces)
// ==============================================================================
// This file contains unit tests to verify control register writes, VRAM
// autoincrement, CRAM writes, and tile/plane scanline conversion.
// ==============================================================================

#include <gtest/gtest.h>
#include "Vdp.h"
#include "VdpRenderer.h"

using namespace GenesisEmu::Core::Domain::Common;
using namespace GenesisEmu::Core::Domain::Vdp;

// ------------------------------------------------------------------------------
// Test Suite: VdpBehaviorTests
// ------------------------------------------------------------------------------

TEST(VdpBehaviorTests, VdpRegisterWrite) {
    Vdp vdp;

    // Write value 0x02 to Register 15 (Auto-increment)
    vdp.WriteWord(0x04, 0x8F02);

    EXPECT_EQ(vdp.GetRegister(15), 0x02);
}

TEST(VdpBehaviorTests, VdpVramWriteAndAutoincrement) {
    Vdp vdp;
    vdp.WriteWord(0x04, 0x8F02);

    // Initiate 32-bit write to VRAM address $0000
    vdp.WriteWord(0x04, 0x4000); 
    vdp.WriteWord(0x04, 0x0000); 

    vdp.WriteWord(0x00, 0xABCD);

    EXPECT_EQ(vdp.ReadVramDirect(0x0000), 0xAB);
    EXPECT_EQ(vdp.ReadVramDirect(0x0001), 0xCD);
    EXPECT_EQ(vdp.GetTargetAddress(), 0x0002);
}

TEST(VdpBehaviorTests, VdpCramWriteAndColorConversion) {
    Vdp vdp;
    
    // Auto-increment 2
    vdp.WriteWord(0x04, 0x8F02);

    // Initiate 32-bit write to CRAM address $0000
    // Command code for CRAM write is 0x03 (CD1-CD0 in 1st word, CD5-CD2 in 2nd word)
    // 1st word: address 0, command bits 1-0 => $C000
    // 2nd word: address 0, command bits 5-2 => $0000
    vdp.WriteWord(0x04, 0xC000);
    vdp.WriteWord(0x04, 0x0000);

    // Write Sega Blue color format: 0000 BBB0 GGG0 RRR0
    // Pure blue: 0x0E00 => High Byte 0x0E (Blue=7), Low Byte 0x00 (Green=0, Red=0)
    vdp.WriteWord(0x00, 0x0E00);

    // Verify written values inside CRAM
    EXPECT_EQ(vdp.ReadCramDirect(0x0000), 0x0E);
    EXPECT_EQ(vdp.ReadCramDirect(0x0001), 0x00);

    // Verify 9-bit BGR to 32-bit RGBA scaling conversion
    // Blue=7 maps to Blue8 = 7 * 36 = 252 (0xFC). RGBA Output: 0x0000FCFF
    std::uint32_t colorRGBA = VdpRenderer::ConvertColor(0x0E, 0x00);
    EXPECT_EQ(colorRGBA, 0x0000FCFF);
}

TEST(VdpBehaviorTests, VdpRenderPlaneScanlineCalculatesOutput) {
    Vdp vdp;
    
    // Register 15 (Auto-increment 2)
    vdp.WriteWord(0x04, 0x8F02);

    // Set Register 2 (Plane A nametable base) to 0x38 (VRAM address $E000)
    // Formula: (Reg[2] & 0x38) << 10 => 0x38 * 1024 = 57344 ($E000)
    vdp.WriteWord(0x04, 0x8238);

    // Write a tile descriptor Word to Plane A nametable at column 0, row 0 ($E000)
    // Descriptor: Palette line 0, Tile index 1
    // Word: $0001
    vdp.WriteWord(0x04, 0x4000 | (0xE000 & 0x3FFF));
    vdp.WriteWord(0x04, 0x0000 | (0xE000 >> 14));
    vdp.WriteWord(0x00, 0x0001);

    // Write 4-bit pixel index pattern data for Tile 1 ($0020 to $003F)
    // Each row of 8 pixels is 4 bytes. Row 0: pixels 1,2,3,4 (color index 1)
    // Word: $1111, $1111 => 8 pixels of color index 1
    vdp.WriteWord(0x04, 0x4020);
    vdp.WriteWord(0x04, 0x0000);
    vdp.WriteWord(0x00, 0x1111);
    vdp.WriteWord(0x00, 0x1111);

    // Write color 1 of palette line 0 inside CRAM ($0002)
    // Color: Red 0x000E => High Byte 0x00, Low Byte 0x0E (Red=7, Green=0, Blue=0)
    vdp.WriteWord(0x04, 0xC002);
    vdp.WriteWord(0x04, 0x0000);
    vdp.WriteWord(0x00, 0x000E);

    // Define target line buffer
    std::uint32_t screenLine[320] = {0};

    // Render scanline 0 (targets Tile Row 0, pixel row 0)
    VdpRenderer::RenderPlaneScanline(vdp, 0, 0, 320, screenLine);

    // Verify first 8 pixels (of tile 1) contain the compiled Red color (0xFC0000FF)
    for (int i = 0; i < 8; ++i) {
        EXPECT_EQ(screenLine[i], 0xFC0000FF);
    }

    // Verify outside pixels remain transparent (0x00000000)
    EXPECT_EQ(screenLine[8], 0x00000000);
}