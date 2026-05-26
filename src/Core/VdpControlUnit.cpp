// ==============================================================================
// GenesisEmu - VDP Control Unit Implementation (Core Domain)
// ==============================================================================
// This file implements the VDP internal register file and the 32-bit control
// command decoding state machine.
// ==============================================================================

#include "VdpControlUnit.h"

namespace GenesisEmu::Core {

VdpControlUnit::VdpControlUnit()
    : m_writePending(false), m_registerLatch(0), m_targetAddress(0), m_controlCode(0) {
    m_registers.fill(0);
}

VdpCommand VdpControlUnit::WriteControl(Word data) {
    VdpCommand cmd;

    // 1. Check if this is a Register Write Command (Bit 15 is 1, Bit 14 is 0)
    // Format: $8000 OR (RegisterNumber << 8) OR Value
    if ((data & 0xC000) == 0x8000) {
        Byte regIndex = (data >> 8) & 0x1F; // Extract Register Number (5 bits)
        Byte regValue = data & 0xFF;        // Extract Value (8 bits)
        
        SetRegister(regIndex, regValue);
        
        // Register writes reset the flip-flop
        m_writePending = false;
        
        cmd.isValid = false; 
        return cmd;
    }

    // 2. Otherwise, this is a 32-bit Command/Address setup
    if (!m_writePending) {
        // First Word written: latch the value and wait for the second word
        m_registerLatch = data;
        m_writePending = true;
        
        cmd.isValid = false;
    } else {
        // Second Word written: decode full command code and internal target address
        // Target Address bits: [13-0] from 1st word, [15-14] from 2nd word bits [1-0]
        m_targetAddress = (m_registerLatch & 0x3FFF) | ((data & 0x0003) << 14);
        
        // Command Code bits: [1-0] from 1st word bits [15-14], [5-2] from 2nd word bits [7-4]
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

} // namespace GenesisEmu::Core