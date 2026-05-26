// ==============================================================================
// GenesisEmu - MainBus Implementation
// ==============================================================================
// This file contains the routing and address calculation logic for the MainBus.
// It resolves memory accesses and reconstructs 32-bit values from 16-bit words.
// ==============================================================================

#include "MainBus.h"

namespace GenesisEmu::Core {

// ------------------------------------------------------------------------------
// Device Routing Engine
// ------------------------------------------------------------------------------
IMemoryMappedDevice* MainBus::FindDevice(Address address, Address& outRelativeOffset) {
    for (const auto& mapping : m_devices) {
        if (address >= mapping.startAddress && address <= mapping.endAddress) {
            // Calculate the internal offset relative to the device's start boundary
            outRelativeOffset = address - mapping.startAddress;
            return mapping.device;
        }
    }
    // Address does not correspond to any attached hardware
    return nullptr;
}

void MainBus::AttachDevice(IMemoryMappedDevice* device, Address startAddress, Address endAddress) {
    m_devices.push_back({device, startAddress, endAddress});
}

// ------------------------------------------------------------------------------
// 8-Bit (Byte) Memory Operations
// ------------------------------------------------------------------------------
Byte MainBus::ReadByte(Address address) {
    Address offset = 0;
    if (IMemoryMappedDevice* target = FindDevice(address, offset)) {
        return target->ReadByte(offset);
    }
    // Return standard open-bus default (0xFF) if reading unmapped space
    return 0xFF;
}

void MainBus::WriteByte(Address address, Byte data) {
    Address offset = 0;
    if (IMemoryMappedDevice* target = FindDevice(address, offset)) {
        target->WriteByte(offset, data);
    }
    // Unmapped writes are safely ignored
}

// ------------------------------------------------------------------------------
// 16-Bit (Word) Memory Operations
// ------------------------------------------------------------------------------
Word MainBus::ReadWord(Address address) {
    Address offset = 0;
    if (IMemoryMappedDevice* target = FindDevice(address, offset)) {
        return target->ReadWord(offset);
    }
    return 0xFFFF;
}

void MainBus::WriteWord(Address address, Word data) {
    Address offset = 0;
    if (IMemoryMappedDevice* target = FindDevice(address, offset)) {
        target->WriteWord(offset, data);
    }
}

// ------------------------------------------------------------------------------
// 32-Bit (Longword) Memory Operations (Simulated as two 16-bit accesses)
// ------------------------------------------------------------------------------
// The physical 68000 data bus is 16-bit wide. Reading/writing a 32-bit value
// requires two consecutive 16-bit operations.
// Due to Big-Endianness: High Word is at 'address', Low Word is at 'address + 2'.
// ------------------------------------------------------------------------------
Longword MainBus::ReadLongword(Address address) {
    Word highWord = ReadWord(address);
    Word lowWord  = ReadWord(address + 2);
    
    return (static_cast<Longword>(highWord) << 16) | lowWord;
}

void MainBus::WriteLongword(Address address, Longword data) {
    Word highWord = static_cast<Word>(data >> 16);
    Word lowWord  = static_cast<Word>(data & 0xFFFF);
    
    WriteWord(address, highWord);
    WriteWord(address + 2, lowWord);
}

} // namespace GenesisEmu::Core