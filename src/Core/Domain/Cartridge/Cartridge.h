// ==============================================================================
// GenesisEmu - Cartridge Entity Header (Core Domain)
// ==============================================================================
// This file declares the Cartridge class, representing a Sega Genesis game.
// It manages ROM data, dynamic SSF2 bank switching, and SRAM (Save Data) routing.
//
// SOLID Compliance:
// 1. Single Responsibility Principle (SRP):
//    Solely responsible for storing raw ROM data, parsing cartridge header
//    records (including SRAM bounds), and translating read/writes internally.
// ==============================================================================

#pragma once

#include "../Common/IMemoryMappedDevice.h"
#include <vector>
#include <string>
#include <array>

namespace GenesisEmu::Core::Domain::Cartridge {

class Cartridge : public Common::IMemoryMappedDevice {
public:
    Cartridge();
    ~Cartridge() override = default;

    // --- Loading and Initialization ---
    bool LoadROM(const std::vector<Common::Byte>& romData);

    // --- Dynamic SSF2 Paging Interface ---
    void SetBank(Common::Byte slot, Common::Byte bankIndex);

    // --- IMemoryMappedDevice Interface Overrides ---
    Common::Byte ReadByte(Common::Address offset) override;
    Common::Word ReadWord(Common::Address offset) override;
    
    // Writes are ignored on the ROM itself but accepted if targeting SRAM
    void WriteByte(Common::Address offset, Common::Byte data) override;
    void WriteWord(Common::Address offset, Common::Word data) override;

    // --- Domain Inspection (Getters) ---
    const std::string& GetGameTitle() const { return m_gameTitle; }
    const std::string& GetSerialCode() const { return m_serialCode; }
    Common::Word GetChecksum() const { return m_checksum; }
    std::size_t GetROMSize() const { return m_rom.size(); }
    
    // SRAM specific getters (Useful for Outer Hexagon adapters to save to disk)
    bool HasSRAM() const { return m_hasSram; }
    const std::vector<Common::Byte>& GetSRAMData() const { return m_sram; }

private:
    std::vector<Common::Byte> m_rom;

    // Sega Header Metadata
    std::string  m_gameTitle;
    std::string  m_serialCode;
    Common::Word m_checksum;

    // SSF2 Paging: 8 virtual slots of 512KB
    std::array<std::uint32_t, 8> m_banks;

    // --- SRAM (Save Data) State ---
    std::vector<Common::Byte> m_sram;
    bool            m_hasSram;
    Common::Address m_sramStart;
    Common::Address m_sramEnd;

    void ParseHeader();
    void ParseSramMetadata();
};

} // namespace GenesisEmu::Core::Domain::Cartridge