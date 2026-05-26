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

TEST(M68kDecoderTests, DecodeDBF) {
    // 1. Arrange & Act
    // Opcode 0x51CA is DBF D2, displacement
    DecodedInstruction inst = M68kDecoder::Decode(0x51CA);

    // 2. Assert
    EXPECT_EQ(inst.type, OpType::DBF);
    EXPECT_EQ(inst.size, OperandSize::WORD);
    EXPECT_EQ(inst.srcRegister, 2); // D2
    EXPECT_EQ(inst.destMode, AddressingMode::ProgramCounterDisplacement);
}

TEST(M68kDecoderTests, DecodeANDI) {
    // 1. Arrange & Act
    // Opcode 0x0240 is ANDI.W #data, D0
    DecodedInstruction inst = M68kDecoder::Decode(0x0240);

    // 2. Assert
    EXPECT_EQ(inst.type, OpType::AND);
    EXPECT_EQ(inst.size, OperandSize::WORD);
    EXPECT_EQ(inst.srcMode, AddressingMode::Immediate);
    EXPECT_EQ(inst.destMode, AddressingMode::DataRegisterDirect);
    EXPECT_EQ(inst.destRegister, 0); // D0
}

TEST(M68kDecoderTests, DecodeUnknownOpcode) {
    // 1. Arrange & Act
    // Opcode 0xFFFF is not supported / unknown in our instruction set
    DecodedInstruction inst = M68kDecoder::Decode(0xFFFF);

    // 2. Assert
    EXPECT_EQ(inst.type, OpType::UNKNOWN);
}

// ------------------------------------------------------------------------------
// ADDQ / SUBQ - Quick Arithmetic
// ------------------------------------------------------------------------------

TEST(M68kDecoderTests, DecodeADDQ_Word_D2) {
    // ADDQ.W #4, D2 = 0x5842
    // encoding: 0101 100 0 01 000 010 = 0x5842  (bit8=0 => ADDQ, bits11-9=100 => imm 4)
    DecodedInstruction inst = M68kDecoder::Decode(0x5842);

    EXPECT_EQ(inst.type, OpType::ADDQ);
    EXPECT_EQ(inst.size, OperandSize::WORD);
    EXPECT_EQ(inst.immediateData, 4u);
    EXPECT_EQ(inst.destMode, AddressingMode::DataRegisterDirect);
    EXPECT_EQ(inst.destRegister, 2);
}

TEST(M68kDecoderTests, DecodeSUBQ_Word_D2) {
    // SUBQ.W #1, D2 => 0x5342
    // encoding: 0101 001 1 01 000 010 = 0x5342
    DecodedInstruction inst = M68kDecoder::Decode(0x5342);

    EXPECT_EQ(inst.type, OpType::SUBQ);
    EXPECT_EQ(inst.size, OperandSize::WORD);
    EXPECT_EQ(inst.immediateData, 1u);
    EXPECT_EQ(inst.destMode, AddressingMode::DataRegisterDirect);
    EXPECT_EQ(inst.destRegister, 2);
}

TEST(M68kDecoderTests, DecodeSUBQ_Long_D0) {
    // SUBQ.L #8, D0 => 0x5380 | (0<<9) = immediate 8 (encoded as 0)
    // encoding: 0101 000 1 10 000 000 = 0x5180 (immediate 8 encoded as 0)
    DecodedInstruction inst = M68kDecoder::Decode(0x5180);

    EXPECT_EQ(inst.type, OpType::SUBQ);
    EXPECT_EQ(inst.size, OperandSize::LONG);
    EXPECT_EQ(inst.immediateData, 8u);  // 0 in bits 11-9 encodes as 8
}

// ------------------------------------------------------------------------------
// SWAP / EXT - Unary operations
// ------------------------------------------------------------------------------

TEST(M68kDecoderTests, DecodeSWAP_NotPEA) {
    // SWAP D1 = 0x4841. Must decode as SWAP, NOT as PEA.
    DecodedInstruction inst = M68kDecoder::Decode(0x4841);

    EXPECT_EQ(inst.type, OpType::SWAP);
    EXPECT_EQ(inst.size, OperandSize::LONG);
    EXPECT_EQ(inst.destMode, AddressingMode::DataRegisterDirect);
    EXPECT_EQ(inst.destRegister, 1);
}

TEST(M68kDecoderTests, DecodeEXT_WordSize) {
    // EXT.W D3 = 0x4883  (byte -> word sign-extend)
    DecodedInstruction inst = M68kDecoder::Decode(0x4883);

    EXPECT_EQ(inst.type, OpType::EXT);
    EXPECT_EQ(inst.size, OperandSize::WORD);
    EXPECT_EQ(inst.destRegister, 3);
}

TEST(M68kDecoderTests, DecodeEXT_LongSize) {
    // EXT.L D5 = 0x48C5  (word -> long sign-extend)
    DecodedInstruction inst = M68kDecoder::Decode(0x48C5);

    EXPECT_EQ(inst.type, OpType::EXT);
    EXPECT_EQ(inst.size, OperandSize::LONG);
    EXPECT_EQ(inst.destRegister, 5);
}

