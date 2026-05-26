// ==============================================================================
// GenesisEmu - Arithmetic Unit Tests (TDD)
// ==============================================================================
// This file contains unit tests to verify the mathematical execution unit in
// absolute isolation from CPU fetch and memory bus components.
// ==============================================================================

#include <gtest/gtest.h>
#include "M68kArithmetic.h"

using namespace GenesisEmu::Core;

// ------------------------------------------------------------------------------
// Test Cases: M68kArithmetic Unit
// ------------------------------------------------------------------------------

TEST(ArithmeticTests, ExecuteADDBasicWord) {
    Word sr = 0x2700; // Preset default Status Register (Flags: 0)
    
    // 10 + 5 = 15
    Longword result = M68kArithmetic::ExecuteADD(10, 5, OperandSize::WORD, sr);

    EXPECT_EQ(result, 15);
    // Flags should remain 0 (result not zero, not negative, no overflow, no carry)
    EXPECT_EQ(sr & 0x001F, 0x0000);
}

TEST(ArithmeticTests, ExecuteADDUpdatesZeroAndNegativeFlags) {
    Word sr = 0x2700;

    // Word addition: 0xFFFF + 1 = 0x0000 (Wraps, sets Z, X, C)
    Longword result = M68kArithmetic::ExecuteADD(0xFFFF, 1, OperandSize::WORD, sr);

    EXPECT_EQ(result, 0x0);
    EXPECT_NE(sr & 0x0004, 0x0); // Z flag (bit 2) must be set
    EXPECT_NE(sr & 0x0001, 0x0); // C flag (bit 0) must be set
    EXPECT_NE(sr & 0x0010, 0x0); // X flag (bit 4) must be set
}

TEST(ArithmeticTests, ExecuteADDDetectsOverflow) {
    Word sr = 0x2700;

    // Positive overflow test (Word signed limit): 0x7FFF (32767) + 1 = 0x8000 (-32768)
    Longword result = M68kArithmetic::ExecuteADD(0x7FFF, 1, OperandSize::WORD, sr);

    EXPECT_EQ(result, 0x8000);
    EXPECT_NE(sr & 0x0002, 0x0); // V flag (bit 1) must be set
    EXPECT_NE(sr & 0x0008, 0x0); // N flag (bit 3) must be set
}

TEST(ArithmeticTests, ExecuteSUBBasicWord) {
    Word sr = 0x2700;

    // 15 - 5 = 10
    Longword result = M68kArithmetic::ExecuteSUB(15, 5, OperandSize::WORD, sr);

    EXPECT_EQ(result, 10);
    EXPECT_EQ(sr & 0x001F, 0x0000);
}

TEST(ArithmeticTests, ExecuteSUBGeneratesBorrow) {
    Word sr = 0x2700;

    // Word subtraction generating borrow: 5 - 10 = 0xFFFB (-5, sets N, X, C)
    Longword result = M68kArithmetic::ExecuteSUB(5, 10, OperandSize::WORD, sr);

    EXPECT_EQ(result, 0xFFFB);
    EXPECT_NE(sr & 0x0008, 0x0); // N flag (bit 3) must be set
    EXPECT_NE(sr & 0x0001, 0x0); // C flag (bit 0) must be set (borrow)
    EXPECT_NE(sr & 0x0010, 0x0); // X flag (bit 4) must be set (borrow)
}

TEST(ArithmeticTests, ExecuteANDBasicWord) {
    Word sr = 0x271F; // Start with all condition flags set to 1

    // 0x5555 AND 0x00FF = 0x0055 (Clear N, Z. C and V must be cleared)
    Longword result = M68kArithmetic::ExecuteAND(0x5555, 0x00FF, OperandSize::WORD, sr);

    EXPECT_EQ(result, 0x0055);
    EXPECT_EQ(sr & 0x0003, 0x0000); // C and V flags must be cleared
    EXPECT_EQ(sr & 0x000C, 0x0000); // N and Z flags must be cleared
    EXPECT_NE(sr & 0x0010, 0x0);     // X flag (bit 4) must remain unaffected (1)
}