// ==============================================================================
// GenesisEmu - VDP and Graphics Renderer Unit Tests (Core Domain)
// ==============================================================================
// This file contains unit tests to verify control register writes, VRAM
// autoincrement, 16-bit word alignment enforcement, and scanline conversion.
// ==============================================================================

#include <gtest/gtest.h>
#include <fstream>
#include <vector>
#include "Vdp.h"
#include "VdpRenderer.h"
#include "WorkRAM.h"
#include "MainBus.h"
#include "M68k.h"
#include "Cartridge.h"
#include "IoPorts.h"

using namespace GenesisEmu::Core::Domain::Common;
using namespace GenesisEmu::Core::Domain::Vdp;
using namespace GenesisEmu::Core::Domain::WorkRAM;
using namespace GenesisEmu::Core::Domain::Bus;
using namespace GenesisEmu::Core::Domain::M68k;
using namespace GenesisEmu::Core::Domain::Cartridge;
using namespace GenesisEmu::Core::Domain::Io;

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

TEST(VdpBehaviorTests, VdpVramWriteOddAddressSwapsBytes) {
    Vdp vdp;
    vdp.WriteWord(0x04, 0x8F02); // Auto-increment 2

    // Initiate 32-bit write to ODD VRAM address $0001
    // First word: address bits 13-0 (0x0001) | command bits (0x4000) => 0x4001
    vdp.WriteWord(0x04, 0x4001); 
    vdp.WriteWord(0x04, 0x0000); 

    vdp.WriteWord(0x00, 0xABCD);

    // On real hardware, writing to an odd address ignores the LSB (aligns to $0000)
    // but SWAPS the written bytes!
    EXPECT_EQ(vdp.ReadVramDirect(0x0000), 0xCD);
    EXPECT_EQ(vdp.ReadVramDirect(0x0001), 0xAB);
    EXPECT_EQ(vdp.GetTargetAddress(), 0x0003); // Auto-increment still adds 2 to original odd address
}

TEST(VdpBehaviorTests, VdpStatusRegisterVIPFlagClearedOnRead) {
    Vdp vdp;
    
    // Trigger VBlank to set the VIP (Vertical Interrupt Pending) flag
    vdp.SetVblankActive(true);
    
    // First read should return VIP flag set (bit 7 = 0x80)
    Word status1 = vdp.ReadWord(0x04);
    EXPECT_NE(status1 & 0x0080, 0); 
    
    // Second read should return VIP flag cleared (bit 7 = 0)
    Word status2 = vdp.ReadWord(0x04);
    EXPECT_EQ(status2 & 0x0080, 0); 
}

TEST(VdpBehaviorTests, VdpCramWriteAndColorConversion) {
    Vdp vdp;
    
    // Auto-increment 2
    vdp.WriteWord(0x04, 0x8F02);

    // Initiate 32-bit write to CRAM address $0000
    vdp.WriteWord(0x04, 0xC000);
    vdp.WriteWord(0x04, 0x0000);

    // Write Sega Blue color format: 0000 BBB0 GGG0 RRR0
    // Pure blue: 0x0E00 => High Byte 0x0E (Blue=7), Low Byte 0x00 (Green=0, Red=0)
    vdp.WriteWord(0x00, 0x0E00);

    // Verify written values inside CRAM
    EXPECT_EQ(vdp.ReadCramDirect(0x0000), 0x0E);
    EXPECT_EQ(vdp.ReadCramDirect(0x0001), 0x00);

    // Verify 9-bit BGR to 32-bit RGBA scaling conversion
    std::uint32_t colorRGBA = VdpRenderer::ConvertColor(0x0E, 0x00);
    EXPECT_EQ(colorRGBA, 0x0000FFFF); 
}

