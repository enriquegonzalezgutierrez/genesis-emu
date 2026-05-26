// ==============================================================================
// GenesisEmu - MainBus Implementation (Core Domain)
// ==============================================================================
// This file contains the routing and address calculation logic for the MainBus.
// Updated to stub the Z80 Bus Request register ($A11100) to return 0 (granted),
// preventing CPU boot lockouts when communicating with the audio coprocessor.
// ==============================================================================

#include "MainBus.h"

namespace GenesisEmu::Core {

// ------------------------------------------------------------------------------
// Device Routing Engine
// ------------------------------------------------------------------------------
IMemoryMappedDevice* MainBus::FindDevice(Address address, Address& outRelativeOffset) {
    for (const auto& mapping : m_devices) {
        if (address >= mapping.startAddress && address <= mapping.endAddress) {
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
    address &= 0x00FFFFFF;
    
    // Stub Z80 Bus Request ($A11100 / $A11101) to return 0 (indicating granted)
    if (address == 0x00A11100 || address == 0x00A11101) {
        return 0x00;
    }
    
    Address offset = 0;
    if (IMemoryMappedDevice* target = FindDevice(address, offset)) {
        return target->ReadByte(offset);
    }
    return 0xFF;
}

void MainBus::WriteByte(Address address, Byte data) {
    address &= 0x00FFFFFF;
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
    
    // Stub Z80 Bus Request ($A11100) to return 0 (indicating granted)
    if (address == 0x00A11100) {
        return 0x0000;
    }
    
    Address offset = 0;
    if (IMemoryMappedDevice* target = FindDevice(address, offset)) {
        return target->ReadWord(offset);
    }
    return 0xFFFF;
}

void MainBus::WriteWord(Address address, Word data) {
    address &= 0x00FFFFFF;
    Address offset = 0;
    if (IMemoryMappedDevice* target = FindDevice(address, offset)) {
        target->WriteWord(offset, data);
    }
}

// ------------------------------------------------------------------------------
// 32-Bit (Longword) Memory Operations (Simulated as two 16-bit accesses)
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