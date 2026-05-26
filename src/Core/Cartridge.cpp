// ==============================================================================
// GenesisEmu - Cartridge Implementation
// ==============================================================================
// This file contains the logic to load the raw ROM binary, parse metadata from
// the Sega Rom Header, and perform read operations in Big-Endian format.
// ==============================================================================

#include "Cartridge.h"
#include <algorithm>

namespace GenesisEmu::Core {

// Helper function to remove trailing spaces from Sega header fixed-size strings
static std::string TrimTrailingSpaces(const std::string& str) {
    auto end = std::find_if_not(str.rbegin(), str.rend(), [](unsigned char ch) {
        return std::isspace(ch);
    });
    return std::string(str.begin(), end.base());
}

// ------------------------------------------------------------------------------
// Public Control Interface
// ------------------------------------------------------------------------------
bool Cartridge::LoadROM(const std::vector<Byte>& romData) {
    // Minimum valid ROM size to contain a Sega Header ($200 bytes)
    if (romData.size() < 512) {
        return false;
    }

    m_rom = romData;
    ParseHeader();
    return true;
}

// ------------------------------------------------------------------------------
// IMemoryMappedDevice Interface Overrides (Read Operations)
// ------------------------------------------------------------------------------
Byte Cartridge::ReadByte(Address offset) {
    if (offset < m_rom.size()) {
        return m_rom[offset];
    }
    // Return standard open-bus default if reading out of bounds
    return 0xFF;
}

Word Cartridge::ReadWord(Address offset) {
    if (offset + 1 < m_rom.size()) {
        Byte highByte = m_rom[offset];
        Byte lowByte  = m_rom[offset + 1];
        
        // Combine into Big-Endian Word
        return (static_cast<Word>(highByte) << 8) | lowByte;
    }
    return 0xFFFF;
}

// ------------------------------------------------------------------------------
// IMemoryMappedDevice Interface Overrides (Write Operations - ROM is Read-Only)
// ------------------------------------------------------------------------------
void Cartridge::WriteByte([[maybe_unused]] Address offset, [[maybe_unused]] Byte data) {
    // Writing to ROM is physically impossible and safely ignored
}

void Cartridge::WriteWord([[maybe_unused]] Address offset, [[maybe_unused]] Word data) {
    // Writing to ROM is physically impossible and safely ignored
}

// ------------------------------------------------------------------------------
// Header Parser Engine
// ------------------------------------------------------------------------------
void Cartridge::ParseHeader() {
    // 1. Parse Domestic Game Title ($120 to $14F - 48 bytes)
    std::string titleRaw(48, ' ');
    for (size_t i = 0; i < 48; ++i) {
        titleRaw[i] = static_cast<char>(m_rom[0x120 + i]);
    }
    m_gameTitle = TrimTrailingSpaces(titleRaw);

    // 2. Parse Serial Code ($180 to $18D - 14 bytes)
    std::string serialRaw(14, ' ');
    for (size_t i = 0; i < 14; ++i) {
        serialRaw[i] = static_cast<char>(m_rom[0x180 + i]);
    }
    m_serialCode = TrimTrailingSpaces(serialRaw);

    // 3. Parse Checksum ($18E to $18F - 2 bytes / Word)
    m_checksum = (static_cast<Word>(m_rom[0x18E]) << 8) | m_rom[0x18F];
}

} // namespace GenesisEmu::Core