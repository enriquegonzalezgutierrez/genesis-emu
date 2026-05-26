// ==============================================================================
// GenesisEmu - VDP Implementation (Core Domain)
// ==============================================================================
// This file implements VRAM and CRAM access routines and delegates control 
// command parsing to the VdpControlUnit component.
// ==============================================================================

#include "Vdp.h"

namespace GenesisEmu::Core {

Vdp::Vdp() {
    // Clear all internal memory spaces on boot
    m_vram.fill(0);
    m_cram.fill(0);
    m_vsram.fill(0);
}

// ------------------------------------------------------------------------------
// IMemoryMappedDevice Interface Overrides
// ------------------------------------------------------------------------------
Byte Vdp::ReadByte([[maybe_unused]] Address offset) {
    return 0x00;
}

Word Vdp::ReadWord(Address offset) {
    if (offset == 0x00) {
        return ReadDataPort();
    }
    if (offset == 0x04) {
        // Reading the control port resets the flip-flop
        m_controlUnit.ResetFlipFlop(); 
        return 0x3400;
    }
    return 0x0000;
}

void Vdp::WriteByte([[maybe_unused]] Address offset, [[maybe_unused]] Byte data) {
    // Byte writes are ignored in standard mode
}

void Vdp::WriteWord(Address offset, Word data) {
    if (offset == 0x04) {
        m_controlUnit.WriteControl(data);
    } else if (offset == 0x00) {
        WriteDataPort(data);
    }
}

// ------------------------------------------------------------------------------
// Data Port Write Processor
// ------------------------------------------------------------------------------
void Vdp::WriteDataPort(Word data) {
    Byte code = m_controlUnit.GetControlCode();
    Address targetAddress = m_controlUnit.GetTargetAddress();

    // 1. Route write based on decoded Command Code
    if (code == 0x01) {
        // VRAM Write (Code 0x01)
        m_vram[targetAddress & 0xFFFF]       = static_cast<Byte>(data >> 8);
        m_vram[(targetAddress + 1) & 0xFFFF] = static_cast<Byte>(data & 0xFF);
    } 
    else if (code == 0x03) {
        // CRAM Write (Code 0x03)
        // CRAM holds 128 bytes (64 Color Words)
        m_cram[targetAddress & 0x7F]       = static_cast<Byte>(data >> 8);
        m_cram[(targetAddress + 1) & 0x7F] = static_cast<Byte>(data & 0xFF);
    }

    // 2. Apply the configured auto-increment step from VDP register 15
    Byte autoIncrement = m_controlUnit.GetRegister(15);
    m_controlUnit.UpdateTargetAddress((targetAddress + autoIncrement) & 0xFFFF);
}

Word Vdp::ReadDataPort() {
    return 0x0000;
}

} // namespace GenesisEmu::Core