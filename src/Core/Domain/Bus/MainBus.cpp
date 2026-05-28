// ==============================================================================
// GenesisEmu - Main Bus Concrete Router Implementation (Core Domain)
// ==============================================================================
// This file implements the routing, mirroring, and Z80 subsystem handshake.
//
// SOLID Compliance:
// 1. Single Responsibility Principle (SRP):
//    It is strictly responsible for physical address validation, 24-bit masking,
//    and Z80 bus request/reset hardware emulation logic.
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
    // Mask down to 24-bit space (16MB physical limit of M68k)
    address &= 0x00FFFFFF;

    // Z80 Bus Request State ($A11100 / $A11101)
    if (address == 0x00A11100 || address == 0x00A11101) {
        bool z80BusGranted = m_z80BusRequested && !m_z80ResetHeld;
        return z80BusGranted ? 0x00 : 0x01; 
    }

    // Z80 Reset State ($A11200 / $A11201)
    if (address == 0x00A11200 || address == 0x00A11201) {
        return m_z80ResetHeld ? 0x00 : 0x01;
    }

    // Z80 Memory Window ($A00000 - $A0FFFF)
    if (address >= 0x00A00000 && address <= 0x00A0FFFF) {
        if (!m_z80BusRequested) {
            return 0xFF; // Open Bus
        }
        
        // Z80 RAM Region (8 KB)
        if (address <= 0x00A01FFF) {
            // FIXED: Return real written memory data so games can verify their sound drivers during boot.
            return m_z80Ram[address & 0x1FFF];
        } 
        
        // YM2612 or PSG region read
        return 0x00; 
    }

    // Standard peripheral routing
    Address offset = 0;
    if (IMemoryMappedDevice* target = FindDevice(address, offset)) {
        return target->ReadByte(offset);
    }

    // Unmapped Open Bus fallback
    return 0xFF;
}

void MainBus::WriteByte(Address address, Byte data) {
    address &= 0x00FFFFFF;

    // Z80 Bus Request ($A11100 / $A11101)
    if (address == 0x00A11100 || address == 0x00A11101) {
        m_z80BusRequested = (data & 0x01) != 0;
        return;
    }

    // Z80 Reset ($A11200 / $A11201)
    if (address == 0x00A11200 || address == 0x00A11201) {
        m_z80ResetHeld = (data & 0x01) == 0;
        return;
    }

    // Z80 Memory Window ($A00000 - $A0FFFF)
    if (address >= 0x00A00000 && address <= 0x00A0FFFF) {
        if (!m_z80BusRequested || m_z80ResetHeld) {
            return; 
        }
        
        if (address <= 0x00A01FFF) {
            m_z80Ram[address & 0x1FFF] = data;
        }
        return;
    }

    // Standard peripheral routing
    Address offset = 0;
    if (IMemoryMappedDevice* target = FindDevice(address, offset)) {
        target->WriteByte(offset, data);
        return;
    }
}

// ------------------------------------------------------------------------------
// 16-Bit (Word) Memory Operations
// ------------------------------------------------------------------------------
Word MainBus::ReadWord(Address address) {
    address &= 0x00FFFFFF;

    if (address == 0x00A11100) {
        bool z80BusGranted = m_z80BusRequested && !m_z80ResetHeld;
        return z80BusGranted ? 0x0000 : 0x0100;
    }

    if (address == 0x00A11200) {
        return m_z80ResetHeld ? 0x0000 : 0x0100;
    }

    // Z80 RAM Word reads
    if (address >= 0x00A00000 && address <= 0x00A01FFF) {
        if (!m_z80BusRequested) return 0xFFFF;
        Address idx = address & 0x1FFF;
        // FIXED: Reconstruct word from actual Z80 RAM bytes
        return (static_cast<Word>(m_z80Ram[idx]) << 8) | m_z80Ram[(idx + 1) & 0x1FFF];
    }

    // Standard peripheral routing
    Address offset = 0;
    if (IMemoryMappedDevice* target = FindDevice(address, offset)) {
        return target->ReadWord(offset);
    }

    // Open Bus
    return 0xFFFF;
}

void MainBus::WriteWord(Address address, Word data) {
    address &= 0x00FFFFFF;

    if (address == 0x00A11100) {
        m_z80BusRequested = (data & 0x0100) != 0;
        return;
    }

    if (address == 0x00A11200) {
        m_z80ResetHeld = (data & 0x0100) == 0;
        return;
    }

    // Z80 Window ($A00000 - $A0FFFF)
    if (address >= 0x00A00000 && address <= 0x00A0FFFF) {
        if (!m_z80BusRequested || m_z80ResetHeld) return;
        
        if (address <= 0x00A01FFF) {
            Address idx = address & 0x1FFF;
            m_z80Ram[idx] = static_cast<Byte>(data >> 8);
            m_z80Ram[(idx + 1) & 0x1FFF] = static_cast<Byte>(data & 0xFF);
        }
        return;
    }

    // Standard peripheral routing
    Address offset = 0;
    if (IMemoryMappedDevice* target = FindDevice(address, offset)) {
        target->WriteWord(offset, data);
        return;
    }
}

// ------------------------------------------------------------------------------
// 32-Bit (Longword) Memory Operations
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

} // namespace GenesisEmu::Core::Domain::Bus