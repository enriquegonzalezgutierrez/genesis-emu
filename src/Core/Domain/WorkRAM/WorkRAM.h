// ==============================================================================
// GenesisEmu - Work RAM Entity (Core Domain)
// ==============================================================================
// This file declares the WorkRAM class representing the 64 KB main system memory.
// It maps to $E00000 - $FFFFFF.
//
// SOLID Compliance:
// 1. Single Responsibility Principle (SRP):
//    It is solely responsible for encapsulating, storing, and mirroring the
//    64 KB working RAM array. It has no dependencies on other console elements.
// ==============================================================================

#pragma once

#include "../Common/IMemoryMappedDevice.h"
#include <array>

namespace GenesisEmu::Core::Domain::WorkRAM {

/**
 * @class WorkRAM
 * @brief Memory-mapped device emulating the 64KB system Work RAM.
 */
class WorkRAM : public Common::IMemoryMappedDevice {
public:
    WorkRAM() {
        m_ram.fill(0x00); // Initialize memory space to clean zeroes
    }
    
    ~WorkRAM() override = default;

    // --- IMemoryMappedDevice Interface Overrides (Read Operations) ---

    Common::Byte ReadByte(Common::Address offset) override {
        // Mirrored Access: masks the address offset to fit within the 64KB array
        return m_ram[offset & 0xFFFF];
    }

    Common::Word ReadWord(Common::Address offset) override {
        Common::Address addr = offset & 0xFFFF;
        // Big-Endian read: reconstructs a 16-bit word from sequential bytes
        return (static_cast<Common::Word>(m_ram[addr]) << 8) | m_ram[(addr + 1) & 0xFFFF];
    }

    // --- IMemoryMappedDevice Interface Overrides (Write Operations) ---

    void WriteByte(Common::Address offset, Common::Byte data) override {
        m_ram[offset & 0xFFFF] = data;
    }

    void WriteWord(Common::Address offset, Common::Word data) override {
        Common::Address addr = offset & 0xFFFF;
        // Big-Endian write: splits the 16-bit word across sequential memory cells
        m_ram[addr]                = static_cast<Common::Byte>(data >> 8);
        m_ram[(addr + 1) & 0xFFFF] = static_cast<Common::Byte>(data & 0xFF);
    }

private:
    std::array<Common::Byte, 0x10000> m_ram; // 64 KB physical buffer limits
};

} // namespace GenesisEmu::Core::Domain::WorkRAM