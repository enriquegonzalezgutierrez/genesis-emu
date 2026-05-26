// ==============================================================================
// GenesisEmu - MainBus Concrete Implementation Header
// ==============================================================================
// This file defines the concrete MainBus class, which implements the IBus
// interface. It manages device routing dynamically using range-based mappings.
// ==============================================================================

#pragma once

#include "IBus.h"
#include <vector>

namespace GenesisEmu::Core {

class MainBus : public IBus {
public:
    MainBus() = default;
    ~MainBus() override = default;

    // --- IBus Read Interface Overrides ---
    Byte ReadByte(Address address) override;
    Word ReadWord(Address address) override;
    Longword ReadLongword(Address address) override;

    // --- IBus Write Interface Overrides ---
    void WriteByte(Address address, Byte data) override;
    void WriteWord(Address address, Word data) override;
    void WriteLongword(Address address, Longword data) override;

    // --- Device Management ---
    void AttachDevice(IMemoryMappedDevice* device, Address startAddress, Address endAddress) override;

private:
    // Helper structure to hold the memory range for an attached device
    struct DeviceMapping {
        IMemoryMappedDevice* device;
        Address startAddress;
        Address endAddress;
    };

    // Collection of all registered devices and their mapping boundaries
    std::vector<DeviceMapping> m_devices;

    // Helper method to locate the device mapped to a specific address.
    // Calculates the relative offset inside the device and returns the device pointer.
    // Returns nullptr if the address is unmapped.
    IMemoryMappedDevice* FindDevice(Address address, Address& outRelativeOffset);
};

} // namespace GenesisEmu::Core