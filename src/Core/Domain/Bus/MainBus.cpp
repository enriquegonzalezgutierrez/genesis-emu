// ==============================================================================
// GenesisEmu - Main Bus concrete Router Implementation (Core Domain)
// ==============================================================================
// This file implements the routing, mirroring, and device lookup mechanics.
//
// SOLID Compliance:
// 1. Single Responsibility Principle (SRP):
//    It is strictly responsible for physical address validation, 24-bit masking,
//    and delegation of memory actions.
// ==============================================================================

#include "MainBus.h"

namespace GenesisEmu::Core::Domain::Bus {

using namespace GenesisEmu::Core::Domain::Common;

// ------------------------------------------------------------------------------
// Device Routing Engine
// ------------------------------------------------------------------------------
IMemoryMappedDevice* MainBus::FindDevice(Address address, Address& outRelativeOffset) {
    for (const auto& mapping : m_devices) {
        if (address >= mapping.startAddress && address <= mapping.endAddress) {
            // Compute offset relative to the base of the device's assigned range
            outRelativeOffset = address - mapping.startAddress;
            return mapping.device;
        }
    }
    return nullptr;
}

void MainBus::AttachDevice(IMemoryMappedDevice* device, Address startAddress, Address endAddress) {
    m_devices.push_back({device, startAddress, endAddress});
}

// ------------------------------------------------------------------------------
// 8-Bit (Byte) Memory Operations
// ------------------------------------------------------------------------------
Byte MainBus::ReadByte(Address address) {
    // Mask down to 24-bit space (16MB maximum address space of physical M68k)
    address &= 0x00FFFFFF;

    // Z80 Bus Request ($A11100 / $A11101)
    // If the 68000 has requested the bus, we return 0 (Granted). 
    // If it has released the bus, we return 1 (Z80 is running).
    if (address == 0x00A11100 || address == 0x00A11101) {
        return m_z80BusReq ? 0x00 : 0x01; 
    }

    Address offset = 0;
    if (IMemoryMappedDevice* target = FindDevice(address, offset)) {
        return target->ReadByte(offset);
    }

    // --- Audio Coprocessor Stub ---
    // If Z80 RAM and Audio Subsystems are unmapped, we force them to return 0x00.
    // Commercial games (like Sonic 1) write commands here and loop infinitely until
    // the Z80 clears the byte to 0x00. Returning 0x00 bypasses these audio hangs.
    if (address >= 0x00A00000 && address <= 0x00A0FFFF) {
        return 0x00;
    }

    // Return unmapped open bus value
    return 0xFF;
}

void MainBus::WriteByte(Address address, Byte data) {
    address &= 0x00FFFFFF;

    // Z80 Bus Request ($A11100 / $A11101)
    // Writing 1 requests the bus (halts Z80), writing 0 releases it.
    if (address == 0x00A11100 || address == 0x00A11101) {
        m_z80BusReq = (data & 0x01) != 0;
        return;
    }

    Address offset = 0;
    if (IMemoryMappedDevice* target = FindDevice(address, offset)) {
        target->WriteByte(offset, data);
    }
}

// ------------------------------------------------------------------------------
// 16-Bit (Word) Memory Operations
// ------------------------------------------------------------------------------
Word MainBus::ReadWord(Address address) {
    address &= 0x00FFFFFF;

    // Z80 Bus Request ($A11100)
    // Same logic as byte read, but the status is mapped to bit 8.
    if (address == 0x00A11100) {
        return m_z80BusReq ? 0x0000 : 0x0100;
    }

    Address offset = 0;
    if (IMemoryMappedDevice* target = FindDevice(address, offset)) {
        return target->ReadWord(offset);
    }

    // Audio Coprocessor Stub (Word context)
    if (address >= 0x00A00000 && address <= 0x00A0FFFF) {
        return 0x0000;
    }

    // Return unmapped open bus value
    return 0xFFFF;
}

void MainBus::WriteWord(Address address, Word data) {
    address &= 0x00FFFFFF;

    // Z80 Bus Request ($A11100)
    if (address == 0x00A11100) {
        m_z80BusReq = (data & 0x0100) != 0;
        return;
    }

    Address offset = 0;
    if (IMemoryMappedDevice* target = FindDevice(address, offset)) {
        target->WriteWord(offset, data);
    }
}

// ------------------------------------------------------------------------------
// 32-Bit (Longword) Memory Operations (Structured as sequential Word accesses)
// ------------------------------------------------------------------------------
Longword MainBus::ReadLongword(Address address) {
    // Read the most significant word first (Big-Endian format)
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

} // namespace GenesisEmu::Core::Domain::Bus