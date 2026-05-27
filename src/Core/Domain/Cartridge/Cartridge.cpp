// ==============================================================================
// GenesisEmu - Cartridge Entity Implementation (Core Domain)
// ==============================================================================
// This file implements header parsing, additive ROM checksum calculations,
// and Big-Endian segment read routing.
//
// SOLID Compliance:
// 1. Single Responsibility Principle (SRP):
//    It handles strictly data loading, parsing, and banking translation.
// ==============================================================================

#include "Cartridge.h"
#include <algorithm>
#include <cctype>

namespace GenesisEmu::Core::Domain::Cartridge {

using namespace GenesisEmu::Core::Domain::Common;

/**
 * @brief Helper function to trim trailing spaces from fixed-width header strings.
 */
static std::string TrimTrailingSpaces(const std::string& str) {
    auto end = std::find_if_not(str.rbegin(), str.rend(), [](unsigned char ch) {
        return std::isspace(ch);
    });
    return std::string(str.begin(), end.base());
}

Cartridge::Cartridge() : m_checksum(0) {
    // Initialize default flat mapping: Slot 0 -> Bank 0 (Offset 0x000000), etc.
    for (int i = 0; i < 8; ++i) {
        m_banks[i] = i * 0x80000; // Each slot covers 512KB (0x80000 bytes)
    }
}

bool Cartridge::LoadROM(const std::vector<Byte>& romData) {
    if (romData.size() < 512) {
        return false;
    }

    m_rom = romData;
    ParseHeader();
    
    // --- Auto-Patch Checksum in Virtual Memory ---
    // Calculates a 16-bit additive checksum of all Words from offset $200
    // to the end of the ROM, patching the header in memory to bypass copy protection locks.
    std::uint32_t calculatedChecksum = 0;
    for (std::size_t i = 0x200; i < m_rom.size(); i += 2) {
        if (i + 1 < m_rom.size()) {
            Word word = (static_cast<Word>(m_rom[i]) << 8) | m_rom[i + 1];
            calculatedChecksum += word;
        }
    }
    
    Word finalChecksum = static_cast<Word>(calculatedChecksum & 0xFFFF);
    m_rom[0x18E] = static_cast<Byte>(finalChecksum >> 8);
    m_rom[0x18F] = static_cast<Byte>(finalChecksum & 0xFF);
    m_checksum = finalChecksum;

    return true;
}

void Cartridge::SetBank(Byte slot, Byte bankIndex) {
    // SSF2 Paging Protection: Only allow dynamic paging if the ROM exceeds 4MB.
    // If the game fits within a standard 4MB window, it remains flat and safe.
    if (m_rom.size() > 4194304) {
        if (slot < 8) {
            m_banks[slot] = static_cast<std::uint32_t>(bankIndex) * 0x80000; // 512 KB
        }
    }
}

// ------------------------------------------------------------------------------
// IMemoryMappedDevice Interface Overrides (Read Operations)
// ------------------------------------------------------------------------------
Byte Cartridge::ReadByte(Address offset) {
    // Extract virtual slot (Bits 21-19 map address ranges to slot index 0-7)
    Byte slot = (offset >> 19) & 0x07;
    
    std::uint32_t bankBase = m_banks[slot];
    Address relativeOffset = offset & 0x7FFFF; // Mask to 512KB range
    Address physicalAddress = bankBase + relativeOffset;

    if (physicalAddress < m_rom.size()) {
        return m_rom[physicalAddress];
    }
    return 0xFF; // Open bus fallback
}

Word Cartridge::ReadWord(Address offset) {
    Byte slot = (offset >> 19) & 0x07;
    
    std::uint32_t bankBase = m_banks[slot];
    Address relativeOffset = offset & 0x7FFFF;
    Address physicalAddress = bankBase + relativeOffset;

    if (physicalAddress + 1 < m_rom.size()) {
        Byte highByte = m_rom[physicalAddress];
        Byte lowByte  = m_rom[physicalAddress + 1];
        
        // Return structured word in Big-Endian format
        return (static_cast<Word>(highByte) << 8) | lowByte;
    }
    return 0xFFFF;
}

// ------------------------------------------------------------------------------
// IMemoryMappedDevice Interface Overrides (Write Operations - ROM is Read-Only)
// ------------------------------------------------------------------------------
void Cartridge::WriteByte([[maybe_unused]] Address offset, [[maybe_unused]] Byte data) {
    // ROM handles no physical byte writes.
}

void Cartridge::WriteWord([[maybe_unused]] Address offset, [[maybe_unused]] Word data) {
    // ROM handles no physical word writes.
}

// ------------------------------------------------------------------------------
// Header Parser Engine
// ------------------------------------------------------------------------------
void Cartridge::ParseHeader() {
    // Extract Game Title (Domestic Name field at offset $120 to $14F)
    std::string titleRaw(48, ' ');
    for (std::size_t i = 0; i < 48; ++i) {
        titleRaw[i] = static_cast<char>(m_rom[0x120 + i]);
    }
    m_gameTitle = TrimTrailingSpaces(titleRaw);

    // Extract Serial Code (Game Code field at offset $180 to $18D)
    std::string serialRaw(14, ' ');
    for (std::size_t i = 0; i < 14; ++i) {
        serialRaw[i] = static_cast<char>(m_rom[0x180 + i]);
    }
    m_serialCode = TrimTrailingSpaces(serialRaw);

    // Initial load of the header's expected checksum value
    m_checksum = (static_cast<Word>(m_rom[0x18E]) << 8) | m_rom[0x18F];
}

} // namespace GenesisEmu::Core::Domain::Cartridge