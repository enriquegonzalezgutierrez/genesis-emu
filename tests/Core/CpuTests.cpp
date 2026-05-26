// ==============================================================================
// GenesisEmu - M68k CPU Unit Tests (TDD - Updated with MOVE Immediate)
// ==============================================================================
// This file contains unit tests to verify CPU initialization (Reset),
// basic instruction execution (NOP), register, memory, and immediate moves.
// ==============================================================================

#include <gtest/gtest.h>
#include "M68k.h"

using namespace GenesisEmu::Core;

// ------------------------------------------------------------------------------
// Mock Bus for CPU Isolation Testing (Updated)
// ------------------------------------------------------------------------------
// A specialized mock bus that simulates a tiny ROM, records memory writes,
// and supports multi-word instructions (extension words for immediate values).
class CpuMockBus : public IBus {
public:
    Longword sspVector = 0x00FF0000; // Standard initial Stack Pointer
    Longword pcVector  = 0x00000100; // Standard entry point
    Word programmedOpcode = 0x4E71;  // Defaults to NOP instruction (0x4E71)
    Word extensionWord = 0x0000;     // Immediate data word following the opcode

    // Spy variables to record memory write operations
    Address lastWriteAddress = 0xFFFFFFFF;
    Word    lastWriteValue = 0x0000;

    Byte ReadByte([[maybe_unused]] Address address) override { return 0x00; }

    Word ReadWord(Address address) override {
        if (address == 0x000000) return static_cast<Word>(sspVector >> 16);
        if (address == 0x000002) return static_cast<Word>(sspVector & 0xFFFF);
        if (address == 0x000004) return static_cast<Word>(pcVector >> 16);
        if (address == 0x000006) return static_cast<Word>(pcVector & 0xFFFF);
        
        // Return our programmed opcode when the CPU fetches code at the PC
        if (address == pcVector) {
            return programmedOpcode;
        }
        // Return the immediate data word when the CPU reads the extension word (PC + 2)
        if (address == pcVector + 2) {
            return extensionWord;
        }
        return 0x0000;
    }

    Longword ReadLongword(Address address) override {
        if (address == 0x000000) return sspVector;
        if (address == 0x000004) return pcVector;
        return 0x00000000;
    }

    void WriteByte([[maybe_unused]] Address address, [[maybe_unused]] Byte data) override {}
    
    // Record word writes to verify memory-indirect store operations
    void WriteWord(Address address, Word data) override {
        lastWriteAddress = address;
        lastWriteValue = data;
    }
    
    void WriteLongword([[maybe_unused]] Address address, [[maybe_unused]] Longword data) override {}
    void AttachDevice([[maybe_unused]] IMemoryMappedDevice* device, [[maybe_unused]] Address start, [[maybe_unused]] Address end) override {}
};

// ------------------------------------------------------------------------------
// Test Suite: CpuExecutionTests
// ------------------------------------------------------------------------------

TEST(CpuExecutionTests, CpuResetLoadsSSPAndPC) {
    // 1. Arrange
    CpuMockBus mockBus;
    mockBus.sspVector = 0x00FFFE00;
    mockBus.pcVector  = 0x00002000;
    
    M68k cpu(&mockBus);

    // 2. Act
    cpu.Reset();

    // 3. Assert
    EXPECT_EQ(cpu.GetARegister(7), 0x00FFFE00);
    EXPECT_EQ(cpu.GetPC(), 0x00002000);
}

TEST(CpuExecutionTests, CpuStepExecutesNOP) {
    // 1. Arrange
    CpuMockBus mockBus;
    mockBus.pcVector = 0x001000;
    mockBus.programmedOpcode = 0x4E71;
    
    M68k cpu(&mockBus);
    cpu.Reset();

    // 2. Act
    int cycles = cpu.Step();

    // 3. Assert
    EXPECT_EQ(cycles, 4);
    EXPECT_EQ(cpu.GetPC(), 0x001002);
}

