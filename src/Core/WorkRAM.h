// ==============================================================================
// GenesisEmu - Work RAM (WRAM) Bounded Context (Core Domain)
// ==============================================================================
// This file emulates the 64 KB system Work RAM mapped at $E00000 to $FFFFFF.
// Handles automatic address mirroring across the 24-bit physical address space.
//
// DESIGN STRATEGY:
// 1. SRP: Pure memory array wrapper dedicated to system state variables.
// 2. Hardware Mirroring: Masking with 0xFFFF maps any address in the 
//    $E00000 - $FFFFFF range to the 64 KB physical buffer limits.
// ==============================================================================

#pragma once

#include "IMemoryMappedDevice.h"
#include <array>

namespace GenesisEmu::Core {

class WorkRAM : public IMemoryMappedDevice {
public:
    WorkRAM() {
        m_ram.fill(0x00); // Initialize system memory to zero
    }
    ~WorkRAM() override = default;

    // --- IMemoryMappedDevice Interface Overrides ---
    
    Byte ReadByte(Address offset) override {
        // Address mirroring: mask to 64 KB boundaries
        return m_ram[offset & 0xFFFF];
    }

    Word ReadWord(Address offset) override {
        Address addr = offset & 0xFFFF;
        // Big-Endian read: combine consecutive bytes
        return (static_cast<Word>(m_ram[addr]) << 8) | m_ram[(addr + 1) & 0xFFFF];
    }

    void WriteByte(Address offset, Byte data) override {
        m_ram[offset & 0xFFFF] = data;
    }

    void WriteWord(Address offset, Word data) override {
        Address addr = offset & 0xFFFF;
        // Big-Endian write: split 16-bit word into bytes
        m_ram[addr]            = static_cast<Byte>(data >> 8);
        m_ram[(addr + 1) & 0xFFFF] = static_cast<Byte>(data & 0xFF);
    }

private:
    std::array<Byte, 0x10000> m_ram; // 64 KB physical RAM array
};

} // namespace GenesisEmu::Core