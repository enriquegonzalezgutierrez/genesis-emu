// ==============================================================================
// GenesisEmu - Cartridge Entity Implementation (Core Domain)
// ==============================================================================
// This file implements header parsing, ROM checksum auto-patching, Big-Endian 
// segment routing, and dynamic SRAM (Battery-backed save memory) mapping.
// ==============================================================================

#include "Cartridge.h"
#include <algorithm>
#include <cctype>
#include <iostream>

namespace GenesisEmu::Core::Domain::Cartridge {

using namespace GenesisEmu::Core::Domain::Common;

static std::string TrimTrailingSpaces(const std::string& str) {
    auto end = std::find_if_not(str.rbegin(), str.rend(), [](unsigned char ch) {
        return std::isspace(ch);
    });
    return std::string(str.begin(), end.base());
}

Cartridge::Cartridge() 
    : m_checksum(0)
    , m_hasSram(false)
    , m_sramStart(0)
    , m_sramEnd(0) 
{
    // Initialize default flat mapping: Slot 0 -> Bank 0 (Offset 0x000000), etc.
    for (int i = 0; i < 8; ++i) {
        m_banks[i] = i * 0x80000; 
    }
}

bool Cartridge::LoadROM(const std::vector<Byte>& romData) {
    if (romData.size() < 512) return false;

    m_rom = romData;
    ParseHeader();
    ParseSramMetadata();
    
    // --- Auto-Patch Checksum in Virtual Memory ---
    // Calculates a 16-bit additive checksum to bypass copy protection locks.
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
    if (m_rom.size() > 4194304) {
        if (slot < 8) {
            m_banks[slot] = static_cast<std::uint32_t>(bankIndex) * 0x80000;
        }
    }
}

// ------------------------------------------------------------------------------
// SRAM & Metadata Parsing
// ------------------------------------------------------------------------------
void Cartridge::ParseSramMetadata() {
    // Check for "RA" (RAM) magic bytes at $0x1B0
    if (m_rom[0x1B0] == 'R' && m_rom[0x1B1] == 'A') {
        
        m_sramStart = (m_rom[0x1B4] << 24) | (m_rom[0x1B5] << 16) | (m_rom[0x1B6] << 8) | m_rom[0x1B7];
        m_sramEnd   = (m_rom[0x1B8] << 24) | (m_rom[0x1B9] << 16) | (m_rom[0x1BA] << 8) | m_rom[0x1BB];
        
        // Safety bounds check. Standard SRAM starts at $200000.
        if (m_sramStart >= 0x200000 && m_sramEnd >= m_sramStart) {
            m_hasSram = true;
            std::size_t sramSize = m_sramEnd - m_sramStart + 1;
            m_sram.resize(sramSize, 0xFF); // Initialize with 0xFF as per real hardware behavior
            
            std::cout << "[Cartridge] SRAM Detected. Size: " << (sramSize / 1024) 
                      << " KB. Range: 0x" << std::hex << m_sramStart << " - 0x" << m_sramEnd << std::dec << std::endl;
        }
    }
}

void Cartridge::ParseHeader() {
    std::string titleRaw(48, ' ');
    for (std::size_t i = 0; i < 48; ++i) titleRaw[i] = static_cast<char>(m_rom[0x120 + i]);
    m_gameTitle = TrimTrailingSpaces(titleRaw);

    std::string serialRaw(14, ' ');
    for (std::size_t i = 0; i < 14; ++i) serialRaw[i] = static_cast<char>(m_rom[0x180 + i]);
    m_serialCode = TrimTrailingSpaces(serialRaw);

    m_checksum = (static_cast<Word>(m_rom[0x18E]) << 8) | m_rom[0x18F];
}

// ------------------------------------------------------------------------------
// IMemoryMappedDevice Interface Overrides (Read Operations)
// ------------------------------------------------------------------------------
Byte Cartridge::ReadByte(Address offset) {
    if (m_hasSram && offset >= m_sramStart && offset <= m_sramEnd) {
        return m_sram[offset - m_sramStart];
    }

    Byte slot = (offset >> 19) & 0x07;
    std::uint32_t bankBase = m_banks[slot];
    Address physicalAddress = bankBase + (offset & 0x7FFFF);

    if (physicalAddress < m_rom.size()) {
        return m_rom[physicalAddress];
    }
    return 0xFF; // Open bus fallback
}

Word Cartridge::ReadWord(Address offset) {
    if (m_hasSram && offset >= m_sramStart && (offset + 1) <= m_sramEnd) {
        Byte highByte = m_sram[offset - m_sramStart];
        Byte lowByte  = m_sram[(offset + 1) - m_sramStart];
        return (static_cast<Word>(highByte) << 8) | lowByte;
    }

    Byte slot = (offset >> 19) & 0x07;
    std::uint32_t bankBase = m_banks[slot];
    Address physicalAddress = bankBase + (offset & 0x7FFFF);

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
void Cartridge::WriteByte(Address offset, Byte data) {
    // ROM is Read-Only, but writes to SRAM are permitted
    if (m_hasSram && offset >= m_sramStart && offset <= m_sramEnd) {
        m_sram[offset - m_sramStart] = data;
    }
}

void Cartridge::WriteWord(Address offset, Word data) {
    if (m_hasSram && offset >= m_sramStart && (offset + 1) <= m_sramEnd) {
        m_sram[offset - m_sramStart]       = static_cast<Byte>(data >> 8);
        m_sram[(offset + 1) - m_sramStart] = static_cast<Byte>(data & 0xFF);
    }
}

} // namespace GenesisEmu::Core::Domain::Cartridge