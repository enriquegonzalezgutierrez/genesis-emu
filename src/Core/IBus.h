// ==============================================================================
// GenesisEmu - IBus Interface
// ==============================================================================
// This file defines the IBus interface, which acts as the core communication
// channel. The CPU interacts exclusively with this interface to read and write
// system memory, completely decoupled from specific hardware devices.
// ==============================================================================

#pragma once

#include "IMemoryMappedDevice.h"

namespace GenesisEmu::Core {

class IBus {
public:
    virtual ~IBus() = default;

    // --------------------------------------------------------------------------
    // Read Operations
    // --------------------------------------------------------------------------
    // Reads an 8-bit Byte from the bus at the given 24-bit physical address
    virtual Byte ReadByte(Address address) = 0;

    // Reads a 16-bit Word from the bus at the given 24-bit physical address
    virtual Word ReadWord(Address address) = 0;

    // Reads a 32-bit Longword from the bus at the given 24-bit physical address
    virtual Longword ReadLongword(Address address) = 0;

    // --------------------------------------------------------------------------
    // Write Operations
    // --------------------------------------------------------------------------
    // Writes an 8-bit Byte to the bus at the given 24-bit physical address
    virtual void WriteByte(Address address, Byte data) = 0;

    // Writes a 16-bit Word to the bus at the given 24-bit physical address
    virtual void WriteWord(Address address, Word data) = 0;

    // Writes a 32-bit Longword to the bus at the given 24-bit physical address
    virtual void WriteLongword(Address address, Longword data) = 0;

    // --------------------------------------------------------------------------
    // Device Registration / Management
    // --------------------------------------------------------------------------
    // Attaches a memory-mapped device to a specific address range in the bus.
    // This implements the Open/Closed Principle (OCP), allowing system expansion
    // without altering the concrete bus implementation class.
    virtual void AttachDevice(IMemoryMappedDevice* device, Address startAddress, Address endAddress) = 0;
};

} // namespace GenesisEmu::Core