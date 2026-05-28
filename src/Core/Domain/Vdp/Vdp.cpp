// ==============================================================================
// GenesisEmu - VDP Aggregate Root Implementation (Core Domain)
// ==============================================================================
// This file implements VRAM/CRAM read-write tunnels, the cycle-accurate DMA 
// engine, Scanline synchronization, and hardware interrupt counters.
//
// SOLID Compliance:
// 1. Single Responsibility Principle (SRP):
//    It manages strictly memory block interfaces and DMA copy mechanisms.
// ==============================================================================

#include "Vdp.h"
#include "VdpRenderer.h"
#include <iostream>

namespace GenesisEmu::Core::Domain::Vdp {

using namespace GenesisEmu::Core::Domain::Common;

Vdp::Vdp(IBus* bus) 
    : m_bus(bus)
    , m_vblankActive(false)
    , m_vblankPending(false)
    , m_frameCycles(0)
    , m_hintCounter(0)
    , m_dmaActive(false)
    , m_dmaFillPending(false)
    , m_dmaLength(0)
    , m_dmaSourceAddress(0)
    , m_dmaFillData(0)
{
    m_vram.fill(0);
    m_cram.fill(0);
    m_vsram.fill(0);
}

// ------------------------------------------------------------------------------
// Timing & Interrupt Mechanics
// ------------------------------------------------------------------------------
void Vdp::SetVblankActive(bool active) { 
    // Trigger the hardware Interrupt Pending flag on the rising edge of VBlank
    if (active && !m_vblankActive) {
        m_vblankPending = true;
    }
    m_vblankActive = active; 
}

bool Vdp::DecrementHintCounter() {
    if (m_hintCounter == 0) {
        // Reload from VDP Register 10
        m_hintCounter = m_controlUnit.GetRegister(10);
        
        // Trigger H-Int (Level 4) only if enabled in VDP Register 0, Bit 4 (IE1)
        return (m_controlUnit.GetRegister(0) & 0x10) != 0;
    } else {
        m_hintCounter--;
        return false;
    }
}

// ------------------------------------------------------------------------------
// IMemoryMappedDevice Interface Overrides
// ------------------------------------------------------------------------------
Byte Vdp::ReadByte(Address offset) {
    // The M68k reads individual bytes from the 16-bit VDP ports by reading 
    // the full word and extracting the requested half (Big-Endian).
    Word data = ReadWord(offset & ~1);
    
    if ((offset & 1) != 0) {
        return static_cast<Byte>(data & 0xFF);         // Odd address: lower byte
    } else {
        return static_cast<Byte>((data >> 8) & 0xFF);  // Even address: upper byte
    }
}

Word Vdp::ReadWord(Address offset) {
    // Data Port Reads (offset 0x00 or 0x02)
    if (offset == 0x00 || offset == 0x02) {
        m_controlUnit.ResetFlipFlop(); // CRITICAL: Reading Data Port resets pending command state
        return ReadDataPort();
    }
    
    // Control Port Reads (offset 0x04 or 0x06) - Status Register
    if (offset == 0x04 || offset == 0x06) {
        m_controlUnit.ResetFlipFlop();
        
        // Build the VDP Status Word.
        // Bit 9: FIFO Empty (Always 1 in our model unless DMA is locking it)
        // Bit 8: FIFO Full (Set to 0)
        // Bit 7: VIP (Vertical Interrupt Pending). Cleared upon read.
        // Bit 3: VBLANK active state.
        // Bit 1: DMA Busy
        Word status = 0x3600; // Base: 0x3400 + FIFO Empty (0x0200)
        
        if (m_dmaActive) {
            status |= 0x0002; // Bit 1: DMA is currently moving data
            status &= ~0x0200; // FIFO is not empty during DMA
        }
        if (m_vblankActive) {
            status |= 0x0008; // Bit 3: VBLANK
        }
        if (m_vblankPending) {
            status |= 0x0080; // Bit 7: VIP
            m_vblankPending = false; // Reset VIP status flag automatically upon read
        }
        
        return status;
    }
    
    // HV Counter Reads (offset 0x08 or 0x0A)
    if (offset == 0x08 || offset == 0x0A) {
        constexpr int cyclesPerLine = 488; 
        int line = m_frameCycles / cyclesPerLine;
        int h    = (m_frameCycles % cyclesPerLine) * 255 / cyclesPerLine; // Scale down to 8-bit
        
        Byte vCounter = static_cast<Byte>(line & 0xFF);
        Byte hCounter = static_cast<Byte>(h & 0xFF);
        
        return (static_cast<Word>(vCounter) << 8) | hCounter;
    }
    return 0x0000;
}

void Vdp::WriteByte([[maybe_unused]] Address offset, [[maybe_unused]] Byte data) {
    // Byte writes are physically ignored by the Sega VDP hardware.
}

void Vdp::WriteWord(Address offset, Word data) {
    // Control Port Writes
    if (offset == 0x04 || offset == 0x06) {
        VdpCommand cmd = m_controlUnit.WriteControl(data);
        
        if (cmd.isValid) {
            // Check if the command requests a DMA transfer (CD5/Bit 5 is set)
            if ((cmd.code & 0x20) != 0) {
                Word srcHigh = m_controlUnit.GetRegister(23);
                Byte dmaType = (srcHigh >> 6) & 0x03; // Bits 7-6 define DMA Mode
                
                if (dmaType == 2) {
                    // DMA Mode 2: VRAM Fill. 
                    // Arms the DMA but waits for the next Data Port write to fetch the pattern.
                    m_dmaFillPending = true;
                } else {
                    // DMA Mode 0 & 1: Memory-to-VRAM Copy.
                    // Start cycle-stealing execution immediately.
                    ArmDmaTransfer();
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
// Data Port & DMA Arming Logic
// ------------------------------------------------------------------------------
void Vdp::WriteDataPort(Word data) {
    m_controlUnit.ResetFlipFlop();
    
    if (m_dmaFillPending) {
        m_dmaFillPending = false;
        m_dmaFillData = data;
        ArmDmaTransfer();
        return; // The actual fill will happen in ProcessDma consuming CPU cycles
    }

    // Standard Direct CPU Writes
    Byte code = m_controlUnit.GetControlCode() & 0x1F; 
    Address targetAddress = m_controlUnit.GetTargetAddress();

    if (code == 0x01) {
        // VRAM Write (Swaps bytes if targeting an odd address)
        // STRICT CORRECTED FIX: Mask alignedAddress with 0xFFFF to prevent out-of-bounds memory corruption.
        Address alignedAddress = (targetAddress & 0xFFFE) & 0xFFFF;
        if ((targetAddress & 1) == 0) {
            m_vram[alignedAddress]     = static_cast<Byte>(data >> 8);
            m_vram[alignedAddress + 1] = static_cast<Byte>(data & 0xFF);
        } else {
            m_vram[alignedAddress]     = static_cast<Byte>(data & 0xFF);
            m_vram[alignedAddress + 1] = static_cast<Byte>(data >> 8);
        }
    } 
    else if (code == 0x03) {
        // CRAM Write
        Address alignedAddress = (targetAddress & 0xFFFE) & 0x7F;
        m_cram[alignedAddress]     = static_cast<Byte>(data >> 8);
        m_cram[alignedAddress + 1] = static_cast<Byte>(data & 0xFF);
    }
    else if (code == 0x05) {
        // VSRAM Write
        Address alignedAddress = (targetAddress & 0xFFFE) % 80;
        m_vsram[alignedAddress]     = static_cast<Byte>(data >> 8);
        m_vsram[(alignedAddress + 1)] = static_cast<Byte>(data & 0xFF);
    }

    Byte autoIncrement = m_controlUnit.GetRegister(15);
    m_controlUnit.UpdateTargetAddress((targetAddress + autoIncrement) & 0xFFFF);
}

Word Vdp::ReadDataPort() {
    Byte code = m_controlUnit.GetControlCode() & 0x1F;
    Address targetAddress = m_controlUnit.GetTargetAddress();
    Word result = 0;

    if (code == 0x00) {
        // VRAM Read
        Address addr = targetAddress & 0xFFFE;
        result = (static_cast<Word>(m_vram[addr & 0xFFFF]) << 8) |
                  m_vram[(addr + 1) & 0xFFFF];
    } else if (code == 0x08) {
        // CRAM Read
        Address addr = targetAddress & 0x7E;
        result = (static_cast<Word>(m_cram[addr]) << 8) | m_cram[addr + 1];
    } else if (code == 0x04) {
        // VSRAM Read
        Address addr = (targetAddress & 0x7E) % 80;
        result = (static_cast<Word>(m_vsram[addr]) << 8) |
                  m_vsram[(addr + 1) % 80];
    }

    // Auto-increment the target address after each read
    Byte autoIncrement = m_controlUnit.GetRegister(15);
    m_controlUnit.UpdateTargetAddress((targetAddress + autoIncrement) & 0xFFFF);

    return result;
}

void Vdp::ArmDmaTransfer() {
    Word dmaLenLow  = m_controlUnit.GetRegister(19);
    Word dmaLenHigh = m_controlUnit.GetRegister(20);
    m_dmaLength = (dmaLenHigh << 8) | dmaLenLow;
    
    // A DMA length of 0 translates to 65,536 words transferred
    if (m_dmaLength == 0) m_dmaLength = 0xFFFF; // We use 0xFFFF + 1 conceptually
    
    Word srcLow  = m_controlUnit.GetRegister(21);
    Word srcMid  = m_controlUnit.GetRegister(22);
    Word srcHigh = m_controlUnit.GetRegister(23); 
    
    // 16-bit word offset inside the 128KB DMA block
    m_dmaSourceAddress = (srcHigh & 0x7F) << 17 | (srcMid << 9) | (srcLow << 1);
    m_dmaActive = true;
}

// ------------------------------------------------------------------------------
// Cycle-Accurate DMA Copier Engine (With Strict Out-of-Bounds Protection)
// ------------------------------------------------------------------------------
int Vdp::ProcessDma(int cycleBudget) {
    if (!m_dmaActive || !m_bus) return 0;

    int cyclesConsumed = 0;
    Byte code = m_controlUnit.GetControlCode() & 0x1F;
    Byte autoIncrement = m_controlUnit.GetRegister(15);
    Address targetAddress = m_controlUnit.GetTargetAddress();
    
    Word srcHigh = m_controlUnit.GetRegister(23);
    Byte dmaType = (srcHigh >> 6) & 0x03;

    // Approximate cost: 2 CPU cycles per transferred Word
    while (cyclesConsumed < cycleBudget && m_dmaLength > 0) {
        Word dataToWrite = 0;

        if (dmaType == 2) {
            // VRAM Fill Mode (Only the high byte is used, XOR'd to write to even slots)
            dataToWrite = static_cast<Byte>(m_dmaFillData >> 8);
        } else {
            // Memory-to-VRAM Mode
            dataToWrite = m_bus->ReadWord(m_dmaSourceAddress);
            
            // Advance source address strictly within the 128KB hardware boundary bug
            Address sourceBlock = m_dmaSourceAddress & 0xFE0000;
            Address sourceOffset = (m_dmaSourceAddress + 2) & 0x01FFFF;
            m_dmaSourceAddress = sourceBlock | sourceOffset;
        }

        if (code == 0x01) { // VRAM
            if (dmaType == 2) {
                // FIXED: Mask with 0xFFFF to prevent heap corruption!
                Address fillAddress = (targetAddress ^ 1) & 0xFFFF;
                m_vram[fillAddress] = static_cast<Byte>(dataToWrite);
            } else {
                // FIXED: Mask with 0xFFFF to prevent heap corruption!
                Address alignedAddress = (targetAddress & 0xFFFE) & 0xFFFF;
                if ((targetAddress & 1) == 0) {
                    m_vram[alignedAddress]     = static_cast<Byte>(dataToWrite >> 8);
                    m_vram[alignedAddress + 1] = static_cast<Byte>(dataToWrite & 0xFF);
                } else {
                    m_vram[alignedAddress]     = static_cast<Byte>(dataToWrite & 0xFF);
                    m_vram[alignedAddress + 1] = static_cast<Byte>(dataToWrite >> 8);
                }
            }
        }
        else if (code == 0x03) { // CRAM
            Address alignedAddress = (targetAddress & 0xFFFE) & 0x7F;
            m_cram[alignedAddress]     = static_cast<Byte>(dataToWrite >> 8);
            m_cram[alignedAddress + 1] = static_cast<Byte>(dataToWrite & 0xFF);
        }
        else if (code == 0x05) { // VSRAM
            Address alignedAddress = (targetAddress & 0xFFFE) % 80;
            m_vsram[alignedAddress]     = static_cast<Byte>(dataToWrite >> 8);
            m_vsram[alignedAddress + 1] = static_cast<Byte>(dataToWrite & 0xFF);
        }

        targetAddress = (targetAddress + autoIncrement) & 0xFFFF;
        m_dmaLength--;
        cyclesConsumed += 2;
    }

    m_controlUnit.UpdateTargetAddress(targetAddress);
    
    // Update Source Address registers so the CPU sees the mutated state correctly
    Word currentSrcLow = static_cast<Word>((m_dmaSourceAddress >> 1) & 0xFFFF);
    m_controlUnit.SetRegister(21, currentSrcLow & 0xFF);
    m_controlUnit.SetRegister(22, (currentSrcLow >> 8) & 0xFF);

    if (m_dmaLength == 0) {
        // --- DEBUG TELEMETRY ---
        std::cout << "[VDP DMA] Transfer complete. Type: " 
                  << (int)((m_controlUnit.GetRegister(23) >> 6) & 0x03)
                  << " | Target: 0x" << std::hex << m_controlUnit.GetTargetAddress() 
                  << " | Code: " << (int)(m_controlUnit.GetControlCode() & 0x1F) << std::dec << std::endl;
        // -----------------------
        m_dmaActive = false; // Transfer completed, release CPU bus
        m_dmaFillPending = false;
    }

    return cyclesConsumed;
}

// ------------------------------------------------------------------------------
// Scanline Pixel Compositor 
// ------------------------------------------------------------------------------
void Vdp::RenderScanline(int scanline, std::uint32_t* frameBuffer) {
    constexpr int SCREEN_WIDTH = 320;
    
    // Calculate the pointer offset for the specific scanline inside the 320x224 buffer
    std::uint32_t* lineBuffer = frameBuffer + (scanline * SCREEN_WIDTH);

    // Get the Backdrop color index from VDP Register 7
    Byte bgIndex = m_controlUnit.GetRegister(7) & 0x3F;
    Byte colorHigh = ReadCramDirect(bgIndex * 2);
    Byte colorLow  = ReadCramDirect(bgIndex * 2 + 1);
    std::uint32_t backdropColor = VdpRenderer::ConvertColor(colorHigh, colorLow);

    // Initialize the line to the backdrop color
    for (int i = 0; i < SCREEN_WIDTH; ++i) {
        lineBuffer[i] = backdropColor;
    }

    std::uint32_t planeBLine[SCREEN_WIDTH] = {0};
    std::uint32_t planeALine[SCREEN_WIDTH] = {0};
    std::uint32_t spriteLine[SCREEN_WIDTH] = {0};

    VdpRenderer::RenderPlaneScanline(*this, 1, scanline, SCREEN_WIDTH, planeBLine); // Plane B
    VdpRenderer::RenderPlaneScanline(*this, 0, scanline, SCREEN_WIDTH, planeALine); // Plane A
    VdpRenderer::RenderSpritesScanline(*this, scanline, SCREEN_WIDTH, spriteLine);  // Sprites

    for (int x = 0; x < SCREEN_WIDTH; ++x) {
        if (spriteLine[x] != 0) {
            lineBuffer[x] = spriteLine[x];
        } else if (planeALine[x] != 0) {
            lineBuffer[x] = planeALine[x];
        } else if (planeBLine[x] != 0) {
            lineBuffer[x] = planeBLine[x];
        }
    }
}

} // namespace GenesisEmu::Core::Domain::Vdp