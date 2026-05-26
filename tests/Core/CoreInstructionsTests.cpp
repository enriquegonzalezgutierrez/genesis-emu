// ==============================================================================
// GenesisEmu - Consolidated Core Instructions Unit Tests (Corrected)
// ==============================================================================
// This file contains unit tests to verify the core CPU instructions
// (Comparisons, Quick Operations, Shifts, Bit Tests, and Unary operations).
// ==============================================================================

#include <gtest/gtest.h>
#include "M68kCoreInstructions.h"

using namespace GenesisEmu::Core;

// ------------------------------------------------------------------------------
// Test Cases: M68kCoreInstructions Unit
// ------------------------------------------------------------------------------

TEST(CoreInstructionsTests, ExecuteCMPCorrectFlags) {
    Word sr = 0x2700;

    // Compare 10 and 15 (calculates 10 - 15 = -5)
    // Dest is smaller than Src, generates borrow (C=1, N=1, Z=0)
    M68kCoreInstructions::ExecuteCMP(10, 15, OperandSize::WORD, sr);

    EXPECT_NE(sr & 0x0001, 0x0); // C flag set
    EXPECT_NE(sr & 0x0008, 0x0); // N flag set
    EXPECT_EQ(sr & 0x0004, 0x0); // Z flag cleared
}

TEST(CoreInstructionsTests, ExecuteTSTUpdatesZeroAndNegative) {
    Word sr = 0x271F; // Start with all condition flags active

    // Test a non-zero positive value (clear Z and N, clear V and C)
    M68kCoreInstructions::ExecuteTST(0x0055, OperandSize::BYTE, sr);

    EXPECT_EQ(sr & 0x000F, 0x0000); // N, Z, V, C must be cleared
    EXPECT_NE(sr & 0x0010, 0x0);     // X remains unaffected
}

TEST(CoreInstructionsTests, ExecuteMOVEQSignExtends) {
    // 0x80 (-128) should sign extend to 0xFFFFFF80 (-128 32-bit)
    Longword result = M68kCoreInstructions::ExecuteMOVEQ(0x80);
    EXPECT_EQ(result, 0xFFFFFF80);

    // 0x0F (15) should extend to 0x0000000F
    Longword resultPos = M68kCoreInstructions::ExecuteMOVEQ(0x0F);
    EXPECT_EQ(resultPos, 0x0000000F);
}

TEST(CoreInstructionsTests, ExecuteBTSTTestsBits) {
    Word sr = 0x2700;

    // Test bit 3 of value 0x0008 (bit 3 is set, so Z should be cleared)
    M68kCoreInstructions::ExecuteBTST(0x0008, 3, OperandSize::WORD, sr);
    EXPECT_EQ(sr & 0x0004, 0x0); // Z flag must be 0 (bit was set)

    // Test bit 2 of value 0x0008 (bit 2 is 0, so Z should be set to 1)
    M68kCoreInstructions::ExecuteBTST(0x0008, 2, OperandSize::WORD, sr);
    EXPECT_NE(sr & 0x0004, 0x0); // Z flag must be 1 (bit was 0)
}

TEST(CoreInstructionsTests, ExecuteLSRShiftsAndUpdatesFlags) {
    Word sr = 0x2700;

    // LSR.B 1, %d0 (Value 0x05 shifted right by 1 => 0x02, Carry is 1)
    Longword result = M68kCoreInstructions::ExecuteLSR(0x05, 1, OperandSize::BYTE, sr);

    EXPECT_EQ(result & 0xFF, 0x02);
    EXPECT_NE(sr & 0x0001, 0x0); // C flag set
    EXPECT_NE(sr & 0x0010, 0x0); // X flag set
    EXPECT_EQ(sr & 0x0004, 0x0); // Z flag clear
}

TEST(CoreInstructionsTests, ExecuteLSLShiftsAndUpdatesFlags) {
    Word sr = 0x2700;

    // LSL.B 1, %d0 (Value 0x80 shifted left by 1 => 0x00, Carry is 1, Zero is 1)
    Longword result = M68kCoreInstructions::ExecuteLSL(0x80, 1, OperandSize::BYTE, sr);

    EXPECT_EQ(result & 0xFF, 0x00);
    EXPECT_NE(sr & 0x0001, 0x0); // C flag set
    EXPECT_NE(sr & 0x0010, 0x0); // X flag set
    EXPECT_NE(sr & 0x0004, 0x0); // Z flag set
}

TEST(CoreInstructionsTests, ExecuteEXTSignExtendsCorrectly) {
    // EXT.W (extend 0x0080 which has bit 7 set, to 0xFF80)
    Longword result = M68kCoreInstructions::ExecuteEXT(0x0080, OperandSize::WORD);
    EXPECT_EQ(result & 0xFFFF, 0xFF80);

    // EXT.L (extend 0x00008000 which has bit 15 set, to 0xFFFF8000)
    Longword resultLong = M68kCoreInstructions::ExecuteEXT(0x00008000, OperandSize::LONG);
    EXPECT_EQ(resultLong, 0xFFFF8000); // Corrected assertion value
}

TEST(CoreInstructionsTests, ExecuteSWAPSwapsWords) {
    Word sr = 0x2700;
    Longword value = 0x12345678;

    Longword result = M68kCoreInstructions::ExecuteSWAP(value, sr);

    EXPECT_EQ(result, 0x56781234);
    EXPECT_EQ(sr & 0x000C, 0x0); // Result is positive and non-zero
}