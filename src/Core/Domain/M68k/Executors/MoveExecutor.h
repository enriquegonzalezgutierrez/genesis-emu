// ==============================================================================
// GenesisEmu - M68k Move Operations Executor (Core Domain)
// ==============================================================================
// This file executes the Move instructions: MOVE, MOVEQ, MOVEM, MOVE_USP, MOVE_TO_SR, MOVE_FROM_SR, MOVE_TO_CCR, LEA, PEA.
//
// SOLID Compliance:
// 1. Single Responsibility Principle (SRP):
//    It is solely responsible for implementing memory and register data moves.
// ==============================================================================

#pragma once

#include "../M68k.h"
#include "../M68kCoreInstructions.h"
#include "../M68kAddressing.h"

namespace GenesisEmu::Core::Domain::M68k::Executors {

/**
 * @class MoveExecutor
 * @brief Stateless executor for standard and specialized MOVE operations.
 */
class MoveExecutor {
public:
    MoveExecutor() = delete;

    /**
     * @brief Executes MOVE, MOVEQ, MOVEM, MOVE_USP, MOVE_TO_SR, MOVE_FROM_SR, MOVE_TO_CCR, LEA, or PEA instruction.
     * @return Clock cycles consumed by the operation.
     */
    static int Execute(const DecodedInstruction& inst, M68k& cpu, Common::IBus* bus, Common::Word opcode) {
        if (inst.type == OpType::MOVEQ) {
            Common::Longword val = M68kCoreInstructions::ExecuteMOVEQ(static_cast<Common::Byte>(inst.immediateData & 0xFF));
            cpu.SetDRegister(inst.destRegister, val);
            
            Common::Word sr = cpu.GetSR();
            sr &= ~0x000F; // Clears V and C
            if (val == 0) sr |= 0x0004; 
            if ((val & 0x80000000) != 0) sr |= 0x0008; 
            cpu.SetSR(sr);
            return 4;
        }

        if (inst.type == OpType::MOVE_TO_SR) {
            Common::Word val = 0;
            if (inst.srcMode == AddressingMode::Immediate) {
                val = bus->ReadWord(cpu.GetPC());
                cpu.SetPC(cpu.GetPC() + 2);
            } else {
                val = static_cast<Common::Word>(M68kAddressing::ReadOperand(inst.srcMode, inst.srcRegister, inst.size, cpu, bus) & 0xFFFF);
            }
            cpu.SetSR(val);
            return 12;
        }

        if (inst.type == OpType::MOVE_FROM_SR) {
            Common::Word sr = cpu.GetSR();
            M68kAddressing::WriteOperand(inst.destMode, inst.destRegister, inst.size, sr, cpu, bus);
            return (inst.destMode == AddressingMode::DataRegisterDirect) ? 6 : 8;
        }

        if (inst.type == OpType::MOVE_TO_CCR) {
            Common::Word val = 0;
            if (inst.srcMode == AddressingMode::Immediate) {
                val = bus->ReadWord(cpu.GetPC());
                cpu.SetPC(cpu.GetPC() + 2);
            } else {
                val = static_cast<Common::Word>(M68kAddressing::ReadOperand(inst.srcMode, inst.srcRegister, inst.size, cpu, bus) & 0xFFFF);
            }
            // Retain the supervisor system byte (upper 8 bits) and overwrite the user CCR byte (lower 8 bits)
            Common::Word sr = cpu.GetSR();
            cpu.SetSR((sr & 0xFF00) | (val & 0x00FF));
            return 12;
        }

        if (inst.type == OpType::MOVE_USP) {
            bool directionToUsp = (opcode & 0x0008) != 0;
            int regIndex = opcode & 0x0007;
            if (directionToUsp) {
                cpu.SetUSP(cpu.GetARegister(regIndex));
            } else {
                cpu.SetARegister(regIndex, cpu.GetUSP());
            }
            return 4;
        }

        if (inst.type == OpType::LEA) {
            Common::Address targetAddress = M68kAddressing::ResolveAddress(inst.srcMode, inst.srcRegister, inst.size, cpu, bus);
            Common::Longword regVal = cpu.GetARegister(inst.destRegister);
            
            M68kCoreInstructions::ExecuteLEA(regVal, targetAddress);
            cpu.SetARegister(inst.destRegister, regVal);
            return 8;
        }

        if (inst.type == OpType::PEA) {
            Common::Address targetAddress = M68kAddressing::ResolveAddress(inst.destMode, inst.destRegister, inst.size, cpu, bus);
            Common::Longword sp = cpu.GetARegister(7);
            
            M68kCoreInstructions::ExecutePEA(bus, sp, targetAddress);
            cpu.SetARegister(7, sp);
            return 12;
        }

        if (inst.type == OpType::MOVEM) {
            Common::Word regMask = bus->ReadWord(cpu.GetPC());
            cpu.SetPC(cpu.GetPC() + 2);
            
            bool isLoad = (inst.immediateData & 0x0400) != 0;
            int regCount = 0;
            for (int i = 0; i < 16; ++i) {
                if ((regMask & (1 << i)) != 0) regCount++;
            }
            int step = (inst.size == OperandSize::LONG) ? 4 : 2;

            if (isLoad) {
                Common::Address addr = (inst.srcMode == AddressingMode::AddressRegisterPostincrement) ? cpu.GetARegister(inst.srcRegister) : M68kAddressing::ResolveAddress(inst.srcMode, inst.srcRegister, inst.size, cpu, bus);
                for (int i = 0; i < 16; ++i) {
                    if ((regMask & (1 << i)) != 0) {
                        Common::Longword val = (inst.size == OperandSize::LONG) ? bus->ReadLongword(addr) : static_cast<Common::Longword>(static_cast<std::int32_t>(static_cast<std::int16_t>(bus->ReadWord(addr))));
                        if (i < 8) cpu.SetDRegister(i, val); else cpu.SetARegister(i - 8, val);
                        addr += step;
                    }
                }
                if (inst.srcMode == AddressingMode::AddressRegisterPostincrement && ((regMask & (1 << (8 + inst.srcRegister))) == 0)) {
                    cpu.SetARegister(inst.srcRegister, addr);
                }
            } else {
                Common::Address addr = (inst.destMode == AddressingMode::AddressRegisterPredecrement) ? cpu.GetARegister(inst.destRegister) : M68kAddressing::ResolveAddress(inst.destMode, inst.destRegister, inst.size, cpu, bus);
                for (int i = 0; i < 16; ++i) {
                    if ((regMask & (1 << i)) != 0) {
                        if (inst.destMode == AddressingMode::AddressRegisterPredecrement) addr -= step;
                        Common::Longword val = (inst.destMode == AddressingMode::AddressRegisterPredecrement) ? ((i < 8) ? cpu.GetARegister(7 - i) : cpu.GetDRegister(15 - i)) : ((i < 8) ? cpu.GetDRegister(i) : cpu.GetARegister(i - 8));
                        if (inst.size == OperandSize::LONG) bus->WriteLongword(addr, val); else bus->WriteWord(addr, static_cast<Common::Word>(val & 0xFFFF));
                        if (inst.destMode != AddressingMode::AddressRegisterPredecrement) addr += step;
                    }
                }
                if (inst.destMode == AddressingMode::AddressRegisterPredecrement) {
                    cpu.SetARegister(inst.destRegister, addr);
                }
            }
            return (isLoad ? 12 : 8) + ((inst.size == OperandSize::LONG ? 8 : 4) * regCount);
        }

        if (inst.type == OpType::MOVE) {
            Common::Longword value = M68kAddressing::ReadOperand(inst.srcMode, inst.srcRegister, inst.size, cpu, bus);
            M68kAddressing::WriteOperand(inst.destMode, inst.destRegister, inst.size, value, cpu, bus);
            
            if (inst.destMode != AddressingMode::AddressRegisterDirect) {
                Common::Word sr = cpu.GetSR();
                sr &= ~0x0003; 
                Common::Longword mask = (inst.size == OperandSize::BYTE) ? 0xFF : (inst.size == OperandSize::WORD) ? 0xFFFF : 0xFFFFFFFF;
                Common::Longword msb  = (inst.size == OperandSize::BYTE) ? 0x80 : (inst.size == OperandSize::WORD) ? 0x8000 : 0x80000000;
                Common::Longword val  = value & mask;
                if (val == 0) sr |= 0x0004; else sr &= ~0x0004;
                if ((val & msb) != 0) sr |= 0x0008; else sr &= ~0x0008;
                cpu.SetSR(sr);
            }
            
            int cycles = 4; 
            if (inst.srcMode == AddressingMode::Immediate || inst.srcMode == AddressingMode::AddressRegisterPostincrement || inst.srcMode == AddressingMode::AddressRegisterPredecrement || inst.srcMode == AddressingMode::AddressRegisterDisplacement) cycles += 4;
            if (inst.destMode == AddressingMode::AddressRegisterIndirect || inst.destMode == AddressingMode::AbsoluteShort || inst.destMode == AddressingMode::AddressRegisterPredecrement || inst.destMode == AddressingMode::AddressRegisterDisplacement) cycles += 4;
            else if (inst.destMode == AddressingMode::AbsoluteLong) cycles += 8;
            return cycles; 
        }

        return 4;
    }
};

} // namespace GenesisEmu::Core::Domain::M68k::Executors