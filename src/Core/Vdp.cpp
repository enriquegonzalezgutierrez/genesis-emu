// ==============================================================================
// GenesisEmu - VDP Implementation (Corrected with C++20 maybe_unused)
// ==============================================================================
// This file implements the VDP memory interfaces, register updates, and
// the 32-bit control command decoder (first/second word flip-flop).
// ==============================================================================

#include "Vdp.h"

namespace GenesisEmu::Core {

Vdp::Vdp() 
    : m_controlWritePending(false), m_controlRegisterLatch(0), 
      m_targetAddress(0), m_controlCode(0) {
    
    // Clear all internal memory spaces on boot
    m_vram.fill(0);
    m_cram.fill(0);
    m_vsram.fill(0);
    m_registers.fill(0);
}

// ------------------------------------------------------------------------------
// IMemoryMappedDevice Interface Overrides
// ------------------------------------------------------------------------------
Byte Vdp::ReadByte([[maybe_unused]] Address offset) {
    // Standard M68k read. In a complete emulator, bytes reads are routed.
    return 0x00;
}

Word Vdp::ReadWord(Address offset) {
    if (offset == 0x00) {
        return ReadDataPort();
    }
    if (offset == 0x04) {
        // Read Control Port returns VDP Status Register (Dummy value for now: 0x3400)
        m_controlWritePending = false; // Reading control port resets the flip-flop
        return 0x3400;
    }
    return 0x0000;
}

void Vdp::WriteByte([[maybe_unused]] Address offset, [[maybe_unused]] Byte data) {
    // VDP is mainly written to using 16-bit Word operations. 
    // Byte writes are safely ignored or processed as mirror operations.
}

void Vdp::WriteWord(Address offset, Word data) {
    if (offset == 0x04) {
        WriteControlPort(data);
    } else if (offset == 0x00) {
        WriteDataPort(data);
    }
}

// ------------------------------------------------------------------------------
// Control Port Command Processor (State Machine)
// ------------------------------------------------------------------------------
void Vdp::WriteControlPort(Word data) {
    // 1. Check if this is a Register Write Command (Bit 15 is 1, Bit 14 is 0)
    if ((data & 0xC000) == 0x8000) {
        Byte regIndex = (data >> 8) & 0x1F; // Extract Register Number (5 bits)
        Byte regValue = data & 0xFF;        // Extract Value (8 bits)
        
        if (regIndex < 24) {
            m_registers[regIndex] = regValue;
        }
        m_controlWritePending = false; // Register writes reset the flip-flop
        return;
    }

    // 2. Otherwise, this is a 32-bit Command/Address setup (Requires 2 writes)
    if (!m_controlWritePending) {
        // First Word: Latch the data and set the pending flag
        m_controlRegisterLatch = data;
        m_controlWritePending = true;
    } else {
        // Second Word: Combine both words to decode Code and Target Address
        // Command layout:
        // 1st Word:  CD1  CD0  A13  A12  A11  A10  A9   A8   A7   A6   A5   A4   A3   A2   A1   A0
        // 2nd Word:  ?    ?    ?    ?    ?    ?    ?    ?    CD5  CD4  CD3  CD2  ?    ?    A15  A14
        
        m_targetAddress = (m_controlRegisterLatch & 0x3FFF) | ((data & 0x0003) << 14);
        m_controlCode   = ((m_controlRegisterLatch & 0xC000) >> 14) | ((data & 0x00F0) >> 2);
        
        m_controlWritePending = false; // Reset flip-flop for the next command
    }
}

// ------------------------------------------------------------------------------
// Data Port Write Processor
// ------------------------------------------------------------------------------
void Vdp::WriteDataPort(Word data) {
    // VRAM Write Command Code is 0x01 (binary 000001)
    if (m_controlCode == 0x01) {
        // Big-Endian byte order write
        m_vram[m_targetAddress & 0xFFFF]       = static_cast<Byte>(data >> 8);
        m_vram[(m_targetAddress + 1) & 0xFFFF] = static_cast<Byte>(data & 0xFF);
    }
    // Note: CRAM (Code 0x03) and VSRAM (Code 0x05) writes will be routed here later

    // Apply auto-increment step from Register 15
    m_targetAddress = (m_targetAddress + m_registers[15]) & 0xFFFF;
}

Word Vdp::ReadDataPort() {
    // Stub read implementation
    return 0x0000;
}

} // namespace GenesisEmu::Core