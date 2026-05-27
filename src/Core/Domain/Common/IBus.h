// ==============================================================================
// GenesisEmu - System Bus Interface (Core Domain)
// ==============================================================================
// This file defines the IBus interface, which serves as the core communication
// gateway. The CPU communicates exclusively with this abstract boundary,
// decoupling execution mechanics from concrete address mapping configurations.
//
// SOLID Compliance:
// 1. Dependency Inversion Principle (DIP):
//    The execution core (M68k) depends entirely on this interface rather than
//    concrete hardware classes.
// 2. Interface Segregation Principle (ISP):
//    The interface is strictly designed for Bus operations (reading, writing,
//    and peripheral registration).
// ==============================================================================

#pragma once

#include "IMemoryMappedDevice.h"

namespace GenesisEmu::Core::Domain::Common {

/**
 * @class IBus
 * @brief Abstract interface representing the system interconnect bus.
 */
class IBus {
public:
    virtual ~IBus() = default;

    // --- Read Channels ---

    /**
     * @brief Fetches an 8-bit Byte from the physical address space.
     * @param address The 24-bit mapped address.
     * @return The read value.
     */
    virtual Byte ReadByte(Address address) = 0;

    /**
     * @brief Fetches a 16-bit Word from the physical address space.
     * @param address The 24-bit mapped address.
     * @return The read value.
     */
    virtual Word ReadWord(Address address) = 0;

    /**
     * @brief Fetches a 32-bit Longword from the physical address space.
     * @param address The 24-bit mapped address.
     * @return The read value.
     */
    virtual Longword ReadLongword(Address address) = 0;

    // --- Write Channels ---

    /**
     * @brief Writes an 8-bit Byte to the physical address space.
     * @param address The 24-bit mapped address.
     * @param data The 8-bit value to write.
     */
    virtual void WriteByte(Address address, Byte data) = 0;

    /**
     * @brief Writes a 16-bit Word to the physical address space.
     * @param address The 24-bit mapped address.
     * @param data The 16-bit value to write.
     */
    virtual void WriteWord(Address address, Word data) = 0;

    /**
     * @brief Writes a 32-bit Longword to the physical address space.
     * @param address The 24-bit mapped address.
     * @param data The 32-bit value to write.
     */
    virtual void WriteLongword(Address address, Longword data) = 0;

    // --- Extension and Peripheral Registration ---

    /**
     * @brief Maps an IMemoryMappedDevice to a physical address range.
     * @param device Pointer to the peripheral.
     * @param startAddress Starting boundary of the range (inclusive).
     * @param endAddress Ending boundary of the range (inclusive).
     */
    virtual void AttachDevice(IMemoryMappedDevice* device, Address startAddress, Address endAddress) = 0;
};

} // namespace GenesisEmu::Core::Domain::Common