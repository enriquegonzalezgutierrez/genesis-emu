// ==============================================================================
// GenesisEmu - VDP Control Unit Implementation (Core Domain)
// ==============================================================================
// This file implements the VDP command and register write decoders.
//
// SOLID Compliance:
// 1. Single Responsibility Principle (SRP):
//    It is solely responsible for command decoding and latching.
// ==============================================================================

#include "VdpControlUnit.h"

namespace GenesisEmu::Core::Domain::Vdp {

using namespace GenesisEmu::Core::Domain::Common;

VdpControlUnit::VdpControlUnit()
    : m_writePending(false)
    , m_registerLatch(0)
    , m_targetAddress(0)
    , m_controlCode(0) 
{
    m_registers.fill(0);
}

VdpCommand VdpControlUnit::WriteControl(Word data) {
    VdpCommand cmd;

    // 1. Register Write Detection (Bits 15-14 are 1 and 0, corresponding to $8000)
    // Format: $8000 | (RegisterIndex << 8) | Value
    if ((data & 0xC000) == 0x8000) {
        Byte regIndex = (data >> 8) & 0x1F; // Extract Register Index (5 bits, registers 0-23)
        Byte regValue = data & 0xFF;        // Extract Register Value (8 bits)
        
        SetRegister(regIndex, regValue);
        
        // Writing directly to a register resets any pending 32-bit command writes
        m_writePending = false;
        
        cmd.isValid = false; 
        return cmd;
    }

    // 2. 32-Bit Command/Address Latching
    if (!m_writePending) {
        // First Word: store the word in the temporary register latch and wait for the second write
        m_registerLatch = data;
        m_writePending = true;
        
        cmd.isValid = false;
    } else {
        // Second Word: merge the register latch with the new write to form address and operation code
        // Target Address: bits [13-0] from the first write, bits [15-14] from the second write (at bits [1-0])
        m_targetAddress = (m_registerLatch & 0x3FFF) | ((data & 0x0003) << 14);
        
        // Command Code: bits [1-0] from the first write (at bits [15-14]), bits [5-2] from second write (at bits [7-4])
        m_controlCode = ((m_registerLatch & 0xC000) >> 14) | ((data & 0x00F0) >> 2);
        
        m_writePending = false;

        cmd.targetAddress = m_targetAddress;
        cmd.code          = m_controlCode;
        cmd.isValid       = true;
    }

    return cmd;
}

void VdpControlUnit::ResetFlipFlop() {
    m_writePending = false;
}

Byte VdpControlUnit::GetRegister(int index) const {
    if (index >= 0 && index < 24) {
        return m_registers[index];
    }
    return 0;
}

void VdpControlUnit::SetRegister(int index, Byte value) {
    if (index >= 0 && index < 24) {
        m_registers[index] = value;
    }
}

} // namespace GenesisEmu::Core::Domain::Vdp