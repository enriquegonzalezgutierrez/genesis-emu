// ==============================================================================
// GenesisEmu - M68k CPU Decoder Unit Tests (Corrected with DDD Namespaces)
// ==============================================================================
// This file contains unit tests to verify that the instruction decoder
// correctly parses raw 16-bit opcodes into structured metadata.
// ==============================================================================

#include <gtest/gtest.h>
#include "M68kInstruction.h"
#include "M68kDecoder.h"

using namespace GenesisEmu::Core::Domain::Common;
using namespace GenesisEmu::Core::Domain::M68k;

// ------------------------------------------------------------------------------
// Test Suite: M68kDecoderTests
// ------------------------------------------------------------------------------

TEST(M68kDecoderTests, DecodeNOP) {
    DecodedInstruction inst = M68kDecoder::Decode(0x4E71);
    EXPECT_EQ(inst.type, OpType::NOP);
    EXPECT_EQ(inst.size, OperandSize::NONE);
}

TEST(M68kDecoderTests, DecodeMoveDataRegisterDirect) {
    DecodedInstruction inst = M68kDecoder::Decode(0x3200);

    EXPECT_EQ(inst.type, OpType::MOVE);
    EXPECT_EQ(inst.size, OperandSize::WORD);
    
    EXPECT_EQ(inst.srcMode, AddressingMode::DataRegisterDirect);
    EXPECT_EQ(inst.srcRegister, 0); // D0
    
    EXPECT_EQ(inst.destMode, AddressingMode::DataRegisterDirect);
    EXPECT_EQ(inst.destRegister, 1); // D1
}

TEST(M68kDecoderTests, DecodeDBF) {
    DecodedInstruction inst = M68kDecoder::Decode(0x51CA);

    EXPECT_EQ(inst.type, OpType::DBF);
    EXPECT_EQ(inst.size, OperandSize::WORD);
    EXPECT_EQ(inst.srcRegister, 2); // D2
    EXPECT_EQ(inst.destMode, AddressingMode::ProgramCounterDisplacement);
}

TEST(M68kDecoderTests, DecodeANDI) {
    DecodedInstruction inst = M68kDecoder::Decode(0x0240);

    EXPECT_EQ(inst.type, OpType::AND);
    EXPECT_EQ(inst.size, OperandSize::WORD);
    EXPECT_EQ(inst.srcMode, AddressingMode::Immediate);
    EXPECT_EQ(inst.destMode, AddressingMode::DataRegisterDirect);
    EXPECT_EQ(inst.destRegister, 0); // D0
}

TEST(M68kDecoderTests, DecodeUnknownOpcode) {
    DecodedInstruction inst = M68kDecoder::Decode(0xFFFF);
    EXPECT_EQ(inst.type, OpType::UNKNOWN);
}

// ------------------------------------------------------------------------------
// ADDQ / SUBQ - Quick Arithmetic
// ------------------------------------------------------------------------------

TEST(M68kDecoderTests, DecodeADDQ_Word_D2) {
    DecodedInstruction inst = M68kDecoder::Decode(0x5842);

    EXPECT_EQ(inst.type, OpType::ADDQ);
    EXPECT_EQ(inst.size, OperandSize::WORD);
    EXPECT_EQ(inst.immediateData, 4u);
    EXPECT_EQ(inst.destMode, AddressingMode::DataRegisterDirect);
    EXPECT_EQ(inst.destRegister, 2);
}

TEST(M68kDecoderTests, DecodeSUBQ_Word_D2) {
    DecodedInstruction inst = M68kDecoder::Decode(0x5342);

    EXPECT_EQ(inst.type, OpType::SUBQ);
    EXPECT_EQ(inst.size, OperandSize::WORD);
    EXPECT_EQ(inst.immediateData, 1u);
    EXPECT_EQ(inst.destMode, AddressingMode::DataRegisterDirect);
    EXPECT_EQ(inst.destRegister, 2);
}

TEST(M68kDecoderTests, DecodeSUBQ_Long_D0) {
    DecodedInstruction inst = M68kDecoder::Decode(0x5180);

    EXPECT_EQ(inst.type, OpType::SUBQ);
    EXPECT_EQ(inst.size, OperandSize::LONG);
    EXPECT_EQ(inst.immediateData, 8u);  
}

// ------------------------------------------------------------------------------
// SWAP / EXT - Unary operations
// ------------------------------------------------------------------------------

TEST(M68kDecoderTests, DecodeSWAP_NotPEA) {
    DecodedInstruction inst = M68kDecoder::Decode(0x4841);

    EXPECT_EQ(inst.type, OpType::SWAP);
    EXPECT_EQ(inst.size, OperandSize::LONG);
    EXPECT_EQ(inst.destMode, AddressingMode::DataRegisterDirect);
    EXPECT_EQ(inst.destRegister, 1);
}

