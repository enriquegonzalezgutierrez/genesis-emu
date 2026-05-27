// ==============================================================================
// GenesisEmu - Cartridge Entity Header (Core Domain)
// ==============================================================================
// This file declares the Cartridge class, representing a Sega Genesis game
// cartridge. It manages memory-mapped accesses, metadata extraction, and
// dynamic bank switching.
//
// SOLID Compliance:
// 1. Single Responsibility Principle (SRP):
//    It is solely responsible for storing raw ROM data, parsing cartridge header
//    records, and translating bank addresses. It does not handle memory writes to
//    mapper registers (which is delegated to a separate mapper class).
// ==============================================================================

#pragma once

#include "../Common/IMemoryMappedDevice.h"
#include <vector>
#include <string>
#include <array>

namespace GenesisEmu::Core::Domain::Cartridge {

/**
 * @class Cartridge
 * @brief Domain Entity representing the physical game cartridge and its banking registers.
 */
class Cartridge : public Common::IMemoryMappedDevice {
public:
    Cartridge();
    ~Cartridge() override = default;

    // --- Loading and Initialization ---

    /**
     * @brief Loads raw ROM binary data and triggers internal header parsing.
     * @param romData The binary ROM buffer.
     * @return True if successful, false otherwise.
     */
    bool LoadROM(const std::vector<Common::Byte>& romData);

    // --- Dynamic SSF2 Paging Interface ---

    /**
     * @brief Maps a physical 512KB ROM chunk to one of the 8 virtual bank slots.
     * @param slot The destination slot (0-7).
     * @param bankIndex The physical bank number to map.
     */
    void SetBank(Common::Byte slot, Common::Byte bankIndex);

    // --- IMemoryMappedDevice Interface Overrides ---
    Common::Byte ReadByte(Common::Address offset) override;
    Common::Word ReadWord(Common::Address offset) override;
    
    // ROM is physically read-only; writes are ignored on the ROM itself
    void WriteByte(Common::Address offset, Common::Byte data) override;
    void WriteWord(Common::Address offset, Common::Word data) override;

    // --- Domain Inspection (Getters) ---
    const std::string& GetGameTitle() const { return m_gameTitle; }
    const std::string& GetSerialCode() const { return m_serialCode; }
    Common::Word GetChecksum() const { return m_checksum; }
    std::size_t GetROMSize() const { return m_rom.size(); }

private:
    std::vector<Common::Byte> m_rom;

    // Metadata parsed from Sega ROM Header ($000100 - $0001FF)
    std::string  m_gameTitle;
    std::string  m_serialCode;
    Common::Word m_checksum;

    // SSF2 Paging: 8 virtual slots of 512KB pointing to physical ROM offsets
    std::array<std::uint32_t, 8> m_banks;

    /**
     * @brief Internal helper to parse metadata fields from the ROM header.
     */
    void ParseHeader();
};

} // namespace GenesisEmu::Core::Domain::Cartridge