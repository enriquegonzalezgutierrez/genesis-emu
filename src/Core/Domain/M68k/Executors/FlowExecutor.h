// ==============================================================================
// GenesisEmu - M68k Flow Control Operations Executor (Core Domain)
// ==============================================================================
// This file executes the Flow Control instructions: JMP, JSR, RTS, RTE, BRA, Bcc, BSR, DBF, SCC, DBCC.
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
 * @brief Stateless executor for program branching, looping, jumping, and conditional setters.
 */
class FlowExecutor {
public:
    FlowExecutor() = delete;

    /**
     * @brief Executes JMP, JSR, RTS, RTE, BRA, Bcc, BSR, DBF, SCC, or DBCC.
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
            
            // Fetch the updated Program Counter after resolving the target address.
            // This guarantees the return address pushed onto the stack correctly points
            // to the next instruction instead of the JSR's extension words.
            Common::Address returnPC = cpu.GetPC();
            M68kFlowControl::ExecuteJSR(bus, returnPC, sp, targetAddress);
            
            cpu.SetARegister(7, sp);
            cpu.SetPC(returnPC);
            
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

        // --- Decrement and Branch (DBF & DBcc) Unified Execution ---
        if (inst.type == OpType::DBF || inst.type == OpType::DBCC) {
            Common::Word extension = bus->ReadWord(cpu.GetPC()); 
            cpu.SetPC(cpu.GetPC() + 2);
            pc = cpu.GetPC(); // Sync PC local state after extension fetch
            std::int16_t displacement = static_cast<std::int16_t>(extension);
            
            bool cond = false;
            if (inst.type == OpType::DBCC) {
                bool n = cpu.GetFlagNegative();
                bool v = cpu.GetFlagOverflow();
                bool z = cpu.GetFlagZero();
                bool c = cpu.GetFlagCarry();

                // Evaluate conditional checks across the 16 M68k condition codes
                switch (inst.immediateData) {
                    case 0x0: cond = true; break;              // T (True)
                    case 0x1: cond = false; break;             // F (False)
                    case 0x2: cond = !c && !z; break;          // HI (High)
                    case 0x3: cond = c || z; break;            // LS (Low or Same)
                    case 0x4: cond = !c; break;                // CC (Carry Clear)
                    case 0x5: cond = c; break;                 // CS (Carry Set)
                    case 0x6: cond = !z; break;                // NE (Not Equal)
                    case 0x7: cond = z; break;                 // EQ (Equal)
                    case 0x8: cond = !v; break;                // VC (Overflow Clear)
                    case 0x9: cond = v; break;                 // VS (Overflow Set)
                    case 0xA: cond = !n; break;                // PL (Plus)
                    case 0xB: cond = n; break;                 // MI (Minus)
                    case 0xC: cond = (n == v); break;          // GE (Greater or Equal)
                    case 0xD: cond = (n != v); break;          // LT (Less Than)
                    case 0xE: cond = (n == v) && !z; break;    // GT (Greater Than)
                    case 0xF: cond = z || (n != v); break;     // LE (Less or Equal)
                }
            } else {
                // DBF (Decrement and Branch if False) is always hardcoded false
                cond = false;
            }

            if (cond) {
                // Condition met: loop terminates immediately, PC advances
                return 12; // True condition takes 12 clock cycles
            } else {
                // Condition false: decrement only the lower 16-bits of Dn
                Common::Word counter = static_cast<Common::Word>(cpu.GetDRegister(inst.srcRegister) & 0xFFFF);
                counter--;
                
                Common::Longword currentD = cpu.GetDRegister(inst.srcRegister);
                cpu.SetDRegister(inst.srcRegister, (currentD & 0xFFFF0000) | counter);
                
                if (counter != 0xFFFF) {
                    // Counter did not wrap to -1: branch is taken
                    cpu.SetPC((instructionPC + 2) + displacement);
                    return 10; 
                } else {
                    // Counter wrapped to -1: fall through, PC advances
                    return 14; 
                }
            }
        }

        // --- Condition Setter (Scc) Execution ---
        if (inst.type == OpType::SCC) {
            bool cond = false;
            bool n = cpu.GetFlagNegative();
            bool v = cpu.GetFlagOverflow();
            bool z = cpu.GetFlagZero();
            bool c = cpu.GetFlagCarry();

            // Evaluate conditional checks across the 16 M68k condition codes
            switch (inst.immediateData) {
                case 0x0: cond = true; break;              // T (True)
                case 0x1: cond = false; break;             // F (False)
                case 0x2: cond = !c && !z; break;          // HI (High)
                case 0x3: cond = c || z; break;            // LS (Low or Same)
                case 0x4: cond = !c; break;                // CC (Carry Clear)
                case 0x5: cond = c; break;                 // CS (Carry Set)
                case 0x6: cond = !z; break;                // NE (Not Equal)
                case 0x7: cond = z; break;                 // EQ (Equal)
                case 0x8: cond = !v; break;                // VC (Overflow Clear)
                case 0x9: cond = v; break;                 // VS (Overflow Set)
                case 0xA: cond = !n; break;                // PL (Plus)
                case 0xB: cond = n; break;                 // MI (Minus)
                case 0xC: cond = (n == v); break;          // GE (Greater or Equal)
                case 0xD: cond = (n != v); break;          // LT (Less Than)
                case 0xE: cond = (n == v) && !z; break;    // GT (Greater Than)
                case 0xF: cond = z || (n != v); break;     // LE (Less or Equal)
            }

            Common::Byte value = cond ? 0xFF : 0x00;
            M68kAddressing::WriteOperand(inst.destMode, inst.destRegister, OperandSize::BYTE, value, cpu, bus);
            
            return (inst.destMode == AddressingMode::DataRegisterDirect) ? 6 : 8;
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
        else if (inst.type == OpType::BSR) takeBranch = true; // Handled separately above
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