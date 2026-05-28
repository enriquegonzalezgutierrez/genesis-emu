// ==============================================================================
// GenesisEmu - Main Bus Concrete Router Header (Core Domain)
// ==============================================================================
// This file declares the MainBus class. It manages dynamic range-based enforcements
// to route CPU requests to registered peripheral devices and handles Z80 handshakes.
//
// SOLID Compliance:
// 1. Single Responsibility Principle (SRP):
//    The MainBus acts solely as a central 24-bit address-translation mediator.
// 2. Open/Closed Principle (OCP):
//    New virtual hardware devices can be integrated via the AttachDevice interface
//    without modifying the underlying routing iteration.
// ==============================================================================

#pragma once

#include "../Common/IBus.h"
#include <vector>
#include <array>

namespace GenesisEmu::Core::Domain::Bus {

class MainBus : public Common::IBus {
public:
    MainBus() = default;
    ~MainBus() override = default;

    // --- IBus Read Interface Overrides ---
    Common::Byte ReadByte(Common::Address address) override;
    Common::Word ReadWord(Common::Address address) override;
    Common::Longword ReadLongword(Common::Address address) override;

    // --- IBus Write Interface Overrides ---
    void WriteByte(Common::Address address, Common::Byte data) override;
    void WriteWord(Common::Address address, Common::Word data) override;
    void WriteLongword(Common::Address address, Common::Longword data) override;

    // --- Device Management ---
    void AttachDevice(Common::IMemoryMappedDevice* device, Common::Address startAddress, Common::Address endAddress) override;

private:
    /**
     * @struct DeviceMapping
     * @brief Structure binding a peripheral pointer to its mapped physical boundaries.
     */
    struct DeviceMapping {
        Common::IMemoryMappedDevice* device;
        Common::Address              startAddress;
        Common::Address              endAddress;
    };

    // Database containing dynamically mapped devices
    std::vector<DeviceMapping> m_devices;

    // --- Z80 Coprocessor Control States ---
    // True if the M68k has requested the Z80 bus.
    bool m_z80BusRequested = false;

    // True if the Z80 reset line is asserted (held in reset).
    // Note: Sega hardware holds the Z80 in reset by default on startup.
    bool m_z80ResetHeld = true; 

    // 8 KB Physical Z80 RAM Buffer ($A00000 - $A01FFF).
    std::array<Common::Byte, 0x2000> m_z80Ram{};

    /**
     * @brief Scans active registries to locate the device owning a given address.
     * @param address The requested physical 24-bit address.
     * @param outRelativeOffset Output reference containing the offset adjusted to the target device.
     * @return Pointer to the matched target device, or nullptr if address is unmapped.
     */
    Common::IMemoryMappedDevice* FindDevice(Common::Address address, Common::Address& outRelativeOffset);
};

} // namespace GenesisEmu::Core::Domain::Bus