TEST(M68kDecoderTests, DecodeEXT_WordSize) {
    DecodedInstruction inst = M68kDecoder::Decode(0x4883);

    EXPECT_EQ(inst.type, OpType::EXT);
    EXPECT_EQ(inst.size, OperandSize::WORD);
    EXPECT_EQ(inst.destRegister, 3);
}

TEST(M68kDecoderTests, DecodeEXT_LongSize) {
    DecodedInstruction inst = M68kDecoder::Decode(0x48C5);

    EXPECT_EQ(inst.type, OpType::EXT);
    EXPECT_EQ(inst.size, OperandSize::LONG);
    EXPECT_EQ(inst.destRegister, 5);
}

TEST(M68kDecoderTests, DecodePEA_StillWorks) {
    DecodedInstruction inst = M68kDecoder::Decode(0x4850);

    EXPECT_EQ(inst.type, OpType::PEA);
    EXPECT_EQ(inst.destMode, AddressingMode::AddressRegisterIndirect);
}

TEST(M68kDecoderTests, DecodeMOVEQ) {
    DecodedInstruction inst = M68kDecoder::Decode(0x76F0);
    EXPECT_EQ(inst.type, OpType::MOVEQ);
    EXPECT_EQ(inst.size, OperandSize::LONG);
    EXPECT_EQ(static_cast<std::int32_t>(inst.immediateData), -16);
    EXPECT_EQ(inst.destMode, AddressingMode::DataRegisterDirect);
    EXPECT_EQ(inst.destRegister, 3);
}

TEST(M68kDecoderTests, DecodeLEA) {
    DecodedInstruction inst = M68kDecoder::Decode(0x43D0);
    EXPECT_EQ(inst.type, OpType::LEA);
    EXPECT_EQ(inst.size, OperandSize::LONG);
    EXPECT_EQ(inst.srcMode, AddressingMode::AddressRegisterIndirect);
    EXPECT_EQ(inst.srcRegister, 0); // A0
    EXPECT_EQ(inst.destMode, AddressingMode::AddressRegisterDirect);
    EXPECT_EQ(inst.destRegister, 1); // A1
}

TEST(M68kDecoderTests, DecodeMOVEM_Store) {
    DecodedInstruction inst = M68kDecoder::Decode(0x48E7);
    EXPECT_EQ(inst.type, OpType::MOVEM);
    EXPECT_EQ(inst.size, OperandSize::LONG);
    EXPECT_EQ(inst.destMode, AddressingMode::AddressRegisterPredecrement);
    EXPECT_EQ(inst.destRegister, 7); // A7
}

TEST(M68kDecoderTests, DecodeMOVEM_Load) {
    DecodedInstruction inst = M68kDecoder::Decode(0x4C9F);
    EXPECT_EQ(inst.type, OpType::MOVEM);
    EXPECT_EQ(inst.size, OperandSize::WORD);
    EXPECT_EQ(inst.srcMode, AddressingMode::AddressRegisterPostincrement);
    EXPECT_EQ(inst.srcRegister, 7); // A7
}

// ------------------------------------------------------------------------------
// Branching & Logic
// ------------------------------------------------------------------------------

TEST(M68kDecoderTests, DecodeBcc_Branches) {
    DecodedInstruction inst1 = M68kDecoder::Decode(0x6610);
    EXPECT_EQ(inst1.type, OpType::BNE);
    EXPECT_EQ(inst1.size, OperandSize::BYTE);

    DecodedInstruction inst2 = M68kDecoder::Decode(0x6300);
    EXPECT_EQ(inst2.type, OpType::BLS);
    EXPECT_EQ(inst2.size, OperandSize::WORD);

    DecodedInstruction inst3 = M68kDecoder::Decode(0x6420);
    EXPECT_EQ(inst3.type, OpType::BCC);
    EXPECT_EQ(inst3.size, OperandSize::BYTE);
}

// ------------------------------------------------------------------------------
// Shift & Rotates (ASR, ASL, ROR, ROL)
// ------------------------------------------------------------------------------

