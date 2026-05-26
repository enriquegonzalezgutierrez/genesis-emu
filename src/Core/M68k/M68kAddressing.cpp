// ==============================================================================
// GenesisEmu - M68k Addressing Mode Resolver Implementation (Core Domain)
// ==============================================================================
// This file implements effective address resolution, reading and writing 
// operands according to Motorola 68000 micro-architecture requirements.
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
            Word code = bus->ReadWord(cpu.GetPC());
            cpu.SetPC(cpu.GetPC() + 2);
            std::int16_t displacement = static_cast<std::int16_t>(code);
            return cpu.GetARegister(reg) + displacement;
        }

        case AddressingMode::AbsoluteShort: {
            Word code = bus->ReadWord(cpu.GetPC());
            cpu.SetPC(cpu.GetPC() + 2);
            std::int16_t shortAddr = static_cast<std::int16_t>(code);
            return static_cast<Address>(static_cast<std::int32_t>(shortAddr));
        }

        case AddressingMode::AbsoluteLong: {
            Word hi = bus->ReadWord(cpu.GetPC());
            cpu.SetPC(cpu.GetPC() + 2);
            Word lo = bus->ReadWord(cpu.GetPC());
            cpu.SetPC(cpu.GetPC() + 2);
            return (static_cast<Longword>(hi) << 16) | lo;
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