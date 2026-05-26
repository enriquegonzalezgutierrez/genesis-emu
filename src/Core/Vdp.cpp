// ==============================================================================
// GenesisEmu - VDP Implementation (Core Domain)
// ==============================================================================
// This file implements VRAM and CRAM access routines and delegates control 
// command parsing to the VdpControlUnit component.
// Updated to set Bit 9 (FIFO Empty) to 1 in the Status Word (0x3600/0x3608)
// to satisfy game hardware synchronization wait loops.
// ==============================================================================

#include "Vdp.h"

namespace GenesisEmu::Core {

Vdp::Vdp() : m_vblankToggle(false) {
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
    // Data Port accesses are mirrored at offsets 0x00 and 0x02
    if (offset == 0x00 || offset == 0x02) {
        return ReadDataPort();
    }
    // Control Port accesses are mirrored at offsets 0x04 and 0x06
    if (offset == 0x04 || offset == 0x06) {
        m_controlUnit.ResetFlipFlop(); 
        
        m_vblankToggle = !m_vblankToggle;
        
        // Return status with Bit 9 (FIFO Empty) set to 1 => 0x3608 or 0x3600
        return m_vblankToggle ? 0x3608 : 0x3600;
    }
    return 0x0000;
}

void Vdp::WriteByte([[maybe_unused]] Address offset, [[maybe_unused]] Byte data) {
    // Byte writes are ignored in standard mode
}

void Vdp::WriteWord(Address offset, Word data) {
    // Control Port writes (offset 0x04/0x06)
    if (offset == 0x04 || offset == 0x06) {
        m_controlUnit.WriteControl(data);
    } 
    // Data Port writes (offset 0x00/0x02)
    else if (offset == 0x00 || offset == 0x02) {
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