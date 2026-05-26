// ==============================================================================
// GenesisEmu - Cartridge Implementation (Updated with Auto-Checksum Patch)
// ==============================================================================
// This file contains the logic to load the raw ROM binary, parse metadata from
// the Sega Rom Header, and perform read operations in Big-Endian format.
// Upgraded with a real-time Memory Checksum Auto-Patching mechanism.
// ==============================================================================

#include "Cartridge.h"
#include <algorithm>

namespace GenesisEmu::Core {

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
    if (romData.size() < 512) {
        return false;
    }

    m_rom = romData;
    ParseHeader();
    
    // --- ADDED: Auto-Patch Checksum in Memory ---
    // Sega spec: 16-bit additive checksum of all Words from $200 to end of ROM.
    // Hack ROMs usually bypass this, so we overwrite the header checksum
    // at $18E with the calculated value so built-in game checks always pass!
    uint16_t calculatedChecksum = 0;
    for (size_t i = 0x200; i < m_rom.size(); i += 2) {
        if (i + 1 < m_rom.size()) {
            uint16_t word = (m_rom[i] << 8) | m_rom[i + 1];
            calculatedChecksum += word;
        }
    }
    
    // Patch expected checksum at $18E-$18F inside our mapped memory
    m_rom[0x18E] = static_cast<Byte>(calculatedChecksum >> 8);
    m_rom[0x18F] = static_cast<Byte>(calculatedChecksum & 0xFF);
    m_checksum = calculatedChecksum;

    return true;
}

void Cartridge::SetBank(Byte slot, Byte bankIndex) {
    if (slot < 8) {
        m_banks[slot] = static_cast<uint32_t>(bankIndex) * 0x80000; // 512 KB = 0x80000 bytes
    }
}

// ------------------------------------------------------------------------------
// IMemoryMappedDevice Interface Overrides (Read Operations)
// ------------------------------------------------------------------------------
Byte Cartridge::ReadByte(Address offset) {
    Byte slot = (offset >> 19) & 0x07;
    
    Address bankBase = m_banks[slot];
    Address relativeOffset = offset & 0x7FFFF; 
    Address physicalAddress = bankBase + relativeOffset;

    if (physicalAddress < m_rom.size()) {
        return m_rom[physicalAddress];
    }
    return 0xFF;
}

Word Cartridge::ReadWord(Address offset) {
    Byte slot = (offset >> 19) & 0x07;
    
    Address bankBase = m_banks[slot];
    Address relativeOffset = offset & 0x7FFFF;
    Address physicalAddress = bankBase + relativeOffset;

    if (physicalAddress + 1 < m_rom.size()) {
        Byte highByte = m_rom[physicalAddress];
        Byte lowByte  = m_rom[physicalAddress + 1];
        
        return (static_cast<Word>(highByte) << 8) | lowByte;
    }
    return 0xFFFF;
}

// ------------------------------------------------------------------------------
// IMemoryMappedDevice Interface Overrides (Write Operations)
// ------------------------------------------------------------------------------
void Cartridge::WriteByte([[maybe_unused]] Address offset, [[maybe_unused]] Byte data) {
    // ROM is physically Read-Only. Writes are handled via the SegaMapperDevice.
}

void Cartridge::WriteWord([[maybe_unused]] Address offset, [[maybe_unused]] Word data) {
    // ROM is physically Read-Only. Writes are handled via the SegaMapperDevice.
}

// ------------------------------------------------------------------------------
// Header Parser Engine
// ------------------------------------------------------------------------------
void Cartridge::ParseHeader() {
    std::string titleRaw(48, ' ');
    for (size_t i = 0; i < 48; ++i) {
        titleRaw[i] = static_cast<char>(m_rom[0x120 + i]);
    }
    m_gameTitle = TrimTrailingSpaces(titleRaw);

    std::string serialRaw(14, ' ');
    for (size_t i = 0; i < 14; ++i) {
        serialRaw[i] = static_cast<char>(m_rom[0x180 + i]);
    }
    m_serialCode = TrimTrailingSpaces(serialRaw);

    m_checksum = (static_cast<Word>(m_rom[0x18E]) << 8) | m_rom[0x18F];
}

} // namespace GenesisEmu::Core