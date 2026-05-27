// ==============================================================================
// GenesisEmu - M68k Addressing Mode Resolver (Core Domain)
// ==============================================================================
// This file implements target address resolution, operand reading, and
// operand writing for the 12 addressing modes of the Motorola 68000.
//
// SOLID Compliance:
// 1. Single Responsibility Principle (SRP):
//    It manages strictly operand data routing and address indexing calculations.
// ==============================================================================

#include "M68kAddressing.h"
#include "M68k.h"

namespace GenesisEmu::Core::Domain::M68k {

using namespace GenesisEmu::Core::Domain::Common;

/**
 * @brief Helper to generate a bitmask corresponding to operand data sizes.
 */
static Longword GetSizeMask(OperandSize size) {
    switch (size) {
        case OperandSize::BYTE: return 0x000000FF;
        case OperandSize::WORD: return 0x0000FFFF;
        case OperandSize::LONG: return 0xFFFFFFFF;
        default:                return 0x00000000;
    }
}

// ------------------------------------------------------------------------------
// Effective Address (EA) Resolution
// ------------------------------------------------------------------------------
Address M68kAddressing::ResolveAddress(AddressingMode mode, Byte reg, OperandSize size, M68k& cpu, IBus* bus) {
    switch (mode) {
        case AddressingMode::AddressRegisterIndirect:
            return cpu.GetARegister(reg);

        case AddressingMode::AddressRegisterPostincrement: {
            Address addr = cpu.GetARegister(reg);
            int increment = (size == OperandSize::BYTE) ? 1 : (size == OperandSize::LONG) ? 4 : 2;
            
            // Stack Pointer Alignment Rule: A7 (SP) must remain word-aligned (even on byte actions)
            if (reg == 7 && increment == 1) {
                increment = 2;
            }
            
            cpu.SetARegister(reg, addr + increment);
            return addr;
        }

        case AddressingMode::AddressRegisterPredecrement: {
            Address addr = cpu.GetARegister(reg);
            int decrement = (size == OperandSize::BYTE) ? 1 : (size == OperandSize::LONG) ? 4 : 2;
            
            if (reg == 7 && decrement == 1) {
                decrement = 2;
            }
            
            addr -= decrement;
            cpu.SetARegister(reg, addr);
            return addr;
        }

        case AddressingMode::AddressRegisterDisplacement: {
            // Consumes 1 extension word representing a signed 16-bit displacement value
            Word extension = bus->ReadWord(cpu.GetPC());
            cpu.SetPC(cpu.GetPC() + 2);
            
            std::int16_t displacement = static_cast<std::int16_t>(extension);
            return cpu.GetARegister(reg) + displacement;
        }

        case AddressingMode::AddressRegisterIndex: {
            // Mode 6: Address Register Indirect with Index
            // Consumes 1 extension word containing index configurations and an 8-bit signed displacement
            Word extension = bus->ReadWord(cpu.GetPC());
            cpu.SetPC(cpu.GetPC() + 2);
            
            // Format: [D/A][Register (3 bits)][W/L][000][8-bit signed displacement]
            bool isAddressReg = (extension & 0x8000) != 0;
            Byte indexRegNum  = (extension >> 12) & 0x07;
            bool isLongIndex  = (extension & 0x0800) != 0;
            std::int8_t disp8 = static_cast<std::int8_t>(extension & 0xFF);
            
            Longword indexValue = isAddressReg ? cpu.GetARegister(indexRegNum) : cpu.GetDRegister(indexRegNum);
            
            // Sign-extend the lower word of the index value if index is Word sized (W/L bit is 0)
            if (!isLongIndex) {
                std::int16_t signedIndex = static_cast<std::int16_t>(indexValue & 0xFFFF);
                indexValue = static_cast<Longword>(static_cast<std::int32_t>(signedIndex));
            }
            
            return cpu.GetARegister(reg) + indexValue + disp8;
        }

        case AddressingMode::AbsoluteShort: {
            Word extension = bus->ReadWord(cpu.GetPC());
            cpu.SetPC(cpu.GetPC() + 2);
            
            // Sign-extend 16-bit address to 32-bit physical address space
            std::int16_t shortAddr = static_cast<std::int16_t>(extension);
            return static_cast<Address>(static_cast<std::int32_t>(shortAddr));
        }

        case AddressingMode::AbsoluteLong: {
            Word highWord = bus->ReadWord(cpu.GetPC());
            cpu.SetPC(cpu.GetPC() + 2);
            Word lowWord  = bus->ReadWord(cpu.GetPC());
            cpu.SetPC(cpu.GetPC() + 2);
            
            return (static_cast<Longword>(highWord) << 16) | lowWord;
        }

        case AddressingMode::ProgramCounterDisplacement: {
            Address instructionPC = cpu.GetPC(); 
            Word extension = bus->ReadWord(cpu.GetPC());
            cpu.SetPC(cpu.GetPC() + 2);
            
            std::int16_t displacement = static_cast<std::int16_t>(extension);
            return instructionPC + displacement;
        }

        case AddressingMode::ProgramCounterIndex: {
            // Mode 7 Sub-mode 3: Program Counter Indirect with Index
            Address instructionPC = cpu.GetPC(); 
            Word extension = bus->ReadWord(cpu.GetPC());
            cpu.SetPC(cpu.GetPC() + 2);
            
            bool isAddressReg = (extension & 0x8000) != 0;
            Byte indexRegNum  = (extension >> 12) & 0x07;
            bool isLongIndex  = (extension & 0x0800) != 0;
            std::int8_t disp8 = static_cast<std::int8_t>(extension & 0xFF);
            
            Longword indexValue = isAddressReg ? cpu.GetARegister(indexRegNum) : cpu.GetDRegister(indexRegNum);
            
            if (!isLongIndex) {
                std::int16_t signedIndex = static_cast<std::int16_t>(indexValue & 0xFFFF);
                indexValue = static_cast<Longword>(static_cast<std::int32_t>(signedIndex));
            }
            
            return instructionPC + indexValue + disp8;
        }

        default:
            return 0; // Register direct modes do not have physical address targets
    }
}

// ------------------------------------------------------------------------------
// Operand Read Channel
// ------------------------------------------------------------------------------
Longword M68kAddressing::ReadOperand(AddressingMode mode, Byte reg, OperandSize size, M68k& cpu, IBus* bus) {
    if (mode == AddressingMode::DataRegisterDirect) {
        return cpu.GetDRegister(reg) & GetSizeMask(size);
    }
    
    if (mode == AddressingMode::AddressRegisterDirect) {
        return cpu.GetARegister(reg) & GetSizeMask(size);
    }

    if (mode == AddressingMode::Immediate) {
        if (size == OperandSize::LONG) {
            Word highWord = bus->ReadWord(cpu.GetPC());
            cpu.SetPC(cpu.GetPC() + 2);
            Word lowWord  = bus->ReadWord(cpu.GetPC());
            cpu.SetPC(cpu.GetPC() + 2);
            return (static_cast<Longword>(highWord) << 16) | lowWord;
        } else {
            Word extension = bus->ReadWord(cpu.GetPC());
            cpu.SetPC(cpu.GetPC() + 2);
            return extension;
        }
    }

    Address targetAddress = ResolveAddress(mode, reg, size, cpu, bus);

    if (size == OperandSize::LONG) {
        return bus->ReadLongword(targetAddress);
    } else if (size == OperandSize::WORD) {
        return bus->ReadWord(targetAddress);
    } else {
        return bus->ReadByte(targetAddress);
    }
}

// ------------------------------------------------------------------------------
// Operand Write Channel
// ------------------------------------------------------------------------------
void M68kAddressing::WriteOperand(AddressingMode mode, Byte reg, OperandSize size, Longword value, M68k& cpu, IBus* bus) {
    if (mode == AddressingMode::DataRegisterDirect) {
        Longword currentRegister = cpu.GetDRegister(reg);
        Longword mask = GetSizeMask(size);
        cpu.SetDRegister(reg, (currentRegister & ~mask) | (value & mask));
        return;
    }

    if (mode == AddressingMode::AddressRegisterDirect) {
        // Word writes to address registers are always sign-extended to 32 bits
        if (size == OperandSize::WORD) {
            std::int16_t signedValue = static_cast<std::int16_t>(value & 0xFFFF);
            cpu.SetARegister(reg, static_cast<Longword>(static_cast<std::int32_t>(signedValue)));
        } else {
            cpu.SetARegister(reg, value);
        }
        return;
    }

    // Protection: Writes to Immediate constants or Program Counter targets are blocked
    if (mode == AddressingMode::Immediate || 
        mode == AddressingMode::ProgramCounterDisplacement || 
        mode == AddressingMode::ProgramCounterIndex) {
        return; 
    }

    Address targetAddress = ResolveAddress(mode, reg, size, cpu, bus);

    if (size == OperandSize::LONG) {
        bus->WriteLongword(targetAddress, value);
    } else if (size == OperandSize::WORD) {
        bus->WriteWord(targetAddress, static_cast<Word>(value & 0xFFFF));
    } else {
        bus->WriteByte(targetAddress, static_cast<Byte>(value & 0xFF));
    }
}

} // namespace GenesisEmu::Core::Domain::M68k