TEST(VdpBehaviorTests, VdpRenderPlaneScanlineCalculatesOutput) {
    Vdp vdp;
    
    // Register 15 (Auto-increment 2)
    vdp.WriteWord(0x04, 0x8F02);

    // Set Register 2 (Plane A nametable base) to 0x38 (VRAM address $E000)
    vdp.WriteWord(0x04, 0x8238);

    // Write a tile descriptor Word to Plane A nametable at column 0, row 0 ($E000)
    vdp.WriteWord(0x04, 0x4000 | (0xE000 & 0x3FFF));
    vdp.WriteWord(0x04, 0x0000 | (0xE000 >> 14));
    vdp.WriteWord(0x00, 0x0001);

    // Write 4-bit pixel index pattern data for Tile 1 ($0020 to $003F)
    vdp.WriteWord(0x04, 0x4020);
    vdp.WriteWord(0x04, 0x0000);
    vdp.WriteWord(0x00, 0x1111);
    vdp.WriteWord(0x00, 0x1111);

    // Write color 1 of palette line 0 inside CRAM ($0002)
    vdp.WriteWord(0x04, 0xC002);
    vdp.WriteWord(0x04, 0x0000);
    vdp.WriteWord(0x00, 0x000E);

    // Define target line buffer (Clear to transparent)
    std::uint32_t screenLine[320] = {0};

    // Render scanline 0 (targets Tile Row 0, pixel row 0)
    VdpRenderer::RenderPlaneScanline(vdp, 0, 0, 320, screenLine);

    // Verify first 8 pixels (of tile 1) contain the compiled Red color
    for (int i = 0; i < 8; ++i) {
        EXPECT_EQ(screenLine[i], 0xFF0000FF); 
    }

    // Verify outside pixels remain transparent (0x00000000)
    EXPECT_EQ(screenLine[8], 0x00000000);
}

TEST(VdpBehaviorTests, VdpRenderPlaneScanlineCalculatesOutputColumn1) {
    Vdp vdp;
    
    vdp.WriteWord(0x04, 0x8F02); // Register 15: Auto-increment 2
    vdp.WriteWord(0x04, 0x8238); // Register 2: Plane A Nametable base $E000

    // Write tile descriptor words for Column 0 AND Column 1 sequentially
    vdp.WriteWord(0x04, 0x4000 | (0xE000 & 0x3FFF));
    vdp.WriteWord(0x04, 0x0000 | (0xE000 >> 14));
    vdp.WriteWord(0x00, 0x0001); // Col 0 -> Point to Tile index 1
    vdp.WriteWord(0x00, 0x0001); // Col 1 -> Point to Tile index 1

    // Write pattern data for Tile 1 ($0020 to $003F)
    vdp.WriteWord(0x04, 0x4020);
    vdp.WriteWord(0x04, 0x0000);
    for (int i = 0; i < 16; ++i) {
        vdp.WriteWord(0x00, 0x1111); // Fill tile 1 rows with color index 1
    }

    // Write color 1 of palette line 0 to CRAM ($0002) -> Red
    vdp.WriteWord(0x04, 0xC002);
    vdp.WriteWord(0x04, 0x0000);
    vdp.WriteWord(0x00, 0x000E);

    std::uint32_t screenLine[320] = {0};
    VdpRenderer::RenderPlaneScanline(vdp, 0, 0, 320, screenLine);

    // Verify both Column 0 (pixels 0-7) AND Column 1 (pixels 8-15) are rendered in Red
    for (int i = 0; i < 16; ++i) {
        EXPECT_EQ(screenLine[i], 0xFF0000FF);
    }
}

TEST(VdpBehaviorTests, VdpDmaCopyFromWramToVram) {
    MainBus bus;
    Vdp vdp(&bus);
    WorkRAM wram;
    
    bus.AttachDevice(&wram, 0xE00000, 0xFFFFFF);
    bus.AttachDevice(&vdp,  0xC00000, 0xC0001F);

    // 1. Write source descriptors to WorkRAM ($E00000 and $E00002)
    wram.WriteWord(0x0000, 0x0001); // Col 0 -> Tile Index 1
    wram.WriteWord(0x0002, 0x0002); // Col 1 -> Tile Index 2

    // 2. Set up VDP Registers for DMA
    vdp.WriteWord(0x04, 0x8F02); // Register 15: Auto-increment 2
    vdp.WriteWord(0x04, 0x8238); // Register 2: Plane A Nametable base $E000

    // Set DMA Length to 2 words
    vdp.WriteWord(0x04, 0x9302); // Reg 19: 2
    vdp.WriteWord(0x04, 0x9400); // Reg 20: 0

    // Set DMA Source Address on M68k side: $E00000 
    vdp.WriteWord(0x04, 0x9500); // Reg 21: srcLow = $00
    vdp.WriteWord(0x04, 0x9600); // Reg 22: srcMid = $00
    vdp.WriteWord(0x04, 0x9770); // Reg 23: srcHigh = $70

    // 3. Trigger DMA to VRAM address $E000
    vdp.WriteWord(0x04, 0x4000 | (0xE000 & 0x3FFF));
    vdp.WriteWord(0x04, 0x0080 | (0xE000 >> 14)); 

    // Run ProcessDma to execute the transfer
    EXPECT_TRUE(vdp.IsDmaActive());
    vdp.ProcessDma(100); 
    EXPECT_FALSE(vdp.IsDmaActive());

    // 4. Verify VRAM contents byte-by-byte
    EXPECT_EQ(vdp.ReadVramDirect(0xE000), 0x00);
    EXPECT_EQ(vdp.ReadVramDirect(0xE001), 0x01);
    EXPECT_EQ(vdp.ReadVramDirect(0xE002), 0x00);
    EXPECT_EQ(vdp.ReadVramDirect(0xE003), 0x02);
}

