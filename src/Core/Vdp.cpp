// ==============================================================================
// GenesisEmu - VDP Implementation (Core Domain)
// ==============================================================================
// This file implements VRAM and CRAM access routines and delegates control 
// command parsing to the VdpControlUnit component.
// Upgraded with a high-performance hardware DMA transfer copier.
// ==============================================================================

#include "Vdp.h"

namespace GenesisEmu::Core {

Vdp::Vdp(IBus* bus) : m_bus(bus), m_vblankToggle(false) {
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
    if (offset == 0x00 || offset == 0x02) {
        return ReadDataPort();
    }
    if (offset == 0x04 || offset == 0x06) {
        m_controlUnit.ResetFlipFlop(); 
        
        m_vblankToggle = !m_vblankToggle;
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
        VdpCommand cmd = m_controlUnit.WriteControl(data);
        
        // If a 32-bit write cycle completes, inspect if bit 5 (CD5) is active
        if (cmd.isValid) {
            if ((cmd.code & 0x20) != 0) {
                ExecuteDMA();
            }
        }
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

// ------------------------------------------------------------------------------
// VDP Hardware DMA Copier Engine
// ------------------------------------------------------------------------------
void Vdp::ExecuteDMA() {
    if (!m_bus) return; // Guard protection if motherboard bus is detached

    // 1. Fetch DMA Length (Registers 19 and 20)
    Word dmaLenLow  = m_controlUnit.GetRegister(19);
    Word dmaLenHigh = m_controlUnit.GetRegister(20);
    Word dmaLength  = (dmaLenHigh << 8) | dmaLenLow;

    // 2. Fetch DMA Source Address (Registers 21, 22, 23)
    Word srcLow   = m_controlUnit.GetRegister(21);
    Word srcMid   = m_controlUnit.GetRegister(22);
    Word srcHigh  = m_controlUnit.GetRegister(23); // Bit 6 determines DMA type (0 = Memory-to-VRAM)
    
    // Compute physical source byte address: Source address in registers is expressed in WORDS,
    // so we shift left by 1 (multiply by 2) to get the actual byte location on the bus.
    Address dmaSource = (((srcHigh & 0x3F) << 16) | (srcMid << 8) | srcLow) << 1;

    Address targetAddress = m_controlUnit.GetTargetAddress();
    Byte code = m_controlUnit.GetControlCode() & 0x1F; // Clear DMA command bit (CD5) to get target type
    Byte autoIncrement = m_controlUnit.GetRegister(15);

    // 3. Perform high-speed block transfer
    for (Word i = 0; i < dmaLength; ++i) {
        // Fetch 16-bit data block from motherboard bus
        Word data = m_bus->ReadWord(dmaSource);
        
        if (code == 0x01) {
            // VRAM Write
            m_vram[targetAddress & 0xFFFF]       = static_cast<Byte>(data >> 8);
            m_vram[(targetAddress + 1) & 0xFFFF] = static_cast<Byte>(data & 0xFF);
        }
        else if (code == 0x03) {
            // CRAM Write
            m_cram[targetAddress & 0x7F]       = static_cast<Byte>(data >> 8);
            m_cram[(targetAddress + 1) & 0x7F] = static_cast<Byte>(data & 0xFF);
        }

        // Advance pointers based on auto-increment register
        dmaSource = (dmaSource + 2) & 0x00FFFFFF;
        targetAddress = (targetAddress + autoIncrement) & 0xFFFF;
    }

    // Update final VDP internal address register state
    m_controlUnit.UpdateTargetAddress(targetAddress);
}

} // namespace GenesisEmu::Core