// ==============================================================================
// GenesisEmu - M68k CPU Unit Tests (TDD - Updated with Math & Logic)
// ==============================================================================
// This file contains unit tests to verify CPU initialization, execution loop,
// register/memory moves, jumps, branches, postincrement, and math operations.
// ==============================================================================

#include <gtest/gtest.h>
#include "M68k.h"

using namespace GenesisEmu::Core;

// ------------------------------------------------------------------------------
// Mock Bus for CPU Isolation Testing (Updated)
// ------------------------------------------------------------------------------
class CpuMockBus : public IBus {
public:
    Longword sspVector = 0x00FF0000; // Standard initial Stack Pointer
    Longword pcVector  = 0x00000100; // Standard entry point
    Word programmedOpcode = 0x4E71;  // Defaults to NOP instruction (0x4E71)
    
    // Support for up to two extension words
    Word extensionWord1 = 0x0000;     
    Word extensionWord2 = 0x0000;     

    // Spy variables to record memory write operations
    Address lastWriteAddress = 0xFFFFFFFF;
    Word    lastWriteValue = 0x0000;

    Byte ReadByte([[maybe_unused]] Address address) override { return 0x00; }

    Word ReadWord(Address address) override {
        if (address == 0x000000) return static_cast<Word>(sspVector >> 16);
        if (address == 0x000002) return static_cast<Word>(sspVector & 0xFFFF);
        if (address == 0x000004) return static_cast<Word>(pcVector >> 16);
        if (address == 0x000006) return static_cast<Word>(pcVector & 0xFFFF);
        
        // Return simulated array RAM data at $00E00020 for postincrement test
        if (address == 0x00E00020) {
            return 0xABCD;
        }

        // Return our programmed opcode when the CPU fetches code at the PC
        if (address == pcVector) {
            return programmedOpcode;
        }
        // Return the first extension word (PC + 2)
        if (address == pcVector + 2) {
            return extensionWord1;
        }
        // Return the second extension word (PC + 4)
        if (address == pcVector + 4) {
            return extensionWord2;
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
    mockBus.programmedOpcode = 0x303C; // MOVE.W #$5678, D0
    mockBus.extensionWord1   = 0x5678; // The immediate value stored at PC + 2
    
    M68k cpu(&mockBus);
    cpu.Reset();

    cpu.SetDRegister(0, 0x0000); // Clear destination register

    // 2. Act
    int cycles = cpu.Step();

    // 3. Assert
    EXPECT_EQ(cpu.GetDRegister(0), 0x5678);
    EXPECT_EQ(cpu.GetPC(), 0x001004);
    EXPECT_EQ(cycles, 8);

    EXPECT_FALSE(cpu.GetFlagZero());
    EXPECT_FALSE(cpu.GetFlagNegative());
    EXPECT_FALSE(cpu.GetFlagOverflow());
    EXPECT_FALSE(cpu.GetFlagCarry());
}

TEST(CpuExecutionTests, CpuExecutesJmpAbsoluteLong) {
    // 1. Arrange
    CpuMockBus mockBus;
    mockBus.pcVector = 0x001000;
    mockBus.programmedOpcode = 0x4EF9; // JMP (xxx).L
    mockBus.extensionWord1   = 0x00FF; // Target address high word ($00FF)
    mockBus.extensionWord2   = 0x0000; // Target address low word ($0000) => Full destination: $00FF0000
    
    M68k cpu(&mockBus);
    cpu.Reset();

    // 2. Act
    int cycles = cpu.Step();

    // 3. Assert
    EXPECT_EQ(cpu.GetPC(), 0x00FF0000);
    EXPECT_EQ(cycles, 16);
}

TEST(CpuExecutionTests, CpuExecutesBraWord) {
    // 1. Arrange
    CpuMockBus mockBus;
    mockBus.pcVector = 0x001000;
    mockBus.programmedOpcode = 0x6000; // BRA.W
    mockBus.extensionWord1   = 0x0020; // Signed 16-bit displacement: +32 bytes
    
    M68k cpu(&mockBus);
    cpu.Reset();

    // 2. Act
    int cycles = cpu.Step();

    // 3. Assert
    EXPECT_EQ(cpu.GetPC(), 0x001022);
    EXPECT_EQ(cycles, 10);
}

TEST(CpuExecutionTests, CpuExecutesBneWordTaken) {
    // 1. Arrange
    CpuMockBus mockBus;
    mockBus.pcVector = 0x001000;
    mockBus.programmedOpcode = 0x6600; // BNE.W
    mockBus.extensionWord1   = 0x0020; // Signed 16-bit displacement: +32 bytes
    
    M68k cpu(&mockBus);
    cpu.Reset();
    
    cpu.SetSR(0x2700); // Clear all flags (Z = 0)

    // 2. Act
    int cycles = cpu.Step();

    // 3. Assert
    EXPECT_EQ(cpu.GetPC(), 0x001022);
    EXPECT_EQ(cycles, 10);
}

TEST(CpuExecutionTests, CpuExecutesBneWordNotTaken) {
    // 1. Arrange
    CpuMockBus mockBus;
    mockBus.pcVector = 0x001000;
    mockBus.programmedOpcode = 0x6600; // BNE.W
    mockBus.extensionWord1   = 0x0020; // Signed 16-bit displacement: +32 bytes
    
    M68k cpu(&mockBus);
    cpu.Reset();
    
    cpu.SetSR(0x2704); // Z flag (bit 2) is active

    // 2. Act
    int cycles = cpu.Step();

    // 3. Assert
    EXPECT_EQ(cpu.GetPC(), 0x001004);
    EXPECT_EQ(cycles, 8);
}

TEST(CpuExecutionTests, CpuExecutesBeqWordTaken) {
    // 1. Arrange
    CpuMockBus mockBus;
    mockBus.pcVector = 0x001000;
    mockBus.programmedOpcode = 0x6700; // BEQ.W
    mockBus.extensionWord1   = 0x0020; // Signed 16-bit displacement: +32 bytes
    
    M68k cpu(&mockBus);
    cpu.Reset();
    
    cpu.SetSR(0x2704); // Z flag (bit 2) is active

    // 2. Act
    int cycles = cpu.Step();

    // 3. Assert
    EXPECT_EQ(cpu.GetPC(), 0x001022);
    EXPECT_EQ(cycles, 10);
}

TEST(CpuExecutionTests, CpuExecutesMovePostincrement) {
    // 1. Arrange
    CpuMockBus mockBus;
    mockBus.pcVector = 0x001000;
    mockBus.programmedOpcode = 0x3218; // MOVE.W (A0)+, D1
    
    M68k cpu(&mockBus);
    cpu.Reset();

    cpu.SetARegister(0, 0x00E00020);
    cpu.SetDRegister(1, 0x0000);

    // 2. Act
    int cycles = cpu.Step();

    // 3. Assert
    EXPECT_EQ(cpu.GetDRegister(1), 0xABCD);
    EXPECT_EQ(cpu.GetARegister(0), 0x00E00022);
    EXPECT_EQ(cpu.GetPC(), 0x001002);
    EXPECT_EQ(cycles, 8);
}

TEST(CpuExecutionTests, CpuExecutesAddWord) {
    // 1. Arrange
    CpuMockBus mockBus;
    mockBus.pcVector = 0x001000;
    mockBus.programmedOpcode = 0xD240; // 0xD240 is: ADD.W D0, D1 (Add D0 to D1)
    
    M68k cpu(&mockBus);
    cpu.Reset();

    cpu.SetDRegister(0, 0x0005); // Source value
    cpu.SetDRegister(1, 0x000A); // Destination value (will become 5 + 10 = 15)

    // 2. Act
    int cycles = cpu.Step();

    // 3. Assert
    EXPECT_EQ(cpu.GetDRegister(1), 0x000F); // 15
    EXPECT_EQ(cpu.GetPC(), 0x001002);
    EXPECT_EQ(cycles, 4); // ADD Dn, Dn takes 4 clock cycles
}

TEST(CpuExecutionTests, CpuExecutesSubWord) {
    // 1. Arrange
    CpuMockBus mockBus;
    mockBus.pcVector = 0x001000;
    mockBus.programmedOpcode = 0x9240; // 0x9240 is: SUB.W D0, D1 (Subtract D0 from D1)
    
    M68k cpu(&mockBus);
    cpu.Reset();

    cpu.SetDRegister(0, 0x0005); // Source value to subtract
    cpu.SetDRegister(1, 0x000F); // Destination value (will become 15 - 5 = 10)

    // 2. Act
    int cycles = cpu.Step();

    // 3. Assert
    EXPECT_EQ(cpu.GetDRegister(1), 0x000A); // 10
    EXPECT_EQ(cpu.GetPC(), 0x001002);
    EXPECT_EQ(cycles, 4); // SUB Dn, Dn takes 4 clock cycles
}

TEST(CpuExecutionTests, CpuExecutesAndWord) {
    // 1. Arrange
    CpuMockBus mockBus;
    mockBus.pcVector = 0x001000;
    mockBus.programmedOpcode = 0xC240; // 0xC240 is: AND.W D0, D1 (Logical AND)
    
    M68k cpu(&mockBus);
    cpu.Reset();

    cpu.SetDRegister(0, 0x00FF); // Bitmask
    cpu.SetDRegister(1, 0x5555); // Value (will become 0x5555 AND 0x00FF = 0x0055)

    // 2. Act
    int cycles = cpu.Step();

    // 3. Assert
    EXPECT_EQ(cpu.GetDRegister(1), 0x0055);
    EXPECT_EQ(cpu.GetPC(), 0x001002);
    EXPECT_EQ(cycles, 4); // AND Dn, Dn takes 4 clock cycles
}

TEST(CpuExecutionTests, CpuExecutesDBFBranchTaken) {
    // 1. Arrange
    CpuMockBus mockBus;
    mockBus.pcVector = 0x001000;
    mockBus.programmedOpcode = 0x51CA; // DBF D2
    mockBus.extensionWord1   = 0x0020; // displacement: +32 bytes
    
    M68k cpu(&mockBus);
    cpu.Reset();

    cpu.SetDRegister(2, 0x0005); // Loop counter

    // 2. Act
    int cycles = cpu.Step();

    // 3. Assert
    EXPECT_EQ(cpu.GetDRegister(2) & 0xFFFF, 0x0004); // decremented
    EXPECT_EQ(cpu.GetPC(), 0x001022); // branched: 0x1002 + 0x20
    EXPECT_EQ(cycles, 10);
}

TEST(CpuExecutionTests, CpuExecutesDBFBranchNotTaken) {
    // 1. Arrange
    CpuMockBus mockBus;
    mockBus.pcVector = 0x001000;
    mockBus.programmedOpcode = 0x51CA; // DBF D2
    mockBus.extensionWord1   = 0x0020; // displacement: +32 bytes
    
    M68k cpu(&mockBus);
    cpu.Reset();

    cpu.SetDRegister(2, 0x0000); // Loop counter is 0, will become -1 (0xFFFF)

    // 2. Act
    int cycles = cpu.Step();

    // 3. Assert
    EXPECT_EQ(cpu.GetDRegister(2) & 0xFFFF, 0xFFFF); // decremented to -1
    EXPECT_EQ(cpu.GetPC(), 0x001004); // no branch: next instruction (PC + 4)
    EXPECT_EQ(cycles, 14);
}

TEST(CpuExecutionTests, CpuExecutesANDIWord) {
    // 1. Arrange
    CpuMockBus mockBus;
    mockBus.pcVector = 0x001000;
    mockBus.programmedOpcode = 0x0240; // ANDI.W #data, D0
    mockBus.extensionWord1   = 0x00FF; // immediate value
    
    M68k cpu(&mockBus);
    cpu.Reset();

    cpu.SetDRegister(0, 0x5555);

    // 2. Act
    int cycles = cpu.Step();

    // 3. Assert
    EXPECT_EQ(cpu.GetDRegister(0) & 0xFFFF, 0x0055);
    EXPECT_EQ(cpu.GetPC(), 0x001004); // Opcode + 1 extension word
    EXPECT_EQ(cycles, 8);
    EXPECT_FALSE(cpu.GetFlagZero());
    EXPECT_FALSE(cpu.GetFlagNegative());
}

TEST(CpuExecutionTests, CpuExecutesORIWord) {
    // 1. Arrange
    CpuMockBus mockBus;
    mockBus.pcVector = 0x001000;
    mockBus.programmedOpcode = 0x0041; // ORI.W #data, D1
    mockBus.extensionWord1   = 0x0F00; // immediate value
    
    M68k cpu(&mockBus);
    cpu.Reset();

    cpu.SetDRegister(1, 0x0055);

    // 2. Act
    int cycles = cpu.Step();

    // 3. Assert
    EXPECT_EQ(cpu.GetDRegister(1) & 0xFFFF, 0x0F55);
    EXPECT_EQ(cpu.GetPC(), 0x001004); // Opcode + 1 extension word
    EXPECT_EQ(cycles, 8);
    EXPECT_FALSE(cpu.GetFlagZero());
    EXPECT_FALSE(cpu.GetFlagNegative());
}

// ------------------------------------------------------------------------------
// ADDQ / SUBQ - Quick Arithmetic (no extension word consumed)
// ------------------------------------------------------------------------------

TEST(CpuExecutionTests, ADDQ_Word_IncrementsRegisterByImmediate) {
    // ADDQ.W #4, D2 = 0x5842  (bit8=0 => ADDQ, bits11-9=100 => imm 4, WORD, D2)
    // Verifies that no extension word is read (PC advances by exactly 2).
    CpuMockBus mockBus;
    mockBus.pcVector         = 0x001000;
    mockBus.programmedOpcode = 0x5842; // ADDQ.W #4, D2

    M68k cpu(&mockBus);
    cpu.Reset();
    cpu.SetDRegister(2, 0x0010);

    int cycles = cpu.Step();

    EXPECT_EQ(cpu.GetDRegister(2) & 0xFFFF, 0x0014); // 0x10 + 4 = 0x14
    EXPECT_EQ(cpu.GetPC(), 0x001002);                  // Only opcode consumed (2 bytes)
    EXPECT_EQ(cycles, 4);
}

TEST(CpuExecutionTests, SUBQ_Word_DecrementsRegisterByImmediate) {
    // SUBQ.W #1, D2 = 0x5342
    // Verifies that no extension word is read (PC advances by exactly 2).
    CpuMockBus mockBus;
    mockBus.pcVector         = 0x001000;
    mockBus.programmedOpcode = 0x5342; // SUBQ.W #1, D2

    M68k cpu(&mockBus);
    cpu.Reset();
    cpu.SetDRegister(2, 0x000A);

    int cycles = cpu.Step();

    EXPECT_EQ(cpu.GetDRegister(2) & 0xFFFF, 0x0009); // 0x0A - 1 = 0x09
    EXPECT_EQ(cpu.GetPC(), 0x001002);                  // Only opcode consumed (2 bytes)
    EXPECT_EQ(cycles, 4);
}

TEST(CpuExecutionTests, SUBQ_Word_SetsZeroFlagWhenResultIsZero) {
    // SUBQ.W #1, D0 = 0x5340, D0 starts at 1 → result 0 → Z flag set
    CpuMockBus mockBus;
    mockBus.pcVector         = 0x001000;
    mockBus.programmedOpcode = 0x5340; // SUBQ.W #1, D0

    M68k cpu(&mockBus);
    cpu.Reset();
    cpu.SetDRegister(0, 0x0001);

    cpu.Step();

    EXPECT_EQ(cpu.GetDRegister(0) & 0xFFFF, 0x0000);
    EXPECT_TRUE(cpu.GetFlagZero());
}

// ------------------------------------------------------------------------------
// SWAP
// ------------------------------------------------------------------------------

TEST(CpuExecutionTests, SWAP_SwapsHighAndLowWords) {
    // SWAP D1 = 0x4841
    CpuMockBus mockBus;
    mockBus.pcVector         = 0x001000;
    mockBus.programmedOpcode = 0x4841; // SWAP D1

    M68k cpu(&mockBus);
    cpu.Reset();
    cpu.SetDRegister(1, 0xABCD1234);

    int cycles = cpu.Step();

    EXPECT_EQ(cpu.GetDRegister(1), 0x1234ABCDu); // Words swapped
    EXPECT_EQ(cpu.GetPC(), 0x001002);             // Only opcode consumed
    EXPECT_EQ(cycles, 4);
    EXPECT_FALSE(cpu.GetFlagZero());
}

TEST(CpuExecutionTests, SWAP_SetsZeroFlagForZeroResult) {
    // SWAP D0 = 0x4840 on value 0x00000000 → result still 0x00000000
    CpuMockBus mockBus;
    mockBus.pcVector         = 0x001000;
    mockBus.programmedOpcode = 0x4840; // SWAP D0

    M68k cpu(&mockBus);
    cpu.Reset();
    cpu.SetDRegister(0, 0x00000000);

    cpu.Step();

    EXPECT_EQ(cpu.GetDRegister(0), 0x00000000u);
    EXPECT_TRUE(cpu.GetFlagZero());
}

// ------------------------------------------------------------------------------
// EXT
// ------------------------------------------------------------------------------

TEST(CpuExecutionTests, EXT_ByteToWord_PositiveValue) {
    // EXT.W D3 = 0x4883  (byte 0x45 → word 0x0045)
    CpuMockBus mockBus;
    mockBus.pcVector         = 0x001000;
    mockBus.programmedOpcode = 0x4883; // EXT.W D3

    M68k cpu(&mockBus);
    cpu.Reset();
    cpu.SetDRegister(3, 0xFFFF0045); // low byte = 0x45

    cpu.Step();

    EXPECT_EQ(cpu.GetDRegister(3) & 0xFFFF, 0x0045);
    EXPECT_FALSE(cpu.GetFlagNegative());
    EXPECT_FALSE(cpu.GetFlagZero());
}

TEST(CpuExecutionTests, EXT_ByteToWord_NegativeValue) {
    // EXT.W D3 = 0x4883  (byte 0x80 → word 0xFF80)
    CpuMockBus mockBus;
    mockBus.pcVector         = 0x001000;
    mockBus.programmedOpcode = 0x4883; // EXT.W D3

    M68k cpu(&mockBus);
    cpu.Reset();
    cpu.SetDRegister(3, 0x00000080); // low byte = 0x80 (negative in signed byte)

    cpu.Step();

    EXPECT_EQ(cpu.GetDRegister(3) & 0xFFFF, 0xFF80);
    EXPECT_TRUE(cpu.GetFlagNegative());
    EXPECT_FALSE(cpu.GetFlagZero());
}

// ------------------------------------------------------------------------------
// MOVEQ / LEA / MOVEM / BSR.S Execution Verification Helpers
// ------------------------------------------------------------------------------

class CpuTestRamBus : public IBus {
public:
    std::vector<Byte> ram;
    CpuTestRamBus() { ram.resize(1024 * 1024, 0); }
    
    Byte ReadByte(Address address) override { return ram[address & 0xFFFFF]; }
    Word ReadWord(Address address) override {
        address &= 0xFFFFF;
        return (ram[address] << 8) | ram[address + 1];
    }
    Longword ReadLongword(Address address) override {
        return (static_cast<Longword>(ReadWord(address)) << 16) | ReadWord(address + 2);
    }
    void WriteByte(Address address, Byte data) override { ram[address & 0xFFFFF] = data; }
    void WriteWord(Address address, Word data) override {
        address &= 0xFFFFF;
        ram[address] = data >> 8;
        ram[address + 1] = data & 0xFF;
    }
    void WriteLongword(Address address, Longword data) override {
        WriteWord(address, data >> 16);
        WriteWord(address + 2, data & 0xFFFF);
    }
    void AttachDevice(IMemoryMappedDevice*, Address, Address) override {}
};

TEST(CpuExecutionTests, CpuExecutesMOVEQ) {
    CpuTestRamBus bus;
    // MOVEQ #-$20, D2 = 0x74E0
    bus.WriteWord(0x1000, 0x74E0);
    
    M68k cpu(&bus);
    cpu.Reset();
    cpu.SetPC(0x1000);
    
    int cycles = cpu.Step();
    EXPECT_EQ(cycles, 4);
    EXPECT_EQ(cpu.GetDRegister(2), 0xFFFFFFE0u);
    EXPECT_TRUE(cpu.GetFlagNegative());
    EXPECT_FALSE(cpu.GetFlagZero());
}

TEST(CpuExecutionTests, CpuExecutesLEA) {
    CpuTestRamBus bus;
    // LEA (A0), A1 = 0x43D0
    bus.WriteWord(0x1000, 0x43D0);
    
    M68k cpu(&bus);
    cpu.Reset();
    cpu.SetPC(0x1000);
    cpu.SetARegister(0, 0x123456);
    
    int cycles = cpu.Step();
    EXPECT_EQ(cycles, 8);
    EXPECT_EQ(cpu.GetARegister(1), 0x123456u);
}

TEST(CpuExecutionTests, CpuExecutesMOVEM_StorePredec) {
    CpuTestRamBus bus;
    // MOVEM.L D0-D2/A0-A1, -(A7)
    // opcode: 0x48E7  (eaMode = 4, eaReg = 7, size = LONG)
    // extension reg mask: 0xE0C0 (D0-D2, A0-A1 reversed for predecrement)
    bus.WriteWord(0x1000, 0x48E7);
    bus.WriteWord(0x1002, 0xE0C0);
    
    M68k cpu(&bus);
    cpu.Reset();
    cpu.SetPC(0x1000);
    
    cpu.SetARegister(7, 0x50000); // SP
    cpu.SetDRegister(0, 0x11111111);
    cpu.SetDRegister(1, 0x22222222);
    cpu.SetDRegister(2, 0x33333333);
    cpu.SetARegister(0, 0xAAAAAAAA);
    cpu.SetARegister(1, 0xBBBBBBBB);
    
    int cycles = cpu.Step();
    EXPECT_EQ(cycles, 48);
    EXPECT_EQ(cpu.GetARegister(7), 0x50000u - 20); // 5 registers * 4 bytes = 20 bytes
    
    // Check values pushed to stack (pre-decrement reversed order: A1, A0, D2, D1, D0)
    EXPECT_EQ(bus.ReadLongword(0x50000 - 4), 0xBBBBBBBB); // A1
    EXPECT_EQ(bus.ReadLongword(0x50000 - 8), 0xAAAAAAAA); // A0
    EXPECT_EQ(bus.ReadLongword(0x50000 - 12), 0x33333333); // D2
    EXPECT_EQ(bus.ReadLongword(0x50000 - 16), 0x22222222); // D1
    EXPECT_EQ(bus.ReadLongword(0x50000 - 20), 0x11111111); // D0
}

TEST(CpuExecutionTests, CpuExecutesMOVEM_LoadPostinc) {
    CpuTestRamBus bus;
    // MOVEM.L (A7)+, D0-D2/A0-A1
    // opcode: 0x4CDF  (eaMode = 3, eaReg = 7, size = LONG, load)
    // extension reg mask: D0-D2 (0x0007) | A0-A1 (0x0300) = 0x0307
    bus.WriteWord(0x1000, 0x4CDF);
    bus.WriteWord(0x1002, 0x0307);
    
    bus.WriteLongword(0x40000, 0x11111111);
    bus.WriteLongword(0x40004, 0x22222222);
    bus.WriteLongword(0x40008, 0x33333333);
    bus.WriteLongword(0x4000C, 0xAAAAAAAA);
    bus.WriteLongword(0x40010, 0xBBBBBBBB);
    
    M68k cpu(&bus);
    cpu.Reset();
    cpu.SetPC(0x1000);
    cpu.SetARegister(7, 0x40000);
    
    int cycles = cpu.Step();
    EXPECT_EQ(cycles, 52);
    EXPECT_EQ(cpu.GetDRegister(0), 0x11111111);
    EXPECT_EQ(cpu.GetDRegister(1), 0x22222222);
    EXPECT_EQ(cpu.GetDRegister(2), 0x33333333);
    EXPECT_EQ(cpu.GetARegister(0), 0xAAAAAAAA);
    EXPECT_EQ(cpu.GetARegister(1), 0xBBBBBBBB);
    EXPECT_EQ(cpu.GetARegister(7), 0x40014); // wait: 5 regs loaded => increments by 20 bytes (0x14)
}

TEST(CpuExecutionTests, CpuExecutesBSR_ShortDisplacement) {
    CpuTestRamBus bus;
    // BSR.S with displacement 0x12 => opcode 0x6112
    bus.WriteWord(0x1000, 0x6112);
    
    M68k cpu(&bus);
    cpu.Reset();
    cpu.SetPC(0x1000);
    cpu.SetARegister(7, 0x40000);
    
    int cycles = cpu.Step();
    EXPECT_EQ(cycles, 18);
    EXPECT_EQ(cpu.GetPC(), 0x1014); // 0x1000 + 2 + 0x12
    EXPECT_EQ(cpu.GetARegister(7), 0x3FFFC); // Stack pointer decremented
    EXPECT_EQ(bus.ReadLongword(0x3FFFC), 0x1002); // Return address pushed
}

TEST(CpuExecutionTests, CpuExecutesCMP_L) {
    CpuTestRamBus bus;
    // CMP.L D0, D1 = 0xB280 (compare D0 and D1)
    bus.WriteWord(0x1000, 0xB280);
    
    M68k cpu(&bus);
    cpu.Reset();
    cpu.SetPC(0x1000);
    
    // Compare equal
    cpu.SetDRegister(0, 0x12345678);
    cpu.SetDRegister(1, 0x12345678);
    cpu.Step();
    EXPECT_TRUE(cpu.GetFlagZero());
    EXPECT_FALSE(cpu.GetFlagNegative());

    // Compare D1 < D0 (Negative, Carry)
    cpu.SetPC(0x1000);
    cpu.SetDRegister(0, 0x20);
    cpu.SetDRegister(1, 0x10);
    cpu.Step();
    EXPECT_FALSE(cpu.GetFlagZero());
    EXPECT_TRUE(cpu.GetFlagNegative());
    EXPECT_TRUE(cpu.GetFlagCarry());
}