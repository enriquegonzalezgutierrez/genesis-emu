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
#include <iostream>
#include <iomanip>

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

    // Z80 Reset ($A11200 / $A11201)
    // Bit 0 is 0 for reset active (asserted), 1 for reset inactive.
    if (address == 0x00A11200 || address == 0x00A11201) {
        return m_z80Reset ? 0x01 : 0x00;
    }

    // Standard peripheral routing
    Address offset = 0;
    if (IMemoryMappedDevice* target = FindDevice(address, offset)) {
        return target->ReadByte(offset);
    }

    // --- Z80 Sound Subsystem Mapping ---
    if (address >= 0x00A00000 && address <= 0x00A0FFFF) {
        Byte value = 0x00;
        
        // 1. Z80 RAM Region (8 KB)
        if (address <= 0x00A01FFF) {
            value = m_z80Ram[address & 0x1FFF];
        } else {
            // 2. Unmapped Z80 / Audio Subsystem space (YM2612 / PSG registers)
            value = 0x00;
        }

        // --- Intelligent Audio Handshake Monitor ---
        // Expanded to capture all reads across the entire $A00000 - $A0FFFF space,
        // specifically targeting YM2612 status ports ($A04000) to find busy/timer loops.
        static Address lastLoggedAddress = 0;
        static int repeatCount = 0;

        if (address != lastLoggedAddress || repeatCount < 5) {
            std::cout << "[Z80 READ] Address: 0x" << std::hex << std::uppercase << address 
                      << " | Read Value: 0x" << std::setw(2) << std::setfill('0') << (int)value 
                      << std::dec << std::endl;
            if (address == lastLoggedAddress) {
                repeatCount++;
            } else {
                lastLoggedAddress = address;
                repeatCount = 0;
            }
        }
        return value;
    }

    // Return unmapped open bus value
    return 0xFF;
}

void MainBus::WriteByte(Address address, Byte data) {
    address &= 0x00FFFFFF;

     // --- TEMPORARY DIAGNOSTIC: TRACE WRITES ---
    if (address == 0x00FF7E1E) {
        std::cout << "[WRITE BYTE DEBUG] PC: 0x" << std::hex << std::uppercase << m_devices[0].device /* just print data */
                  << " | Write 0x" << (int)data << " to 0xFF7E1E" << std::dec << std::endl;
    }
    // ------------------------------------------

    // Z80 Bus Request ($A11100 / $A11101)
    // Writing 1 requests the bus (halts Z80), writing 0 releases it.
    if (address == 0x00A11100 || address == 0x00A11101) {
        m_z80BusReq = (data & 0x01) != 0;
        return;
    }

    // Z80 Reset ($A11200 / $A11201)
    // Writing 0 triggers a reset, writing 1 cancels the reset.
    if (address == 0x00A11200 || address == 0x00A11201) {
        m_z80Reset = (data & 0x01) != 0;
        return;
    }

    // Standard peripheral routing
    Address offset = 0;
    if (IMemoryMappedDevice* target = FindDevice(address, offset)) {
        target->WriteByte(offset, data);
        return;
    }

    // Z80 RAM write persistence
    if (address >= 0x00A00000 && address <= 0x00A01FFF) {
        m_z80Ram[address & 0x1FFF] = data;
        return;
    }

    // Ignore unmapped Z80 audio writes
    if (address >= 0x00A00000 && address <= 0x00A0FFFF) {
        return;
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

    // Z80 Reset ($A11200)
    if (address == 0x00A11200) {
        return m_z80Reset ? 0x0100 : 0x0000;
    }

    // Standard peripheral routing
    Address offset = 0;
    if (IMemoryMappedDevice* target = FindDevice(address, offset)) {
        return target->ReadWord(offset);
    }

    // Z80 RAM Word reads
    if (address >= 0x00A00000 && address <= 0x00A01FFF) {
        Address idx = address & 0x1FFF;
        return (static_cast<Word>(m_z80Ram[idx]) << 8) | m_z80Ram[(idx + 1) & 0x1FFF];
    }

    // Unmapped Audio Space
    if (address >= 0x00A00000 && address <= 0x00A0FFFF) {
        return 0x0000;
    }

    // Return unmapped open bus value
    return 0xFFFF;
}

void MainBus::WriteWord(Address address, Word data) {
    address &= 0x00FFFFFF;

    if (address == 0x00FF7E1E) {
        std::cout << "[WRITE WORD DEBUG] Write 0x" << std::hex << data << " to 0xFF7E1E" << std::dec << std::endl;
    }

    // Z80 Bus Request ($A11100)
    if (address == 0x00A11100) {
        m_z80BusReq = (data & 0x0100) != 0;
        return;
    }

    // Z80 Reset ($A11200)
    if (address == 0x00A11200) {
        m_z80Reset = (data & 0x0100) != 0;
        return;
    }

    // Standard peripheral routing
    Address offset = 0;
    if (IMemoryMappedDevice* target = FindDevice(address, offset)) {
        target->WriteWord(offset, data);
        return;
    }

    // Z80 RAM Word writes
    if (address >= 0x00A00000 && address <= 0x00A01FFF) {
        Address idx = address & 0x1FFF;
        m_z80Ram[idx] = static_cast<Byte>(data >> 8);
        m_z80Ram[(idx + 1) & 0x1FFF] = static_cast<Byte>(data & 0xFF);
        return;
    }

    // Unmapped Audio Space
    if (address >= 0x00A00000 && address <= 0x00A0FFFF) {
        return;
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