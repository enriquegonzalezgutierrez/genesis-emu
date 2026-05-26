// ==============================================================================
// GenesisEmu - VDP Unit Tests (TDD)
// ==============================================================================
// This file contains unit tests to verify the VDP control register writes,
// the control port 32-bit state machine (flip-flop), and data port VRAM writes.
// ==============================================================================

#include <gtest/gtest.h>
#include "Vdp.h"

using namespace GenesisEmu::Core;

// ------------------------------------------------------------------------------
// Test Suite: VdpBehaviorTests
// ------------------------------------------------------------------------------

TEST(VdpBehaviorTests, VdpRegisterWrite) {
    // 1. Arrange
    Vdp vdp;

    // 2. Act
    // Write value 0x02 to Register 15 (0x0F - Auto-increment)
    // Format: $8000 OR (register << 8) OR value => $8F02
    // Control Port is at Offset 0x04 relative to $C00000
    vdp.WriteWord(0x04, 0x8F02);

    // 3. Assert
    EXPECT_EQ(vdp.GetRegister(15), 0x02);
}

TEST(VdpBehaviorTests, VdpVramWriteAndAutoincrement) {
    // 1. Arrange
    Vdp vdp;
    
    // Set VDP Register 15 (Auto-increment) to 2
    vdp.WriteWord(0x04, 0x8F02);

    // 2. Act
    // Initiate 32-bit write to VRAM address $0000.
    // VRAM Write command mask is $40000000.
    // 1st word: Address bits 13-0 OR command bits 31-16 => $4000 (VRAM Write)
    // 2nd word: Address bits 15-14 (placed in bits 1-0) => $0000
    vdp.WriteWord(0x04, 0x4000); // 1st Word (Sets write pending, latches data)
    vdp.WriteWord(0x04, 0x0000); // 2nd Word (Executes command, sets target address)

    // Write a Word data $ABCD to the VDP Data Port (Offset 0x00)
    vdp.WriteWord(0x00, 0xABCD);

    // 3. Assert
    // Verify that the data was written to VRAM.
    // Since the system is Big-Endian: high byte ($AB) goes to address 0, low byte ($CD) to address 1.
    EXPECT_EQ(vdp.ReadVramDirect(0x0000), 0xAB);
    EXPECT_EQ(vdp.ReadVramDirect(0x0001), 0xCD);

    // Verify that target address auto-incremented by 2 (the value in Register 15)
    EXPECT_EQ(vdp.GetTargetAddress(), 0x0002);
}