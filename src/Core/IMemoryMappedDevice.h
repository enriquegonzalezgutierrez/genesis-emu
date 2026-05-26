// ==============================================================================
// GenesisEmu - IMemoryMappedDevice Interface
// ==============================================================================
// This file defines the core data types (matching our Ubiquitous Language)
// and the abstract interface that all physical devices mapped to the Bus must
// implement (Dependency Inversion Principle - SOLID).
// ==============================================================================

#pragma once

#include <cstdint>

namespace GenesisEmu::Core {

// ------------------------------------------------------------------------------
// Ubiquitous Language Type Aliases (DDD)
// ------------------------------------------------------------------------------
// These map 68000 hardware-specific sizes directly to modern C++ standard types.
using Byte     = std::uint8_t;   // 8-bit unsigned integer
using Word     = std::uint16_t;  // 16-bit unsigned integer
using Longword = std::uint32_t;  // 32-bit unsigned integer
using Address  = std::uint32_t;  // 24-bit/32-bit address space container

// ------------------------------------------------------------------------------
// IMemoryMappedDevice Interface
// ------------------------------------------------------------------------------
// Any component that responds to memory reads or writes on the MainBus must
// inherit from this interface.
// ------------------------------------------------------------------------------
class IMemoryMappedDevice {
public:
    // Virtual destructor is mandatory for proper cleanup of inherited classes
    virtual ~IMemoryMappedDevice() = default;

    // --- Read Operations ---
    // Reads an 8-bit Byte from the device at the given address offset
    virtual Byte ReadByte(Address offset) = 0;

    // Reads a 16-bit Word from the device at the given address offset
    virtual Word ReadWord(Address offset) = 0;

    // --- Write Operations ---
    // Writes an 8-bit Byte to the device at the given address offset
    virtual void WriteByte(Address offset, Byte data) = 0;

    // Writes a 16-bit Word to the device at the given address offset
    virtual void WriteWord(Address offset, Word data) = 0;
};

} // namespace GenesisEmu::Core