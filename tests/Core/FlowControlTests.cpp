// ==============================================================================
// GenesisEmu - Flow Control Unit Tests (Corrected with DDD Namespaces)
// ==============================================================================
// This file contains unit tests to verify stack mechanics and program flow
// jumps (JSR, BSR, RTS) in absolute isolation from instruction fetching.
// ==============================================================================

#include <gtest/gtest.h>
#include "M68kFlowControl.h"
#include "IBus.h"

using namespace GenesisEmu::Core::Domain::Common;
using namespace GenesisEmu::Core::Domain::M68k;

// ------------------------------------------------------------------------------
// Isolated Mock Bus for Flow Control Testing
// ------------------------------------------------------------------------------
class FlowControlMockBus : public IBus {
public:
    // Simulated RAM space
    std::uint32_t ram[1024] = {0}; 

    Byte ReadByte([[maybe_unused]] Address address) override { return 0; }
    Word ReadWord([[maybe_unused]] Address address) override { return 0; }

    Longword ReadLongword(Address address) override {
        // Simple map address to our local ram array (starting at $00FF0000)
        if (address >= 0x00FF0000 && address < 0x00FF1000) {
            std::size_t idx = (address - 0x00FF0000) / 4;
            return ram[idx];
        }
        return 0;
    }

    void WriteByte([[maybe_unused]] Address address, [[maybe_unused]] Byte data) override {}
    void WriteWord([[maybe_unused]] Address address, [[maybe_unused]] Word data) override {}

    void WriteLongword(Address address, Longword data) override {
        if (address >= 0x00FF0000 && address < 0x00FF1000) {
            std::size_t idx = (address - 0x00FF0000) / 4;
            ram[idx] = data;
        }
    }

    void AttachDevice([[maybe_unused]] IMemoryMappedDevice* device, [[maybe_unused]] Address start, [[maybe_unused]] Address end) override {}
};

// ------------------------------------------------------------------------------
// Test Cases: Flow Control Unit
// ------------------------------------------------------------------------------

TEST(FlowControlTests, ExecuteJSRPushesPCAndJumps) {
    FlowControlMockBus bus;
    Address pc = 0x00001004; // Simulate current execution PC
    Longword sp = 0x00FF0F00; // Simulated Stack Pointer in RAM ($00FF0F00)
    Address target = 0x00002500;

    int cycles = M68kFlowControl::ExecuteJSR(&bus, pc, sp, target);

    // Verify clock cycles
    EXPECT_EQ(cycles, 16);

    // Stack pointer must have decremented by 4 bytes: 0x00FF0F00 - 4 = 0x00FF0EFC
    EXPECT_EQ(sp, 0x00FF0EFC);

    // The return address must be written to the top of the stack (0x00FF0EFC)
    EXPECT_EQ(bus.ReadLongword(0x00FF0EFC), 0x00001004);

    // Program Counter must point directly to target
    EXPECT_EQ(pc, target);
}

TEST(FlowControlTests, ExecuteRTSLoadsPCAndIncrementsSP) {
    FlowControlMockBus bus;
    Address pc = 0x00009999; 
    Longword sp = 0x00FF0EFC; // Stack Pointer pointing to stored return address

    // Pre-load return address onto the stack at $00FF0EFC
    bus.WriteLongword(0x00FF0EFC, 0x00001004);

    int cycles = M68kFlowControl::ExecuteRTS(&bus, pc, sp);

    EXPECT_EQ(cycles, 16);

    // Stack pointer must have incremented back by 4 bytes: 0x00FF0EFC + 4 = 0x00FF0F00
    EXPECT_EQ(sp, 0x00FF0F00);

    // PC must have loaded the return address
    EXPECT_EQ(pc, 0x00001004);
}

TEST(FlowControlTests, ExecuteBSRCalculatesDisplacementAndPushes) {
    FlowControlMockBus bus;
    Address pc = 0x00001004; // Pointing past BSR opcode + displacement
    Longword sp = 0x00FF0F00;
    Address instAddress = 0x00001000; // BSR.W started at $1000
    std::int16_t displacement = 0x0020; // +32 bytes displacement

    int cycles = M68kFlowControl::ExecuteBSR(&bus, pc, sp, displacement, instAddress);

    EXPECT_EQ(cycles, 18);

    // Verify stack pointer change (0x00FF0F00 - 4 = 0x00FF0EFC)
    EXPECT_EQ(sp, 0x00FF0EFC);

    // Verify return address pushed is 'pc'
    EXPECT_EQ(bus.ReadLongword(0x00FF0EFC), 0x00001004);

    // Target calculation: (instAddress + 2) + displacement => (0x1000 + 2) + 32 = 0x1022
    EXPECT_EQ(pc, 0x00001022);
}