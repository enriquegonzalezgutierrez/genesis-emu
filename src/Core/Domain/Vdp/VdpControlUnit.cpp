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

    // Register Write Detection.
    // Must only trigger when there is NO 32-bit command write currently pending.
    // Register command format: $8000 | (RegisterIndex << 8) | Value
    if (!m_writePending && ((data & 0xC000) == 0x8000)) {
        Byte regIndex = (data >> 8) & 0x1F; // Extract Register Index (5 bits, registers 0-23)
        Byte regValue = data & 0xFF;        // Extract Register Value (8 bits)
        
        SetRegister(regIndex, regValue);
        
        cmd.isValid = false; 
        return cmd;
    }

    // 32-Bit Command/Address Latching State Machine (The Flip-Flop)
    if (!m_writePending) {
        // First Word: store the word in the temporary register latch and wait for the second write
        m_registerLatch = data;
        m_writePending = true;

        // Hardware behavior: the first control port write immediately updates
        // bits 13-0 of the target address and CD1-CD0 of the control code.
        // Games rely on this partial update for Data Port operations between
        // the two halves of a 32-bit command sequence.
        m_targetAddress = (m_targetAddress & 0xC000) | (data & 0x3FFF);
        m_controlCode   = (m_controlCode & 0x3C) | ((data >> 14) & 0x03);

        cmd.isValid = false;
    } else {
        // Second Word: merge the current target address and register latch with the new write
        
        // Target Address: bits [13-0] are preserved from the CURRENT m_targetAddress
        // (which might have been auto-incremented by Data Port accesses),
        // bits [15-14] from the second write (at bits [1-0])
        m_targetAddress = (m_targetAddress & 0x3FFF) | ((data & 0x0003) << 14);
        
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