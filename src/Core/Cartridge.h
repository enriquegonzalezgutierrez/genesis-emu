// ==============================================================================
// GenesisEmu - Cartridge Domain Model Header
// ==============================================================================
// This class represents a physical Sega Genesis cartridge. It stores the ROM 
// buffer, parses the ROM Header, and implements the IMemoryMappedDevice interface
// to respond to CPU read/write operations.
// ==============================================================================

#pragma once

#include "IMemoryMappedDevice.h"
#include <vector>
#include <string>

namespace GenesisEmu::Core {

class Cartridge : public IMemoryMappedDevice {
public:
    Cartridge() = default;
    ~Cartridge() override = default;

    // --------------------------------------------------------------------------
    // Public Control Interface
    // --------------------------------------------------------------------------
    // Loads raw ROM binary data into the cartridge's memory buffer.
    // Automatically triggers the ROM Header parsing. Returns true on success.
    bool LoadROM(const std::vector<Byte>& romData);

    // --- IMemoryMappedDevice Interface Overrides ---
    // Responds to M68k read requests within the cartridge memory space ($000000 - $3FFFFF)
    Byte ReadByte(Address offset) override;
    Word ReadWord(Address offset) override;
    
    // ROM is physically Read-Only. Writes are ignored or routed to SRAM if active.
    void WriteByte(Address offset, Byte data) override;
    void WriteWord(Address offset, Word data) override;

    // --------------------------------------------------------------------------
    // Metadata Inspection (Getters)
    // --------------------------------------------------------------------------
    const std::string& GetGameTitle() const { return m_gameTitle; }
    const std::string& GetSerialCode() const { return m_serialCode; }
    Word GetChecksum() const { return m_checksum; }
    size_t GetROMSize() const { return m_rom.size(); }

private:
    // Core game data storage
    std::vector<Byte> m_rom;

    // Parsed Metadata from ROM Header ($100 to $1FF)
    std::string m_gameTitle;
    std::string m_serialCode;
    Word        m_checksum;

    // Helper method to parse metadata from the loaded ROM data
    void ParseHeader();
};

} // namespace GenesisEmu::Core