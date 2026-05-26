// ==============================================================================
// GenesisEmu - M68k CPU Unit Tests (TDD - Updated with Memory MOVE)
// ==============================================================================
// This file contains unit tests to verify CPU initialization (Reset),
// basic instruction execution (NOP), register moves, and memory-indirect moves.
// ==============================================================================

#include <gtest/gtest.h>
#include "M68k.h"

using namespace GenesisEmu::Core;

// ------------------------------------------------------------------------------
// Mock Bus for CPU Isolation Testing (Updated)
// ------------------------------------------------------------------------------
// A specialized mock bus that simulates a tiny ROM and records memory writes
// triggered by the CPU during memory-indirect instructions.
class CpuMockBus : public IBus {
public:
    Longword sspVector = 0x00FF0000; // Standard initial Stack Pointer
    Longword pcVector  = 0x00000100; // Standard entry point
    Word programmedOpcode = 0x4E71;  // Defaults to NOP instruction (0x4E71)

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
    mockBus.programmedOpcode = 0x3080; // 0x3080 is: MOVE.W D0, (A0)
    
    M68k cpu(&mockBus);
    cpu.Reset();

    // Initialize registers
    cpu.SetDRegister(0, 0xABCD);       // Data to store
    cpu.SetARegister(0, 0x00E00020);   // Target memory pointer (Work RAM offset)

    // 2. Act
    int cycles = cpu.Step();

    // 3. Assert
    // Verify that the data was written to the correct address on the bus
    EXPECT_EQ(mockBus.lastWriteAddress, 0x00E00020);
    EXPECT_EQ(mockBus.lastWriteValue, 0xABCD);
    
    // PC must advance by 2 bytes
    EXPECT_EQ(cpu.GetPC(), 0x001002);
    // MOVE Dn, (An) takes exactly 8 CPU clock cycles (4 for instruction, 4 for write access)
    EXPECT_EQ(cycles, 8);

    // Flags are updated based on the value written (0xABCD is non-zero, and negative: bit 15 is 1)
    EXPECT_FALSE(cpu.GetFlagZero());
    EXPECT_TRUE(cpu.GetFlagNegative());
    EXPECT_FALSE(cpu.GetFlagOverflow());
    EXPECT_FALSE(cpu.GetFlagCarry());
}