TEST(VdpBehaviorTests, VdpDmaCopyFullTileFromWramToVram) {
    MainBus bus;
    Vdp vdp(&bus);
    WorkRAM wram;
    
    bus.AttachDevice(&wram, 0xE00000, 0xFFFFFF);
    bus.AttachDevice(&vdp,  0xC00000, 0xC0001F);

    // 1. Fill WRAM with 16 distinct sequential words (32 bytes representing 1 tile)
    for (int i = 0; i < 16; ++i) {
        wram.WriteWord(i * 2, 0x1111 * (i + 1));
    }

    // 2. Set up DMA
    vdp.WriteWord(0x04, 0x8F02); // Auto-increment 2
    vdp.WriteWord(0x04, 0x9310); // Reg 19: Length = 16 words (0x0010)
    vdp.WriteWord(0x04, 0x9400); // Reg 20: 0

    vdp.WriteWord(0x04, 0x9500); // Reg 21: srcLow = 0
    vdp.WriteWord(0x04, 0x9600); // Reg 22: srcMid = 0
    vdp.WriteWord(0x04, 0x9770); // Reg 23: srcHigh = $70 (WRAM $E00000)

    // Trigger DMA to VRAM address $0020
    vdp.WriteWord(0x04, 0x4000 | (0x0020 & 0x3FFF));
    vdp.WriteWord(0x04, 0x0080 | (0x0020 >> 14));

    // Run DMA
    EXPECT_TRUE(vdp.IsDmaActive());
    vdp.ProcessDma(200); // Spend cycles to execute
    EXPECT_FALSE(vdp.IsDmaActive());

    // 3. Verify entire VRAM block sequentially
    for (int i = 0; i < 16; ++i) {
        Word expected = 0x1111 * (i + 1);
        Address addr = 0x0020 + i * 2;
        Word actual = (vdp.ReadVramDirect(addr) << 8) | vdp.ReadVramDirect(addr + 1);
        EXPECT_EQ(actual, expected) << "DMA corruption at index " << i << " (VRAM Address: 0x" << std::hex << addr << ")";
    }
}

TEST(VdpBehaviorTests, VdpDmaVramFillCalculatesOutput) {
    MainBus bus;
    Vdp vdp(&bus);
    
    bus.AttachDevice(&vdp, 0xC00000, 0xC0001F);

    // 1. Arm VRAM Fill
    vdp.WriteWord(0x04, 0x8F01); // Register 15: Auto-increment 1 (Byte-based)
    vdp.WriteWord(0x04, 0x9304); // Reg 19: Length = 4 bytes
    vdp.WriteWord(0x04, 0x9400); // Reg 20: 0
    vdp.WriteWord(0x04, 0x9780); // Reg 23: DMA Fill Mode (Bit 7 is 1, Bit 6 is 0)

    // 2. Set VRAM Fill Destination to $E000 and Trigger with Data Port Write
    vdp.WriteWord(0x04, 0x4000 | (0xE000 & 0x3FFF));
    vdp.WriteWord(0x04, 0x0080 | (0xE000 >> 14)); // CD5 is set (DMA)

    // Write the fill value to the Data Port (Upper byte 0xAA is used for filling)
    vdp.WriteWord(0x00, 0xAA00); 

    // Run DMA Fill
    EXPECT_TRUE(vdp.IsDmaActive());
    vdp.ProcessDma(100);
    EXPECT_FALSE(vdp.IsDmaActive());

    // 3. Verify VRAM was filled byte-by-byte at $E000, $E001, $E002, $E003
    EXPECT_EQ(vdp.ReadVramDirect(0xE000), 0xAA);
    EXPECT_EQ(vdp.ReadVramDirect(0xE001), 0xAA);
    EXPECT_EQ(vdp.ReadVramDirect(0xE002), 0xAA);
    EXPECT_EQ(vdp.ReadVramDirect(0xE003), 0xAA);
}

