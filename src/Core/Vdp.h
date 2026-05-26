// ==============================================================================
// GenesisEmu - VDP (Video Display Processor) Domain Model Header (Updated)
// ==============================================================================
// Added direct IBus* bus pointer binding and declared ExecuteDMA() to emulate
// high-speed DMA graphics transfers from system ROM/RAM into VRAM/CRAM.
// ==============================================================================

#pragma once

#include "IMemoryMappedDevice.h"
#include "VdpControlUnit.h"
#include "IBus.h" // Bind to standard system bus for DMA transfers
#include <array>

namespace GenesisEmu::Core {

class Vdp : public IMemoryMappedDevice {
public:
    // Bind VDP to the motherboard bus to allow the internal DMA controller
    // to perform direct block copies from ROM/RAM into VRAM/CRAM.
    explicit Vdp(IBus* bus = nullptr);
    ~Vdp() override = default;

    // --- IMemoryMappedDevice Interface Overrides ---
    Byte ReadByte(Address offset) override;
    Word ReadWord(Address offset) override;
    void WriteByte(Address offset, Byte data) override;
    void WriteWord(Address offset, Word data) override;

    // --------------------------------------------------------------------------
    // State Inspection (Getters for TDD / Debugging)
    // --------------------------------------------------------------------------
    Byte GetRegister(int index) const { return m_controlUnit.GetRegister(index); }
    Address GetTargetAddress() const { return m_controlUnit.GetTargetAddress(); }
    
    // Direct VRAM & CRAM Inspectors (Required for Decoupled VdpRenderer)
    Byte ReadVramDirect(Address addr) const { return m_vram[addr & 0xFFFF]; }
    Byte ReadCramDirect(Address addr) const { return m_cram[addr & 0x7F]; } 

private:
    // --------------------------------------------------------------------------
    // VDP Internal Memories (Encapsulated DDD Entities)
    // --------------------------------------------------------------------------
    std::array<Byte, 0x10000> m_vram;   // 64 KB Video RAM (Tiles, Tilemaps, Sprites)
    std::array<Byte, 128>     m_cram;   // 128 Bytes Color RAM (Palettes)
    std::array<Byte, 80>      m_vsram;  // 80 Bytes Vertical Scroll RAM

    // Encapsulated Control Unit Component (Delegation Pattern)
    VdpControlUnit m_controlUnit;

    // Pointer to the motherboard bus to perform DMA reads
    IBus* m_bus;

    // Simulated refresh state tracking
    bool m_vblankToggle;

    // --------------------------------------------------------------------------
    // Private Command Processors & DMA Engine
    // --------------------------------------------------------------------------
    void WriteDataPort(Word data);
    Word ReadDataPort();

    /**
     * @brief Executes a hardware-level DMA copy transfer from IBus into VRAM/CRAM.
     */
    void ExecuteDMA();
};

} // namespace GenesisEmu::Core