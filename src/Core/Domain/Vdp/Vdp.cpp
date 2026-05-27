// ==============================================================================
// GenesisEmu - VDP Aggregate Root Implementation (Core Domain)
// ==============================================================================
// This file implements VRAM/CRAM read-write tunnels, auto-increment address steps,
// and hardware-level DMA block transfers.
//
// SOLID Compliance:
// 1. Single Responsibility Principle (SRP):
//    It manages strictly memory block interfaces and DMA copy mechanisms.
// ==============================================================================

#include "Vdp.h"

namespace GenesisEmu::Core::Domain::Vdp {

using namespace GenesisEmu::Core::Domain::Common;

Vdp::Vdp(IBus* bus) 
    : m_bus(bus)
    , m_vblankToggle(false) 
{
    m_vram.fill(0);
    m_cram.fill(0);
    m_vsram.fill(0);
}

// ------------------------------------------------------------------------------
// IMemoryMappedDevice Interface Overrides
// ------------------------------------------------------------------------------
Byte Vdp::ReadByte([[maybe_unused]] Address offset) {
    return 0x00; // Byte accesses are not supported by VDP data ports
}

Word Vdp::ReadWord(Address offset) {
    // Data Port Reads (offset 0x00 or 0x02)
    if (offset == 0x00 || offset == 0x02) {
        return ReadDataPort();
    }
    // Control Port Reads (offset 0x04 or 0x06)
    if (offset == 0x04 || offset == 0x06) {
        m_controlUnit.ResetFlipFlop(); 
        
        // Simulates the physical vertical blank status flag.
        // Toggles bit 3 on subsequent reads to prevent game loops from hanging
        // during frame synchronization cycles.
        m_vblankToggle = !m_vblankToggle;
        return m_vblankToggle ? 0x3608 : 0x3600;
    }
    return 0x0000;
}

void Vdp::WriteByte([[maybe_unused]] Address offset, [[maybe_unused]] Byte data) {
    // Byte writes are physically ignored by the Sega VDP interface
}

void Vdp::WriteWord(Address offset, Word data) {
    // Control Port Writes
    if (offset == 0x04 || offset == 0x06) {
        VdpCommand cmd = m_controlUnit.WriteControl(data);
        
        if (cmd.isValid) {
            // Check if the command specifies a DMA transfer operation (CD5/Bit 5 is set)
            if ((cmd.code & 0x20) != 0) {
                Word srcHigh = m_controlUnit.GetRegister(23);
                Byte dmaType = (srcHigh >> 6) & 0x03; // Bits 7-6 of Reg 23 define DMA Mode
                
                // Mode 2 is VRAM Fill (which triggers on a subsequent write to the Data Port).
                // Modes 0 and 1 are Memory-to-VRAM DMAs, which must execute immediately.
                if (dmaType != 2) {
                    ExecuteDMA();
                }
            }
        }
    } 
    // Data Port Writes
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

    Word srcHigh = m_controlUnit.GetRegister(23);
    Byte dmaType = (srcHigh >> 6) & 0x03;

    // --- High-Performance VRAM Fill DMA Block ---
    // If the control code specifies DMA (Bit 5 set) and DMA Mode is VRAM Fill (Type 2)
    if ((code & 0x20) != 0 && dmaType == 2) {
        Word dmaLenLow  = m_controlUnit.GetRegister(19);
        Word dmaLenHigh = m_controlUnit.GetRegister(20);
        Word dmaLength  = (dmaLenHigh << 8) | dmaLenLow;

        // In VRAM Fill, the upper byte of the written word acts as the fill pattern
        Byte fillValue = static_cast<Byte>(data >> 8); 
        Byte autoIncrement = m_controlUnit.GetRegister(15);

        // First, write the lower byte of the data word directly to the target address
        m_vram[targetAddress & 0xFFFF] = static_cast<Byte>(data & 0xFF);
        targetAddress = (targetAddress + autoIncrement) & 0xFFFF;

        // Fast block-fill of the remaining length
        for (Word i = 0; i < dmaLength; ++i) {
            m_vram[targetAddress & 0xFFFF] = fillValue;
            targetAddress = (targetAddress + autoIncrement) & 0xFFFF;
        }

        m_controlUnit.UpdateTargetAddress(targetAddress);
        return;
    }

    // Standard Direct CPU Writes
    Byte actualCode = code & 0x1F; // Extract lower 5 bits of VDP control code
    
    if (actualCode == 0x01) {
        // VRAM Write (splits the 16-bit word across VRAM byte indices)
        m_vram[targetAddress & 0xFFFF]       = static_cast<Byte>(data >> 8);
        m_vram[(targetAddress + 1) & 0xFFFF] = static_cast<Byte>(data & 0xFF);
    } 
    else if (actualCode == 0x03) {
        // CRAM Write (color index targeting)
        m_cram[targetAddress & 0x7F]       = static_cast<Byte>(data >> 8);
        m_cram[(targetAddress + 1) & 0x7F] = static_cast<Byte>(data & 0xFF);
    }

    Byte autoIncrement = m_controlUnit.GetRegister(15);
    m_controlUnit.UpdateTargetAddress((targetAddress + autoIncrement) & 0xFFFF);
}

Word Vdp::ReadDataPort() {
    return 0x0000; // Standard stub for reading data port
}

// ------------------------------------------------------------------------------
// VDP Hardware DMA Copier Engine
// ------------------------------------------------------------------------------
void Vdp::ExecuteDMA() {
    if (!m_bus) return; 

    // 1. Decode DMA Copy Length (Registers 19 and 20)
    Word dmaLenLow  = m_controlUnit.GetRegister(19);
    Word dmaLenHigh = m_controlUnit.GetRegister(20);
    Word dmaLength  = (dmaLenHigh << 8) | dmaLenLow;

    // 2. Decode DMA Source Address (Registers 21, 22, 23)
    Word srcLow   = m_controlUnit.GetRegister(21);
    Word srcMid   = m_controlUnit.GetRegister(22);
    Word srcHigh  = m_controlUnit.GetRegister(23); 
    
    // Assemble the physical 24-bit source address (shifted left by 1 word boundaries)
    Address dmaSource = (((srcHigh & 0x3F) << 16) | (srcMid << 8) | srcLow) << 1;

    Address targetAddress = m_controlUnit.GetTargetAddress();
    Byte code = m_controlUnit.GetControlCode() & 0x1F; 
    Byte autoIncrement = m_controlUnit.GetRegister(15);

    // 3. Perform High-Speed Block Copy
    for (Word i = 0; i < dmaLength; ++i) {
        Word data = m_bus->ReadWord(dmaSource);
        
        if (code == 0x01) {
            // Memory-to-VRAM Copy
            m_vram[targetAddress & 0xFFFF]       = static_cast<Byte>(data >> 8);
            m_vram[(targetAddress + 1) & 0xFFFF] = static_cast<Byte>(data & 0xFF);
        }
        else if (code == 0x03) {
            // Memory-to-CRAM Copy
            m_cram[targetAddress & 0x7F]       = static_cast<Byte>(data >> 8);
            m_cram[(targetAddress + 1) & 0x7F] = static_cast<Byte>(data & 0xFF);
        }

        dmaSource = (dmaSource + 2) & 0x00FFFFFF;
        targetAddress = (targetAddress + autoIncrement) & 0xFFFF;
    }

    m_controlUnit.UpdateTargetAddress(targetAddress);
}

} // namespace GenesisEmu::Core::Domain::Vdp