TEST(VdpBehaviorTests, VdpRenderPlaneScanlineWithHScroll) {
    Vdp vdp;
    vdp.WriteWord(0x04, 0x8F02); // Auto-increment 2
    vdp.WriteWord(0x04, 0x8238); // Plane A nametable base $E000
    vdp.WriteWord(0x04, 0x8D3F); // Reg 13: H-Scroll Table Base Address $FC00 (offset $FC00 >> 10 = $3F)

    // Write tile descriptor at $E000 (Col 0) -> Tile 1
    vdp.WriteWord(0x04, 0x4000 | (0xE000 & 0x3FFF));
    vdp.WriteWord(0x04, 0x0000 | (0xE000 >> 14));
    vdp.WriteWord(0x00, 0x0001);

    // Write tile 1 patterns to $0020
    vdp.WriteWord(0x04, 0x4020);
    vdp.WriteWord(0x04, 0x0000);
    for (int i = 0; i < 16; ++i) {
        vdp.WriteWord(0x00, 0x1111);
    }

    // Write H-Scroll offset of 4 pixels to $FC00 (Plane A H-Scroll)
    vdp.WriteWord(0x04, 0x4000 | (0xFC00 & 0x3FFF));
    vdp.WriteWord(0x04, 0x0000 | (0xFC00 >> 14));
    vdp.WriteWord(0x00, 0x0004); // Shift Plane A right by 4 pixels

    // Write color 1 of palette line 0 inside CRAM ($0002)
    vdp.WriteWord(0x04, 0xC002);
    vdp.WriteWord(0x04, 0x0000);
    vdp.WriteWord(0x00, 0x000E);

    std::uint32_t screenLine[320] = {0};
    VdpRenderer::RenderPlaneScanline(vdp, 0, 0, 320, screenLine);

    // Since we shifted Plane A right by 4 pixels:
    // Pixels 0-3 must be completely empty/transparent (0),
    // and pixels 4-11 must contain the Red tile pixels.
    for (int i = 0; i < 4; ++i) {
        EXPECT_EQ(screenLine[i], 0x00000000) << "Expected transparency at pixel " << i;
    }
    for (int i = 4; i < 12; ++i) {
        EXPECT_EQ(screenLine[i], 0xFF0000FF) << "Expected red at pixel " << i;
    }
}

