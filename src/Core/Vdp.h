// ==============================================================================
// GenesisEmu - VDP (Video Display Processor) Domain Model Header (Updated)
// ==============================================================================
// Added direct CRAM (Color RAM) accessor to resolve graphics coloring bindings
// within the decoupled rendering pipeline.
// ==============================================================================

#pragma once

#include "IMemoryMappedDevice.h"
#include "VdpControlUnit.h"
#include <array>

namespace GenesisEmu::Core {

class Vdp : public IMemoryMappedDevice {
public:
    Vdp();
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
    Byte ReadCramDirect(Address addr) const { return m_cram[addr & 0x7F]; } // Safely mirrors to 128 bytes limits

private:
    // --------------------------------------------------------------------------
    // VDP Internal Memories (Encapsulated DDD Entities)
    // --------------------------------------------------------------------------
    std::array<Byte, 0x10000> m_vram;   // 64 KB Video RAM (Tiles, Tilemaps, Sprites)
    std::array<Byte, 128>     m_cram;   // 128 Bytes Color RAM (Palettes)
    std::array<Byte, 80>      m_vsram;  // 80 Bytes Vertical Scroll RAM

    // Encapsulated Control Unit Component (Delegation Pattern)
    VdpControlUnit m_controlUnit;

    // --------------------------------------------------------------------------
    // Private Command Processors
    // --------------------------------------------------------------------------
    void WriteDataPort(Word data);
    Word ReadDataPort();
};

} // namespace GenesisEmu::Core