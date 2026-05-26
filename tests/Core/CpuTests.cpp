// ==============================================================================
// GenesisEmu - M68k CPU Unit Tests (TDD - Corrected with maybe_unused)
// ==============================================================================
// This file contains unit tests to verify CPU initialization (Reset) and
// basic instruction execution (NOP) before the concrete CPU code is written.
// ==============================================================================

#include <gtest/gtest.h>
#include "M68k.h"

using namespace GenesisEmu::Core;

// ------------------------------------------------------------------------------
// Mock Bus for CPU Isolation Testing
// ------------------------------------------------------------------------------
// A specialized mock bus that simulates a tiny ROM. It allows setting up 
// initial vectors (SSP, PC) and programming raw opcodes at specific addresses.
class CpuMockBus : public IBus {
public:
    Longword sspVector = 0x00FF0000; // Standard initial Stack Pointer
    Longword pcVector  = 0x00000100; // Standard entry point
    Word programmedOpcode = 0x4E71;  // Defaults to NOP instruction (0x4E71)

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
    void WriteWord([[maybe_unused]] Address address, [[maybe_unused]] Word data) override {}
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
    // A7 (SP) must load Vector 0 (Initial SSP)
    EXPECT_EQ(cpu.GetARegister(7), 0x00FFFE00);
    // PC must load Vector 1 (Initial PC)
    EXPECT_EQ(cpu.GetPC(), 0x00002000);
}

TEST(CpuExecutionTests, CpuStepExecutesNOP) {
    // 1. Arrange
    CpuMockBus mockBus;
    mockBus.pcVector = 0x001000;
    mockBus.programmedOpcode = 0x4E71; // 0x4E71 is the M68k NOP opcode
    
    M68k cpu(&mockBus);
    cpu.Reset(); // Loads PC with 0x001000

    // 2. Act
    int cycles = cpu.Step();

    // 3. Assert
    // NOP takes exactly 4 CPU clock cycles
    EXPECT_EQ(cycles, 4);
    // PC must advance by 2 bytes (size of the NOP instruction word)
    EXPECT_EQ(cpu.GetPC(), 0x001002);
}