TEST(VdpBehaviorTests, BootSonicAndCheckVram) {
    std::ifstream file("roms/sonic.bin", std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        SUCCEED(); // Gracefully skip test if ROM is missing in the container
        return;
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<Byte> romData(size);
    file.read(reinterpret_cast<char*>(romData.data()), size);

    MainBus bus;
    M68k cpu(&bus);
    Vdp vdp(&bus);
    WorkRAM wram;
    IoPorts ioPorts;
    Cartridge cartridge;
    
    cartridge.LoadROM(romData);

    bus.AttachDevice(&cartridge, 0x000000, 0x3FFFFF);
    bus.AttachDevice(&ioPorts,   0xA10000, 0xA1001F);
    bus.AttachDevice(&vdp,       0xC00000, 0xC0001F);
    bus.AttachDevice(&wram,      0xE00000, 0xFFFFFF);

    cpu.Reset();

    // Run the integrated console motherboard for 200,000 cycles
    int cycles = 0;
    while (cycles < 200000) {
        int consumed = 0;
        if (vdp.IsDmaActive()) {
            consumed = vdp.ProcessDma(50);
        } else {
            consumed = cpu.Step();
        }
        cycles += consumed;
        vdp.SetFrameCycles(cycles);
    }

    // Inspect VRAM to verify the ROM successfully executed instructions,
    // bypassed security handshakes, and loaded graphical assets.
    int nonZeroBytes = 0;
    for (int i = 0; i < 0x10000; ++i) {
        if (vdp.ReadVramDirect(i) != 0) {
            nonZeroBytes++;
        }
    }
    
    EXPECT_GT(nonZeroBytes, 100) << "VRAM remains unpopulated. ROM program crashed or locked during boot.";
}

// --- NEW TDD INTEGRATION TEST WITH HIGH-PRECISION TRACING ---
TEST(VdpBehaviorTests, BootCustomHomebrewAndCheckVisuals) {
    std::vector<Byte> mockRom(1024, 0x00);
    
    // Set up standard Stack Pointer ($00FF0000) and PC ($00000100) vectors at $0
    mockRom[0] = 0x00; mockRom[1] = 0xFF; mockRom[2] = 0x00; mockRom[3] = 0x00;
    mockRom[4] = 0x00; mockRom[5] = 0x00; mockRom[6] = 0x01; mockRom[7] = 0x00;

    // Write our custom compiled assembly binary program starting at PC $100:
    std::vector<Word> opcodes = {
        0x33FC, 0x8F02, 0x00C0, 0x0004, // move.w #$8F02, ($C00004).l  (Reg 15: auto-inc = 2)
        0x33FC, 0x8238, 0x00C0, 0x0004, // move.w #$8238, ($C00004).l  (Reg 2: Plane A at $E000)
        0x23FC, 0x6000, 0x0003, 0x00C0, 0x0004, // move.l #$60000003, ($C00004).l (VRAM $E000 write)
        0x303C, 0x0027,                 // move.w #39, d0             (Loop for 40 columns)
        0x33FC, 0x0001, 0x00C0, 0x0000, // write_loop: move.w #$0001, ($C00000).l (Write descriptor)
        0x51C8, 0xFFF6,                 // dbf d0, write_loop         (Decrement and loop)
        0x23FC, 0x4020, 0x0000, 0x00C0, 0x0004, // move.l #$40200000, ($C00004).l (VRAM $0020 tile 1 pattern)
        0x303C, 0x000F,                 // move.w #15, d0
        0x33FC, 0x1111, 0x00C0, 0x0000, // pattern_loop: move.w #$1111, ($C00000).l
        0x51C8, 0xFFF6,                 // dbf d0, pattern_loop
        0x23FC, 0xC002, 0x0000, 0x00C0, 0x0004, // move.l #$C0020000, ($C00004).l (CRAM $0002 setup)
        0x33FC, 0x000E, 0x00C0, 0x0000, // move.w #$000E, ($C00000).l (Red palette)
        0x60FE                          // infinite_loop: bra.s infinite_loop
    };

    for (std::size_t i = 0; i < opcodes.size(); ++i) {
        mockRom[0x100 + i * 2] = static_cast<Byte>(opcodes[i] >> 8);
        mockRom[0x100 + i * 2 + 1] = static_cast<Byte>(opcodes[i] & 0xFF);
    }

    MainBus bus;
    M68k cpu(&bus);
    Vdp vdp(&bus);
    WorkRAM wram;
    IoPorts ioPorts;
    Cartridge cartridge;
    
    cartridge.LoadROM(mockRom);

    bus.AttachDevice(&cartridge, 0x000000, 0x3FFFFF);
    bus.AttachDevice(&ioPorts,   0xA10000, 0xA1001F);
    bus.AttachDevice(&vdp,       0xC00000, 0xC0001F);
    bus.AttachDevice(&wram,      0xE00000, 0xFFFFFF);

    cpu.Reset();

    // Run the custom ROM program for 5000 cycles, and PRINT THE INSTRUCTION BY INSTRUCTION TRACE.
    // This allows us to inspect exactly why no values are written to VRAM during M68K pipeline execution.
    int cycles = 0;
    while (cycles < 5000) {
        Address currentPC = cpu.GetPC();
        int consumed = cpu.Step();
        cycles += consumed;
        
        // Print execution trace for PC range 0x100 to 0x150 to catch our program setup
        if (currentPC >= 0x100 && currentPC <= 0x150) {
            std::cout << "[TDD TRACE] PC: 0x" << std::hex << std::uppercase << currentPC 
                      << " | D0: 0x" << cpu.GetDRegister(0) 
                      << " | VDP Target: 0x" << vdp.GetTargetAddress() 
                      << " | VRAM[$E001]: 0x" << static_cast<int>(vdp.ReadVramDirect(0xE001))
                      << std::dec << std::endl;
        }
    }

    // 1. Verify CPU successfully completed sequential VRAM writes
    EXPECT_EQ(vdp.ReadVramDirect(0xE000), 0x00);
    EXPECT_EQ(vdp.ReadVramDirect(0xE001), 0x01);
    
    EXPECT_EQ(vdp.ReadVramDirect(0xE002), 0x00);
    EXPECT_EQ(vdp.ReadVramDirect(0xE003), 0x01);

    // 2. Render scanline 0
    std::uint32_t screenLine[320] = {0};
    VdpRenderer::RenderPlaneScanline(vdp, 0, 0, 320, screenLine);

    for (int i = 0; i < 320; ++i) {
        ASSERT_EQ(screenLine[i], 0xFF0000FF) << "Alternating column dropout detected at pixel index " << i << "!";
    }
}