// ==============================================================================
// GenesisEmu - VDP (Video Display Processor) Domain Model Header
// ==============================================================================
// This class represents the VDP graphics chip. It delegates register management 
// and command parsing to VdpControlUnit, keeping its design strictly focused 
// on video memory buffers and rendering logic.
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
    // These respond directly to M68k bus accesses.
    // Address offsets: 0x00 = Data Port ($C00000), 0x04 = Control Port ($C00004)
    Byte ReadByte(Address offset) override;
    Word ReadWord(Address offset) override;
    void WriteByte(Address offset, Byte data) override;
    void WriteWord(Address offset, Word data) override;

    // --------------------------------------------------------------------------
    // State Inspection (Getters for TDD / Debugging)
    // --------------------------------------------------------------------------
    Byte GetRegister(int index) const { return m_controlUnit.GetRegister(index); }
    Address GetTargetAddress() const { return m_controlUnit.GetTargetAddress(); }
    Byte ReadVramDirect(Address addr) const { return m_vram[addr & 0xFFFF]; }

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
    // Handles writes to the VdpData Port ($C00000)
    void WriteDataPort(Word data);

    // Handles reads from the VdpData Port ($C00000)
    Word ReadDataPort();
};

} // namespace GenesisEmu::Core