TEST(CpuExecutionTests, CpuExecutesMoveWord) {
    // 1. Arrange
    CpuMockBus mockBus;
    mockBus.pcVector = 0x001000;
    mockBus.programmedOpcode = 0x3200; // MOVE.W D0, D1
    
    M68k cpu(&mockBus);
    cpu.Reset();

    cpu.SetDRegister(0, 0x1234);
    cpu.SetDRegister(1, 0xFFFF);

    // 2. Act
    int cycles = cpu.Step();

    // 3. Assert
    EXPECT_EQ(cpu.GetDRegister(1), 0x1234);
    EXPECT_EQ(cpu.GetPC(), 0x001002);
    EXPECT_EQ(cycles, 4);

    EXPECT_FALSE(cpu.GetFlagZero());
    EXPECT_FALSE(cpu.GetFlagNegative());
    EXPECT_FALSE(cpu.GetFlagOverflow());
    EXPECT_FALSE(cpu.GetFlagCarry());
}

TEST(CpuExecutionTests, CpuExecutesMoveToMemoryIndirect) {
    // 1. Arrange
    CpuMockBus mockBus;
    mockBus.pcVector = 0x001000;
    mockBus.programmedOpcode = 0x3080; // MOVE.W D0, (A0)
    
    M68k cpu(&mockBus);
    cpu.Reset();

    // Initialize registers
    cpu.SetDRegister(0, 0xABCD);       // Data to store
    cpu.SetARegister(0, 0x00E00020);   // Target memory pointer (Work RAM offset)

    // 2. Act
    int cycles = cpu.Step();

    // 3. Assert
    EXPECT_EQ(mockBus.lastWriteAddress, 0x00E00020);
    EXPECT_EQ(mockBus.lastWriteValue, 0xABCD);
    EXPECT_EQ(cpu.GetPC(), 0x001002);
    EXPECT_EQ(cycles, 8);

    EXPECT_FALSE(cpu.GetFlagZero());
    EXPECT_TRUE(cpu.GetFlagNegative());
    EXPECT_FALSE(cpu.GetFlagOverflow());
    EXPECT_FALSE(cpu.GetFlagCarry());
}

TEST(CpuExecutionTests, CpuExecutesMoveToAddressRegister) {
    // 1. Arrange
    CpuMockBus mockBus;
    mockBus.pcVector = 0x001000;
    mockBus.programmedOpcode = 0x3040; // MOVEA.W D0, A0
    
    M68k cpu(&mockBus);
    cpu.Reset();

    cpu.SetSR(0x271F); // All CCR flags set to 1
    cpu.SetDRegister(0, 0x8000); 

    // 2. Act
    int cycles = cpu.Step();

    // 3. Assert
    EXPECT_EQ(cpu.GetARegister(0), 0xFFFF8000);
    EXPECT_EQ(cycles, 4);
    EXPECT_EQ(cpu.GetPC(), 0x001002);

    EXPECT_TRUE(cpu.GetFlagZero());
    EXPECT_TRUE(cpu.GetFlagNegative());
    EXPECT_TRUE(cpu.GetFlagOverflow());
    EXPECT_TRUE(cpu.GetFlagCarry());
    EXPECT_TRUE(cpu.GetFlagExtend());
}

TEST(CpuExecutionTests, CpuExecutesMoveImmediate) {
    // 1. Arrange
    CpuMockBus mockBus;
    mockBus.pcVector = 0x001000;
    mockBus.programmedOpcode = 0x303C; // 0x303C is: MOVE.W #$5678, D0
    mockBus.extensionWord    = 0x5678; // The immediate value stored at PC + 2
    
    M68k cpu(&mockBus);
    cpu.Reset();

    cpu.SetDRegister(0, 0x0000); // Clear destination register

    // 2. Act
    int cycles = cpu.Step();

    // 3. Assert
    // D0 must receive the immediate value 0x5678
    EXPECT_EQ(cpu.GetDRegister(0), 0x5678);
    // PC must advance by 4 bytes (2 bytes for opcode + 2 bytes for immediate data)
    EXPECT_EQ(cpu.GetPC(), 0x001004);
    // MOVE #<data>, Dn takes exactly 8 CPU clock cycles (4 for instruction + 4 for fetching extension word)
    EXPECT_EQ(cycles, 8);

    // CCR Flags update for 0x5678 (positive and non-zero)
    EXPECT_FALSE(cpu.GetFlagZero());
    EXPECT_FALSE(cpu.GetFlagNegative());
    EXPECT_FALSE(cpu.GetFlagOverflow());
    EXPECT_FALSE(cpu.GetFlagCarry());
}