// ==============================================================================
// GenesisEmu - Sega Mapper Device Header (Core Domain)
// ==============================================================================
// This file declares the SegaMapperDevice class, mapped to $A13000 - $A130FF.
// It acts as the physical writing port for cartridge bank switching.
//
// SOLID Compliance:
// 1. Single Responsibility Principle (SRP):
//    It is solely responsible for parsing write requests to mapper registers
//    and communicating those requests to the Cartridge entity.
// 2. Dependency Inversion Principle (DIP):
//    It interacts via abstract IMemoryMappedDevice boundaries.
// ==============================================================================

#pragma once

#include "../Common/IMemoryMappedDevice.h"
#include "Cartridge.h"

namespace GenesisEmu::Core::Domain::Cartridge {

/**
 * @class SegaMapperDevice
 * @brief Memory-mapped device handling dynamic banking control writes (SSF2 format).
 */
class SegaMapperDevice : public Common::IMemoryMappedDevice {
public:
    explicit SegaMapperDevice(Cartridge* cartridge) 
        : m_cartridge(cartridge) {}

    ~SegaMapperDevice() override = default;

    // --- IMemoryMappedDevice Interface Overrides (Read Operations) ---
    
    Common::Byte ReadByte([[maybe_unused]] Common::Address offset) override { 
        return 0xFF; 
    }
    
    Common::Word ReadWord([[maybe_unused]] Common::Address offset) override { 
        return 0xFFFF; 
    }

    // --- IMemoryMappedDevice Interface Overrides (Write Operations) ---

    void WriteByte(Common::Address offset, Common::Byte data) override {
        // SSF2 Registers live at odd byte addresses: $A130F3, $A130F5 ... $A130FF
        // Corresponding to relative device offsets 0xF3, 0xF5, 0xF7, 0xF9, 0xFB, 0xFD, 0xFF
        if (m_cartridge && offset >= 0xF3 && offset <= 0xFF && (offset & 1) != 0) {
            // Maps 0xF3 -> Slot 1, 0xF5 -> Slot 2, ... 0xFF -> Slot 7
            Common::Byte slot = (offset - 0xF1) >> 1;
            if (slot >= 1 && slot < 8) {
                m_cartridge->SetBank(slot, data);
            }
        }
    }

    void WriteWord(Common::Address offset, Common::Word data) override {
        // Word writes to odd register ranges typically route the lower byte
        // to the odd location (offset + 1)
        WriteByte(offset + 1, static_cast<Common::Byte>(data & 0xFF));
    }

private:
    Cartridge* m_cartridge; // Reference to our parent aggregate root Cartridge
};

} // namespace GenesisEmu::Core::Domain::Cartridge