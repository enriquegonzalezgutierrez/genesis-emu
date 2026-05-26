// ==============================================================================
// GenesisEmu - VDP (Video Display Processor) Domain Model Header
// ==============================================================================
// This class represents the VDP graphics chip. It manages private video memory
// spaces (VRAM, CRAM, VSRAM) and processes register and memory write commands.
// ==============================================================================

#pragma once

#include "IMemoryMappedDevice.h"
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
    Byte GetRegister(int index) const { return m_registers[index & 0x1F]; }
    Address GetTargetAddress() const { return m_targetAddress; }
    Byte ReadVramDirect(Address addr) const { return m_vram[addr & 0xFFFF]; }

private:
    // --------------------------------------------------------------------------
    // VDP Internal Memories (Encapsulated DDD Entities)
    // --------------------------------------------------------------------------
    std::array<Byte, 0x10000> m_vram;   // 64 KB Video RAM (Tiles, Tilemaps, Sprites)
    std::array<Byte, 128>     m_cram;   // 128 Bytes Color RAM (Palettes)
    std::array<Byte, 80>      m_vsram;  // 80 Bytes Vertical Scroll RAM
    std::array<Byte, 24>      m_registers; // 24 Internal Registers ($00 to $17)

    // --------------------------------------------------------------------------
    // Control Port State Machine (Flip-Flop)
    // --------------------------------------------------------------------------
    bool     m_controlWritePending;     // True if the first 16-bit word has been written
    Word     m_controlRegisterLatch;    // Temporary storage for the first word
    Address  m_targetAddress;           // Parsed 14-bit (or 16-bit) internal memory offset
    Byte     m_controlCode;             // Parsed 6-bit command code (Read/Write/DMA)

    // --------------------------------------------------------------------------
    // Private Command Processors
    // --------------------------------------------------------------------------
    // Handles writes to the VdpCtrl Port ($C00004)
    void WriteControlPort(Word data);

    // Handles writes to the VdpData Port ($C00000)
    void WriteDataPort(Word data);

    // Handles reads from the VdpData Port ($C00000)
    Word ReadDataPort();
};

} // namespace GenesisEmu::Core