TEST(M68kDecoderTests, DecodePEA_StillWorks) {
    // PEA (A0) = 0x4850  (EA mode 2 = AddressRegisterIndirect)
    DecodedInstruction inst = M68kDecoder::Decode(0x4850);

    EXPECT_EQ(inst.type, OpType::PEA);
    EXPECT_EQ(inst.destMode, AddressingMode::AddressRegisterIndirect);
}

TEST(M68kDecoderTests, DecodeMOVEQ) {
    // MOVEQ #-$10, D3 = 0x76F0  (imm = 0xF0 = -16, reg = 3)
    DecodedInstruction inst = M68kDecoder::Decode(0x76F0);
    EXPECT_EQ(inst.type, OpType::MOVEQ);
    EXPECT_EQ(inst.size, OperandSize::LONG);
    EXPECT_EQ(static_cast<std::int32_t>(inst.immediateData), -16);
    EXPECT_EQ(inst.destMode, AddressingMode::DataRegisterDirect);
    EXPECT_EQ(inst.destRegister, 3);
}

TEST(M68kDecoderTests, DecodeLEA) {
    // LEA (A0), A1 = 0x43D0  (opcode: 0100 001 111 010 000 = 0x43D0)
    DecodedInstruction inst = M68kDecoder::Decode(0x43D0);
    EXPECT_EQ(inst.type, OpType::LEA);
    EXPECT_EQ(inst.size, OperandSize::LONG);
    EXPECT_EQ(inst.srcMode, AddressingMode::AddressRegisterIndirect);
    EXPECT_EQ(inst.srcRegister, 0); // A0
    EXPECT_EQ(inst.destMode, AddressingMode::AddressRegisterDirect);
    EXPECT_EQ(inst.destRegister, 1); // A1
}

TEST(M68kDecoderTests, DecodeMOVEM_Store) {
    // MOVEM.L registers, -(A7) = 0x48E7  (eaMode = 4, eaReg = 7, size = LONG, bit10 = 0)
    DecodedInstruction inst = M68kDecoder::Decode(0x48E7);
    EXPECT_EQ(inst.type, OpType::MOVEM);
    EXPECT_EQ(inst.size, OperandSize::LONG);
    EXPECT_EQ(inst.destMode, AddressingMode::AddressRegisterPredecrement);
    EXPECT_EQ(inst.destRegister, 7); // A7
}

TEST(M68kDecoderTests, DecodeMOVEM_Load) {
    // MOVEM.W (A7)+, registers = 0x4C9F (eaMode = 3, eaReg = 7, size = WORD is bit 6 = 0, bit10 = 1)
    DecodedInstruction inst = M68kDecoder::Decode(0x4C9F);
    EXPECT_EQ(inst.type, OpType::MOVEM);
    EXPECT_EQ(inst.size, OperandSize::WORD);
    EXPECT_EQ(inst.srcMode, AddressingMode::AddressRegisterPostincrement);
    EXPECT_EQ(inst.srcRegister, 7); // A7
}

TEST(M68kDecoderTests, DecodeBSR_ShortAndWord) {
    // BSR.S displacement = 0x6110  (disp8 = 0x10)
    DecodedInstruction instS = M68kDecoder::Decode(0x6110);
    EXPECT_EQ(instS.type, OpType::BSR);
    EXPECT_EQ(instS.size, OperandSize::BYTE);

    // BSR.W displacement = 0x6100  (disp8 = 0x00)
    DecodedInstruction instW = M68kDecoder::Decode(0x6100);
    EXPECT_EQ(instW.type, OpType::BSR);
    EXPECT_EQ(instW.size, OperandSize::WORD);
}

TEST(M68kDecoderTests, DecodeCMP_L_D0_D1) {
    // CMP.L D0, D1 = 0xB280
    DecodedInstruction inst = M68kDecoder::Decode(0xB280);
    EXPECT_EQ(inst.type, OpType::CMP);
    EXPECT_EQ(inst.size, OperandSize::LONG);
    EXPECT_EQ(inst.srcMode, AddressingMode::DataRegisterDirect);
    EXPECT_EQ(inst.srcRegister, 0);
    EXPECT_EQ(inst.destMode, AddressingMode::DataRegisterDirect);
    EXPECT_EQ(inst.destRegister, 1);
}

TEST(M68kDecoderTests, DecodeCMPA_L_A0_A1) {
    // CMPA.L (A0), A1 = 0xB3D0
    // Encoding: 1011 001 111 010 000
    //   Bits[15:12] = 1011 (CMP group)
    //   Bits[11:9]  = 001  (dest = A1)
    //   Bits[8:6]   = 111  (opmode LONG = CMPA.L)
    //   Bits[5:3]   = 010  (EA mode = AddressRegisterIndirect)
    //   Bits[2:0]   = 000  (EA reg  = A0)
    // Note: 0xB2D0 would be CMPA.W (opmode=011), not CMPA.L (opmode=111)
    DecodedInstruction inst = M68kDecoder::Decode(0xB3D0);
    EXPECT_EQ(inst.type, OpType::CMP);
    EXPECT_EQ(inst.size, OperandSize::LONG);
    EXPECT_EQ(inst.srcMode, AddressingMode::AddressRegisterIndirect);
    EXPECT_EQ(inst.srcRegister, 0);
    EXPECT_EQ(inst.destMode, AddressingMode::AddressRegisterDirect);
    EXPECT_EQ(inst.destRegister, 1);
}