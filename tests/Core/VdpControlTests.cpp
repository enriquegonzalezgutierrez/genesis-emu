// ==============================================================================
// GenesisEmu - VDP Control Unit Tests (Corrected with DDD Namespaces)
// ==============================================================================
// This file contains unit tests to verify the VdpControlUnit's register 
// management and 32-bit Command/Address decoding state machine in isolation.
// ==============================================================================

#include <gtest/gtest.h>
#include "VdpControlUnit.h"

using namespace GenesisEmu::Core::Domain::Common;
using namespace GenesisEmu::Core::Domain::Vdp;

// ------------------------------------------------------------------------------
// Test Cases: VdpControlUnit
// ------------------------------------------------------------------------------

TEST(VdpControlTests, WriteRegisterDirect) {
    VdpControlUnit ctrl;

    // Write value 0x02 to Register 15 (Auto-increment)
    // Command format: $8F02 => 0x8F00 | 0x02
    VdpCommand cmd = ctrl.WriteControl(0x8F02);

    // Register writes do not trigger a 32-bit memory command
    EXPECT_FALSE(cmd.isValid);
    EXPECT_EQ(ctrl.GetRegister(15), 0x02);
}

TEST(VdpControlTests, Decode32BitCommandWithFlipFlop) {
    VdpControlUnit ctrl;

    // Initiate 32-bit write setup to VRAM address $0000.
    // 1st Write: Address bits 13-0 | Command bits 31-16 (CD1-CD0) => $4000
    VdpCommand cmd1 = ctrl.WriteControl(0x4000);
    EXPECT_FALSE(cmd1.isValid); // 32-bit cycle is not complete yet

    // 2nd Write: Address bits 15-14 (placed in bits 1-0) | Command bits 35-32 (CD5-CD2 in bits 7-4) => $0000
    VdpCommand cmd2 = ctrl.WriteControl(0x0000);
    EXPECT_TRUE(cmd2.isValid); // 32-bit cycle is complete

    // Verify parsed results
    EXPECT_EQ(cmd2.targetAddress, 0x0000);
    EXPECT_EQ(cmd2.code, 0x01); // 0x01 corresponds to a standard VRAM Write
}

TEST(VdpControlTests, SecondWordLookingLikeRegisterWriteIsNotIntercepted) {
    VdpControlUnit ctrl;

    // 1st Write: Initiate VRAM Write setup (address $0000, command bits) => $4000
    VdpCommand cmd1 = ctrl.WriteControl(0x4000);
    EXPECT_FALSE(cmd1.isValid); // Not complete yet

    // 2nd Write: Send a second word that looks like a register write command.
    // Standard register write format: $8000 | (RegIndex << 8) | Value.
    // Value $8080 would mimic a write to Register 0 with value $80.
    // Under our corrected VDP, this must NOT write to Reg 0, but complete the 32-bit setup.
    VdpCommand cmd2 = ctrl.WriteControl(0x8080);
    
    EXPECT_TRUE(cmd2.isValid);
    EXPECT_EQ(cmd2.targetAddress, 0x0000);
    
    // Register 0 must NOT have been written (should remain default 0)
    EXPECT_EQ(ctrl.GetRegister(0), 0x00);
}

TEST(VdpControlTests, ResetFlipFlopManually) {
    VdpControlUnit ctrl;

    // Write first word of command
    ctrl.WriteControl(0x4000);

    // Reset flip-flop state manually (simulating a Control Port Read operation)
    ctrl.ResetFlipFlop();

    // Write another "first word" command. If flip-flop reset worked, this will 
    // be processed as a new first word instead of finishing the previous sequence.
    VdpCommand cmd = ctrl.WriteControl(0x4000);
    EXPECT_FALSE(cmd.isValid); 
}