// ==============================================================================
// GenesisEmu - M68k Flow Control Operations Executor (Core Domain)
// ==============================================================================
// This file executes the Flow Control instructions: JMP, JSR, RTS, RTE, BRA, Bcc, BSR, DBF.
//
// SOLID Compliance:
// 1. Single Responsibility Principle (SRP):
//    It is solely responsible for implementing CPU program counter branching and subroutines.
// ==============================================================================

#pragma once

#include "../M68k.h"
#include "../M68kFlowControl.h"
#include "../M68kAddressing.h"

namespace GenesisEmu::Core::Domain::M68k::Executors {

/**
 * @class FlowExecutor
 * @brief Stateless executor for program branching, looping, and jumping.
 */
class FlowExecutor {
public:
    FlowExecutor() = delete;

    /**
     * @brief Executes JMP, JSR, RTS, RTE, BRA, Bcc, BSR, or DBF instruction.
     * @return Clock cycles consumed by the operation.
     */
    static int Execute(const DecodedInstruction& inst, M68k& cpu, Common::IBus* bus, Common::Word opcode, Common::Address instructionPC) {
        Common::Address pc = cpu.GetPC();

        if (inst.type == OpType::JMP) {
            Common::Address targetAddress = M68kAddressing::ResolveAddress(inst.destMode, inst.destRegister, OperandSize::NONE, cpu, bus);
            cpu.SetPC(targetAddress);
            
            if (inst.destMode == AddressingMode::AbsoluteLong) return 16;
            else if (inst.destMode == AddressingMode::AbsoluteShort || inst.destMode == AddressingMode::AddressRegisterIndirect) return 12;
            else return 14;
        }

        if (inst.type == OpType::JSR) {
            Common::Address targetAddress = M68kAddressing::ResolveAddress(inst.destMode, inst.destRegister, OperandSize::NONE, cpu, bus);
            Common::Longword sp = cpu.GetARegister(7);
            
            M68kFlowControl::ExecuteJSR(bus, pc, sp, targetAddress);
            
            cpu.SetARegister(7, sp);
            cpu.SetPC(pc);
            
            if (inst.destMode == AddressingMode::AddressRegisterIndex || inst.destMode == AddressingMode::ProgramCounterIndex) return 20;
            return 16;
        }

        if (inst.type == OpType::RTS) {
            Common::Longword sp = cpu.GetARegister(7);
            
            M68kFlowControl::ExecuteRTS(bus, pc, sp);
            
            cpu.SetARegister(7, sp);
            cpu.SetPC(pc);
            return 16;
        }

        if (inst.type == OpType::RTE) {
            Common::Longword sp = cpu.GetARegister(7);
            
            // 1. Pop Status Register (SR) from Stack (16-bit word)
            Common::Word sr = bus->ReadWord(sp);
            sp += 2;
            
            // 2. Pop Program Counter (PC) from Stack (32-bit longword)
            Common::Address returnPC = bus->ReadLongword(sp);
            sp += 4;
            
            cpu.SetSR(sr);
            cpu.SetPC(returnPC);
            cpu.SetARegister(7, sp);
            
            // Standard RTE execution consumes exactly 20 clock cycles
            return 20;
        }

        if (inst.type == OpType::BSR) {
            Common::Longword sp = cpu.GetARegister(7);
            
            int cycles = 0;
            if (inst.size == OperandSize::WORD) {
                Common::Word extension = bus->ReadWord(cpu.GetPC()); cpu.SetPC(cpu.GetPC() + 2);
                pc = cpu.GetPC(); // Sync PC local state after extension fetch
                std::int16_t displacement = static_cast<std::int16_t>(extension);
                cycles = M68kFlowControl::ExecuteBSR(bus, pc, sp, displacement, instructionPC);
            } else if (inst.size == OperandSize::BYTE) {
                std::int8_t displacement8 = static_cast<std::int8_t>(opcode & 0x00FF);
                cycles = M68kFlowControl::ExecuteBSR(bus, pc, sp, displacement8, instructionPC);
            }
            
            cpu.SetARegister(7, sp);
            cpu.SetPC(pc);
            return cycles;
        }

        if (inst.type == OpType::DBF) {
            Common::Word extension = bus->ReadWord(cpu.GetPC()); cpu.SetPC(cpu.GetPC() + 2);
            pc = cpu.GetPC(); // Sync PC local state after extension fetch
            std::int16_t displacement = static_cast<std::int16_t>(extension);
            Common::Word counter = static_cast<Common::Word>(cpu.GetDRegister(inst.srcRegister) & 0xFFFF);
            counter--;
            
            Common::Longword currentD = cpu.GetDRegister(inst.srcRegister);
            cpu.SetDRegister(inst.srcRegister, (currentD & 0xFFFF0000) | counter);
            
            if (counter != 0xFFFF) {
                cpu.SetPC((instructionPC + 2) + displacement);
                return 10; 
            } else return 14; 
        }

        // Handles standard conditional branch families (Bcc)
        bool takeBranch = false;
        bool n = cpu.GetFlagNegative();
        bool v = cpu.GetFlagOverflow();
        bool z = cpu.GetFlagZero();
        bool c = cpu.GetFlagCarry();

        if (inst.type == OpType::BRA)      takeBranch = true; 
        else if (inst.type == OpType::BCC) takeBranch = !c; 
        else if (inst.type == OpType::BCS) takeBranch = c;  
        else if (inst.type == OpType::BEQ) takeBranch = z; 
        else if (inst.type == OpType::BGE) takeBranch = (n == v);  
        else if (inst.type == OpType::BGT) takeBranch = (n == v) && !z;
        else if (inst.type == OpType::BHI) takeBranch = !c && !z;
        else if (inst.type == OpType::BLE) takeBranch = z || (n != v);
        else if (inst.type == OpType::BLS) takeBranch = c || z;
        else if (inst.type == OpType::BLT) takeBranch = (n != v);
        else if (inst.type == OpType::BMI) takeBranch = n;
        else if (inst.type == OpType::BNE) takeBranch = !z;
        else if (inst.type == OpType::BPL) takeBranch = !n;
        else if (inst.type == OpType::BVC) takeBranch = !v;
        else if (inst.type == OpType::BVS) takeBranch = v;

        if (inst.size == OperandSize::WORD) {
            Common::Word extension = bus->ReadWord(cpu.GetPC()); cpu.SetPC(cpu.GetPC() + 2);
            pc = cpu.GetPC(); // Sync PC local state after extension fetch
            std::int16_t displacement = static_cast<std::int16_t>(extension);
            if (takeBranch) {
                cpu.SetPC((instructionPC + 2) + displacement);
                return 10; 
            } else return 8;
        }
        else if (inst.size == OperandSize::BYTE) {
            std::int8_t displacement8 = static_cast<std::int8_t>(opcode & 0x00FF);
            if (takeBranch) {
                cpu.SetPC((instructionPC + 2) + displacement8);
                return 10; 
            } else return 8;  
        }

        return 4;
    }
};

} // namespace GenesisEmu::Core::Domain::M68k::Executors