TEST(M68kDecoderTests, DecodeShiftRotate_ASR_ASL_ROR_ROL) {
    DecodedInstruction inst1 = M68kDecoder::Decode(0xE280);
    EXPECT_EQ(inst1.type, OpType::ASR);
    EXPECT_EQ(inst1.size, OperandSize::LONG);
    EXPECT_EQ(inst1.srcMode, AddressingMode::Immediate);
    EXPECT_EQ(inst1.immediateData, 1u);
    EXPECT_EQ(inst1.destRegister, 0);

    DecodedInstruction inst2 = M68kDecoder::Decode(0xE561);
    EXPECT_EQ(inst2.type, OpType::ASL);
    EXPECT_EQ(inst2.size, OperandSize::WORD);
    EXPECT_EQ(inst2.srcMode, AddressingMode::DataRegisterDirect);
    EXPECT_EQ(inst2.srcRegister, 2); 
    EXPECT_EQ(inst2.destRegister, 1); 

    DecodedInstruction inst3 = M68kDecoder::Decode(0xE218);
    EXPECT_EQ(inst3.type, OpType::ROR);
    EXPECT_EQ(inst3.size, OperandSize::BYTE);
    EXPECT_EQ(inst3.srcMode, AddressingMode::Immediate);
    EXPECT_EQ(inst3.immediateData, 1u);
    EXPECT_EQ(inst3.destRegister, 0);
}

// ------------------------------------------------------------------------------
// Logical OR / AND
// ------------------------------------------------------------------------------

TEST(M68kDecoderTests, DecodeOR_Standard) {
    DecodedInstruction inst = M68kDecoder::Decode(0x8041);
    EXPECT_EQ(inst.type, OpType::OR);
    EXPECT_EQ(inst.size, OperandSize::WORD);
    EXPECT_EQ(inst.srcMode, AddressingMode::DataRegisterDirect);
    EXPECT_EQ(inst.srcRegister, 1); // D1
    EXPECT_EQ(inst.destMode, AddressingMode::DataRegisterDirect);
    EXPECT_EQ(inst.destRegister, 0); // D0
}

// ------------------------------------------------------------------------------
// Set Conditionally (Scc)
// ------------------------------------------------------------------------------

TEST(M68kDecoderTests, DecodeSCC_Standard) {
    DecodedInstruction inst = M68kDecoder::Decode(0x50C2);
    EXPECT_EQ(inst.type, OpType::SCC);
    EXPECT_EQ(inst.size, OperandSize::BYTE);
    EXPECT_EQ(inst.immediateData, 0x0); // Condition code 0 = T (True)
    EXPECT_EQ(inst.destMode, AddressingMode::DataRegisterDirect);
    EXPECT_EQ(inst.destRegister, 2); // D2
}

// ------------------------------------------------------------------------------
// Add/Subtract with Extend (ADDX / SUBX)
// ------------------------------------------------------------------------------

TEST(M68kDecoderTests, DecodeADDX_SUBX_Standard) {
    // ADDX.B D3, D3 = 0xD703
    DecodedInstruction inst1 = M68kDecoder::Decode(0xD703);
    EXPECT_EQ(inst1.type, OpType::ADDX);
    EXPECT_EQ(inst1.size, OperandSize::BYTE);
    EXPECT_EQ(inst1.srcMode, AddressingMode::DataRegisterDirect);
    EXPECT_EQ(inst1.srcRegister, 3); // D3
    EXPECT_EQ(inst1.destMode, AddressingMode::DataRegisterDirect);
    EXPECT_EQ(inst1.destRegister, 3); // D3

    // SUBX.W -(A0), -(A1) = 0x9348 (Predecrement)
    DecodedInstruction inst2 = M68kDecoder::Decode(0x9348);
    EXPECT_EQ(inst2.type, OpType::SUBX);
    EXPECT_EQ(inst2.size, OperandSize::WORD);
    EXPECT_EQ(inst2.srcMode, AddressingMode::AddressRegisterPredecrement);
    EXPECT_EQ(inst2.srcRegister, 0); // A0
    EXPECT_EQ(inst2.destMode, AddressingMode::AddressRegisterPredecrement);
    EXPECT_EQ(inst2.destRegister, 1); // A1
}

TEST(M68kDecoderTests, DecodeCMP_L_D0_D1) {
    DecodedInstruction inst = M68kDecoder::Decode(0xB280);
    EXPECT_EQ(inst.type, OpType::CMP);
    EXPECT_EQ(inst.size, OperandSize::LONG);
    EXPECT_EQ(inst.srcMode, AddressingMode::DataRegisterDirect);
    EXPECT_EQ(inst.srcRegister, 0);
    EXPECT_EQ(inst.destMode, AddressingMode::DataRegisterDirect);
    EXPECT_EQ(inst.destRegister, 1);
}

TEST(M68kDecoderTests, DecodeCMPA_L_A0_A1) {
    DecodedInstruction inst = M68kDecoder::Decode(0xB3D0);
    EXPECT_EQ(inst.type, OpType::CMP);
    EXPECT_EQ(inst.size, OperandSize::LONG);
    EXPECT_EQ(inst.srcMode, AddressingMode::AddressRegisterIndirect);
    EXPECT_EQ(inst.srcRegister, 0);
    EXPECT_EQ(inst.destMode, AddressingMode::AddressRegisterDirect);
    EXPECT_EQ(inst.destRegister, 1);
}