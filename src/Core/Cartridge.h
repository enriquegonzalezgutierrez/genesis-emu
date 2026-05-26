// ==============================================================================
// GenesisEmu - Cartridge Domain Model Header (Updated with Sega Mapper)
// ==============================================================================
// Upgraded the Cartridge class with 8 logical 512KB bank slots (SSF2 Sega Mapper).
// Added the decoupled SegaMapperDevice to map to address range $A13000-$A130FF.
// ==============================================================================

#pragma once

#include "IMemoryMappedDevice.h"
#include <vector>
#include <string>
#include <array>

namespace GenesisEmu::Core {

class Cartridge : public IMemoryMappedDevice {
public:
    // Constructor initializes default flat mapping: Slot 0 -> Bank 0, etc.
    Cartridge() {
        for (int i = 0; i < 8; ++i) {
            m_banks[i] = i * 0x80000; // Each slot covers 512KB (0x80000 bytes)
        }
    }
    ~Cartridge() override = default;

    // --- Public Control Interface ---
    bool LoadROM(const std::vector<Byte>& romData);

    // --- Bank Selection API (SSF2 Mapper Interface) ---
    /**
     * @brief Paginates a physical ROM segment into one of the 8 logical slots.
     * @param slot logical slot index (0 to 7).
     * @param bankIndex physical 512KB bank index (0 to 63).
     */
    void SetBank(Byte slot, Byte bankIndex);

    // --- IMemoryMappedDevice Interface Overrides ---
    Byte ReadByte(Address offset) override;
    Word ReadWord(Address offset) override;
    
    void WriteByte(Address offset, Byte data) override;
    void WriteWord(Address offset, Word data) override;

    // --- Metadata Inspection (Getters) ---
    const std::string& GetGameTitle() const { return m_gameTitle; }
    const std::string& GetSerialCode() const { return m_serialCode; }
    Word GetChecksum() const { return m_checksum; }
    size_t GetROMSize() const { return m_rom.size(); }

private:
    std::vector<Byte> m_rom;

    // Metadata parsed from Sega Header ($100 to $1FF)
    std::string m_gameTitle;
    std::string m_serialCode;
    Word        m_checksum;

    // 8 logical slots of 512KB pointing to physical ROM addresses
    std::array<uint32_t, 8> m_banks;

    void ParseHeader();
};

// ------------------------------------------------------------------------------
// Decoupled Sega Mapper Register Device
// ------------------------------------------------------------------------------
// Maps to the Cartridge I/O range $A13000 to $A130FF to handle bank-switching.
// ------------------------------------------------------------------------------
class SegaMapperDevice : public IMemoryMappedDevice {
public:
    explicit SegaMapperDevice(Cartridge* cartridge) : m_cartridge(cartridge) {}
    ~SegaMapperDevice() override = default;

    Byte ReadByte([[maybe_unused]] Address offset) override { return 0xFF; }
    Word ReadWord([[maybe_unused]] Address offset) override { return 0xFFFF; }

    void WriteByte(Address offset, Byte data) override {
        // Registers live at odd addresses: $A130F3, $A130F5 ... $A130FF
        // Corresponding to offsets 0xF3, 0xF5, 0xF7, 0xF9, 0xFB, 0xFD, 0xFF
        if (offset >= 0xF3 && offset <= 0xFF && (offset & 1) != 0) {
            Byte slot = (offset - 0xF1) >> 1; // Maps 0xF3 -> Slot 1, 0xF5 -> Slot 2, etc.
            if (slot >= 1 && slot < 8) {
                m_cartridge->SetBank(slot, data);
            }
        }
    }

    void WriteWord(Address offset, Word data) override {
        // Word writes to odd registers copy the lower byte to the odd location
        WriteByte(offset + 1, static_cast<Byte>(data & 0xFF));
    }

private:
    Cartridge* m_cartridge;
};

} // namespace GenesisEmu::Core