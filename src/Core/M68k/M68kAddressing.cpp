// ==============================================================================
// GenesisEmu - M68k Addressing Mode Resolver Implementation (Core Domain)
// ==============================================================================
// This file implements effective address resolution, reading and writing 
// operands according to Motorola 68000 micro-architecture requirements.
// Fixed Mode 6 (Address Register Index) and Mode 7.3 (PC Index) execution logic.
// ==============================================================================

#include "M68kAddressing.h"
#include "M68k.h"

namespace GenesisEmu::Core {

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
            int inc = (size == OperandSize::BYTE) ? 1 : (size == OperandSize::LONG) ? 4 : 2;
            if (reg == 7 && inc == 1) inc = 2; // Keep SP aligned to Word limits
            cpu.SetARegister(reg, addr + inc);
            return addr;
        }

        case AddressingMode::AddressRegisterPredecrement: {
            Address addr = cpu.GetARegister(reg);
            int dec = (size == OperandSize::BYTE) ? 1 : (size == OperandSize::LONG) ? 4 : 2;
            if (reg == 7 && dec == 1) dec = 2; // Keep SP aligned to Word limits
            addr -= dec;
            cpu.SetARegister(reg, addr);
            return addr;
        }

        case AddressingMode::AddressRegisterDisplacement: {
            // Consumes 1 extension word: Signed 16-bit displacement
            Word code = bus->ReadWord(cpu.GetPC());
            cpu.SetPC(cpu.GetPC() + 2);
            std::int16_t displacement = static_cast<std::int16_t>(code);
            return cpu.GetARegister(reg) + displacement;
        }

        case AddressingMode::AddressRegisterIndex: {
            // Mode 6: Address Register Indirect with Index
            // Consumes 1 extension word containing the Index Register and an 8-bit signed displacement
            Word extension = bus->ReadWord(cpu.GetPC());
            cpu.SetPC(cpu.GetPC() + 2);
            
            // Extension word format: [D/A][Register 3 bits][W/L][000][8-bit displacement]
            bool isAddressReg = (extension & 0x8000) != 0;
            Byte indexRegNum  = (extension >> 12) & 0x07;
            bool isLongIndex  = (extension & 0x0800) != 0;
            
            std::int8_t disp8 = static_cast<std::int8_t>(extension & 0xFF);
            
            Longword indexVal = isAddressReg ? cpu.GetARegister(indexRegNum) : cpu.GetDRegister(indexRegNum);
            
            // If the index size is Word (not Long), sign-extend the lower 16 bits
            if (!isLongIndex) {
                std::int16_t signedIndex = static_cast<std::int16_t>(indexVal & 0xFFFF);
                indexVal = static_cast<Longword>(static_cast<std::int32_t>(signedIndex));
            }
            
            return cpu.GetARegister(reg) + indexVal + disp8;
        }

        case AddressingMode::AbsoluteShort: {
            Word code = bus->ReadWord(cpu.GetPC());
            cpu.SetPC(cpu.GetPC() + 2);
            std::int16_t shortAddr = static_cast<std::int16_t>(code);
            // Sign extended to 32-bit before becoming an address
            return static_cast<Address>(static_cast<std::int32_t>(shortAddr));
        }

        case AddressingMode::AbsoluteLong: {
            Word hi = bus->ReadWord(cpu.GetPC());
            cpu.SetPC(cpu.GetPC() + 2);
            Word lo = bus->ReadWord(cpu.GetPC());
            cpu.SetPC(cpu.GetPC() + 2);
            return (static_cast<Longword>(hi) << 16) | lo;
        }

        case AddressingMode::ProgramCounterDisplacement: {
            Address instructionPC = cpu.GetPC(); 
            Word code = bus->ReadWord(cpu.GetPC());
            cpu.SetPC(cpu.GetPC() + 2);
            std::int16_t displacement = static_cast<std::int16_t>(code);
            return instructionPC + displacement;
        }

        case AddressingMode::ProgramCounterIndex: {
            // Mode 7 Sub-mode 3: Program Counter Indirect with Index
            // Same logic as Address Register Index, but using the PC as the base address
            Address instructionPC = cpu.GetPC(); // PC points to the extension word
            Word extension = bus->ReadWord(cpu.GetPC());
            cpu.SetPC(cpu.GetPC() + 2);
            
            bool isAddressReg = (extension & 0x8000) != 0;
            Byte indexRegNum  = (extension >> 12) & 0x07;
            bool isLongIndex  = (extension & 0x0800) != 0;
            
            std::int8_t disp8 = static_cast<std::int8_t>(extension & 0xFF);
            
            Longword indexVal = isAddressReg ? cpu.GetARegister(indexRegNum) : cpu.GetDRegister(indexRegNum);
            
            if (!isLongIndex) {
                std::int16_t signedIndex = static_cast<std::int16_t>(indexVal & 0xFFFF);
                indexVal = static_cast<Longword>(static_cast<std::int32_t>(signedIndex));
            }
            
            return instructionPC + indexVal + disp8;
        }

        default:
            return 0; // Register modes do not have a physical memory address
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
            Word hi = bus->ReadWord(cpu.GetPC());
            cpu.SetPC(cpu.GetPC() + 2);
            Word lo = bus->ReadWord(cpu.GetPC());
            cpu.SetPC(cpu.GetPC() + 2);
            return (static_cast<Longword>(hi) << 16) | lo;
        } else {
            Word code = bus->ReadWord(cpu.GetPC());
            cpu.SetPC(cpu.GetPC() + 2);
            return code;
        }
    }

    // Resolve memory address for any memory-indirect modes
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
        Longword currentD = cpu.GetDRegister(reg);
        Longword mask = GetSizeMask(size);
        cpu.SetDRegister(reg, (currentD & ~mask) | (value & mask));
        return;
    }

    if (mode == AddressingMode::AddressRegisterDirect) {
        // Address Registers do not have byte writes. 
        // Word writes are always sign-extended to full 32-bit (Sega hardware specs)
        if (size == OperandSize::WORD) {
            std::int16_t signedVal = static_cast<std::int16_t>(value & 0xFFFF);
            cpu.SetARegister(reg, static_cast<Longword>(static_cast<std::int32_t>(signedVal)));
        } else {
            cpu.SetARegister(reg, value);
        }
        return;
    }

    // Protection: You cannot write to immediate data or Program Counter relative modes.
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

} // namespace GenesisEmu::Core