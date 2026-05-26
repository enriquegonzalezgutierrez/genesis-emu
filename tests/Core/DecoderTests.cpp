// ==============================================================================
// GenesisEmu - M68k CPU Decoder Unit Tests (TDD)
// ==============================================================================
// This file contains unit tests to verify that the instruction decoder
// correctly parses raw 16-bit opcodes into structured metadata.
// ==============================================================================

#include <gtest/gtest.h>
#include "M68kInstruction.h"
#include "M68kDecoder.h" // Note: This header will be created in the next step

using namespace GenesisEmu::Core;

// ------------------------------------------------------------------------------
// Test Suite: M68kDecoderTests
// ------------------------------------------------------------------------------

TEST(M68kDecoderTests, DecodeNOP) {
    // 1. Arrange & Act
    // Opcode 0x4E71 is NOP
    DecodedInstruction inst = M68kDecoder::Decode(0x4E71);

    // 2. Assert
    EXPECT_EQ(inst.type, OpType::NOP);
    EXPECT_EQ(inst.size, OperandSize::NONE);
}

TEST(M68kDecoderTests, DecodeMoveDataRegisterDirect) {
    // 1. Arrange & Act
    // Opcode 0x3200 is: MOVE.W D0, D1 (Move Word from D0 to D1)
    // Structure of 68000 MOVE instruction:
    // Bits 15-14: 00 (MOVE identifier)
    // Bits 13-12: 11 (Size: Word .W)
    // Bits 11-9:  001 (Destination Register: D1)
    // Bits 8-6:   000 (Destination Mode: Data Register Direct)
    // Bits 5-3:   000 (Source Mode: Data Register Direct)
    // Bits 2-0:   000 (Source Register: D0)
    // Binary: 0011 0010 0000 0000 => 0x3200
    DecodedInstruction inst = M68kDecoder::Decode(0x3200);

    // 2. Assert
    EXPECT_EQ(inst.type, OpType::MOVE);
    EXPECT_EQ(inst.size, OperandSize::WORD);
    
    // Verify Source Operand
    EXPECT_EQ(inst.srcMode, AddressingMode::DataRegisterDirect);
    EXPECT_EQ(inst.srcRegister, 0); // D0
    
    // Verify Destination Operand
    EXPECT_EQ(inst.destMode, AddressingMode::DataRegisterDirect);
    EXPECT_EQ(inst.destRegister, 1); // D1
}

TEST(M68kDecoderTests, DecodeUnknownOpcode) {
    // 1. Arrange & Act
    // Opcode 0x0000 is not supported / unknown in our current instruction set
    DecodedInstruction inst = M68kDecoder::Decode(0x0000);

    // 2. Assert
    EXPECT_EQ(inst.type, OpType::UNKNOWN);
}