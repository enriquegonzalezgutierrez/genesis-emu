// ==============================================================================
// GenesisEmu - Memory Mapped Device Interface (Core Domain)
// ==============================================================================
// This file defines the core data type aliases and the abstract interface that
// all virtual components connected to the system bus must implement.
//
// SOLID Compliance:
// 1. Dependency Inversion Principle (DIP):
//    The system memory bus depends on this abstraction, ensuring that the bus is
//    decoupled from concrete implementations of ROM, RAM, VDP, and I/O registers.
// 2. Interface Segregation Principle (ISP):
//    The interface is kept focused solely on direct byte and word memory-mapped
//    read/write operations.
// ==============================================================================

#pragma once

#include <cstdint>

namespace GenesisEmu::Core::Domain::Common {

// --- Global Domain Type Aliases ---
using Byte     = std::uint8_t;   // 8-bit unsigned integer
using Word     = std::uint16_t;  // 16-bit unsigned integer
using Longword = std::uint32_t;  // 32-bit unsigned integer
using Address  = std::uint32_t;  // 32-bit container for 24-bit physical addresses

/**
 * @class IMemoryMappedDevice
 * @brief Abstract interface representing any peripheral mapped to the system bus.
 */
class IMemoryMappedDevice {
public:
    virtual ~IMemoryMappedDevice() = default;

    // --- Read Operations ---

    /**
     * @brief Reads an 8-bit byte from the device at a calculated offset.
     * @param offset Address relative to the device's mapped base, or absolute address.
     * @return The 8-bit value read.
     */
    virtual Byte ReadByte(Address offset) = 0;

    /**
     * @brief Reads a 16-bit word from the device at a calculated offset.
     * @param offset Address relative to the device's mapped base, or absolute address.
     * @return The 16-bit value read.
     */
    virtual Word ReadWord(Address offset) = 0;

    // --- Write Operations ---

    /**
     * @brief Writes an 8-bit byte to the device at a calculated offset.
     * @param offset Address relative to the device's mapped base, or absolute address.
     * @param data The 8-bit value to write.
     */
    virtual void WriteByte(Address offset, Byte data) = 0;

    /**
     * @brief Writes a 16-bit word to the device at a calculated offset.
     * @param offset Address relative to the device's mapped base, or absolute address.
     * @param data The 16-bit value to write.
     */
    virtual void WriteWord(Address offset, Word data) = 0;
};

} // namespace GenesisEmu::Core::Domain::Common