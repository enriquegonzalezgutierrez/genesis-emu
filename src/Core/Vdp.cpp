// ==============================================================================
// GenesisEmu - VDP Implementation (Core Domain)
// ==============================================================================
// This file implements VRAM and CRAM access routines and delegates control 
// command parsing to the VdpControlUnit component.
// Upgraded with a high-performance VRAM Fill DMA block copy mechanism.
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
        
        if (cmd.isValid) {
            // Memory-to-VRAM DMA copies are triggered immediately by the Control Port.
            // VRAM Fill DMA copies (dmaType == 2) wait for a subsequent write to the Data Port.
            if ((cmd.code & 0x20) != 0) {
                Word srcHigh = m_controlUnit.GetRegister(23);
                Byte dmaType = (srcHigh >> 6) & 0x03;
                if (dmaType != 2) {
                    ExecuteDMA();
                }
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

    // Check if the VDP is currently configured for a VRAM Fill DMA transfer
    Word srcHigh = m_controlUnit.GetRegister(23);
    Byte dmaType = (srcHigh >> 6) & 0x03;

    // --- ADDED: Execute VRAM Fill DMA ---
    if ((code & 0x20) != 0 && dmaType == 2) {
        Word dmaLenLow  = m_controlUnit.GetRegister(19);
        Word dmaLenHigh = m_controlUnit.GetRegister(20);
        Word dmaLength  = (dmaLenHigh << 8) | dmaLenLow;

        Byte fillValue = static_cast<Byte>(data >> 8); // Upper byte of the write is the fill value
        Byte autoIncrement = m_controlUnit.GetRegister(15);

        // First, the VDP writes the lower byte of the received word to the current target
        m_vram[targetAddress & 0xFFFF] = static_cast<Byte>(data & 0xFF);
        targetAddress = (targetAddress + autoIncrement) & 0xFFFF;

        // Fill subsequent bytes with the fill value
        for (Word i = 0; i < dmaLength; ++i) {
            m_vram[targetAddress & 0xFFFF] = fillValue;
            targetAddress = (targetAddress + autoIncrement) & 0xFFFF;
        }

        m_controlUnit.UpdateTargetAddress(targetAddress);
        return;
    }

    // Standard Direct CPU Write
    Byte actualCode = code & 0x1F;
    if (actualCode == 0x01) {
        m_vram[targetAddress & 0xFFFF]       = static_cast<Byte>(data >> 8);
        m_vram[(targetAddress + 1) & 0xFFFF] = static_cast<Byte>(data & 0xFF);
    } 
    else if (actualCode == 0x03) {
        m_cram[targetAddress & 0x7F]       = static_cast<Byte>(data >> 8);
        m_cram[(targetAddress + 1) & 0x7F] = static_cast<Byte>(data & 0xFF);
    }

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
    if (!m_bus) return; 

    // 1. Fetch DMA Length (Registers 19 and 20)
    Word dmaLenLow  = m_controlUnit.GetRegister(19);
    Word dmaLenHigh = m_controlUnit.GetRegister(20);
    Word dmaLength  = (dmaLenHigh << 8) | dmaLenLow;

    // 2. Fetch DMA Source Address (Registers 21, 22, 23)
    Word srcLow   = m_controlUnit.GetRegister(21);
    Word srcMid   = m_controlUnit.GetRegister(22);
    Word srcHigh  = m_controlUnit.GetRegister(23); 
    
    // Compute physical source byte address
    Address dmaSource = (((srcHigh & 0x3F) << 16) | (srcMid << 8) | srcLow) << 1;

    Address targetAddress = m_controlUnit.GetTargetAddress();
    Byte code = m_controlUnit.GetControlCode() & 0x1F; 
    Byte autoIncrement = m_controlUnit.GetRegister(15);

    // 3. Perform high-speed block transfer (Memory-to-VRAM/CRAM)
    for (Word i = 0; i < dmaLength; ++i) {
        Word data = m_bus->ReadWord(dmaSource);
        
        if (code == 0x01) {
            m_vram[targetAddress & 0xFFFF]       = static_cast<Byte>(data >> 8);
            m_vram[(targetAddress + 1) & 0xFFFF] = static_cast<Byte>(data & 0xFF);
        }
        else if (code == 0x03) {
            m_cram[targetAddress & 0x7F]       = static_cast<Byte>(data >> 8);
            m_cram[(targetAddress + 1) & 0x7F] = static_cast<Byte>(data & 0xFF);
        }

        dmaSource = (dmaSource + 2) & 0x00FFFFFF;
        targetAddress = (targetAddress + autoIncrement) & 0xFFFF;
    }

    m_controlUnit.UpdateTargetAddress(targetAddress);
}

} // namespace GenesisEmu::Core