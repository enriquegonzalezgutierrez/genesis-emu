// ==============================================================================
// GenesisEmu - M68k Arithmetic Operations Executor (Core Domain)
// ==============================================================================
// This file executes the Arithmetic instructions: 
// ADD, SUB, ADDQ, SUBQ, ADDX, SUBX, CMP, CMPI, TST, MULU, MULS, DIVU, DIVS.
//
// SOLID Compliance:
// 1. Single Responsibility Principle (SRP):
//    It is solely responsible for implementing CPU arithmetic operations.
// ==============================================================================

#pragma once

#include "../M68k.h"
#include "../M68kArithmetic.h"
#include "../M68kCoreInstructions.h"
#include "../M68kAddressing.h"

namespace GenesisEmu::Core::Domain::M68k::Executors {

/**
 * @class ArithmeticExecutor
 * @brief Stateless executor for standard, multi-precision, and multiplication/division.
 */
class ArithmeticExecutor {
public:
    ArithmeticExecutor() = delete;

    /**
     * @brief Executes Arithmetic family instructions.
     * @return Clock cycles consumed.
     */
    static int Execute(const DecodedInstruction& inst, M68k& cpu, Common::IBus* bus, Common::Word opcode) {
        (void)opcode;

        // --- 1. Basic Tests (TST) ---
        if (inst.type == OpType::TST) {
            Common::Longword value = M68kAddressing::ReadOperand(inst.srcMode, inst.srcRegister, inst.size, cpu, bus);
            Common::Word sr = cpu.GetSR();
            M68kCoreInstructions::ExecuteTST(value, inst.size, sr);
            cpu.SetSR(sr);
            return 8;
        }

        // --- 2. Multiplications (MULU / MULS) ---
        if (inst.type == OpType::MULU || inst.type == OpType::MULS) {
            Common::Word src = static_cast<Common::Word>(M68kAddressing::ReadOperand(inst.srcMode, inst.srcRegister, OperandSize::WORD, cpu, bus) & 0xFFFF);
            Common::Word dest = static_cast<Common::Word>(cpu.GetDRegister(inst.destRegister) & 0xFFFF);
            Common::Longword result = 0;

            if (inst.type == OpType::MULU) {
                result = static_cast<Common::Longword>(src) * static_cast<Common::Longword>(dest);
            } else {
                // Signed multiplication requires casting to signed 16-bit before expanding
                result = static_cast<Common::Longword>(static_cast<std::int32_t>(static_cast<std::int16_t>(src)) * static_cast<std::int32_t>(static_cast<std::int16_t>(dest)));
            }

            cpu.SetDRegister(inst.destRegister, result);
            Common::Word sr = cpu.GetSR();
            sr &= ~0x000F; // MUL clears V and C
            if (result == 0) sr |= 0x0004;
            if ((result & 0x80000000) != 0) sr |= 0x0008;
            cpu.SetSR(sr);
            return 70; 
        }

        // --- 3. Divisions (DIVU / DIVS) ---
        if (inst.type == OpType::DIVU || inst.type == OpType::DIVS) {
            Common::Word divisor = static_cast<Common::Word>(M68kAddressing::ReadOperand(inst.srcMode, inst.srcRegister, OperandSize::WORD, cpu, bus) & 0xFFFF);
            
            // Critical hardware check: Zero Divide triggers Exception Vector 5
            if (divisor == 0) {
                cpu.Exception(5); 
                return 4;
            }

            Common::Longword dividend = cpu.GetDRegister(inst.destRegister);
            Common::Longword quotient = 0;
            Common::Longword remainder = 0;

            if (inst.type == OpType::DIVU) {
                quotient = dividend / divisor;
                remainder = dividend % divisor;
                if (quotient > 0xFFFF) { // Overflow V flag
                    Common::Word sr = cpu.GetSR();
                    sr |= 0x0002; cpu.SetSR(sr);
                    return 10;
                }
            } else {
                std::int32_t s_dividend = static_cast<std::int32_t>(dividend);
                std::int16_t s_divisor = static_cast<std::int16_t>(divisor);
                quotient = static_cast<Common::Longword>(s_dividend / s_divisor);
                remainder = static_cast<Common::Longword>(s_dividend % s_divisor);
                if (static_cast<std::int32_t>(quotient) > 32767 || static_cast<std::int32_t>(quotient) < -32768) {
                    Common::Word sr = cpu.GetSR();
                    sr |= 0x0002; cpu.SetSR(sr);
                    return 10;
                }
            }

            cpu.SetDRegister(inst.destRegister, (remainder << 16) | (quotient & 0xFFFF));
            Common::Word sr = cpu.GetSR();
            sr &= ~0x000F;
            if ((quotient & 0xFFFF) == 0) sr |= 0x0004;
            if ((quotient & 0x8000) != 0) sr |= 0x0008;
            cpu.SetSR(sr);
            return 140; 
        }

        // --- 4. Comparative (CMP / CMPI / CMPA / CMPM) ---
        if (inst.type == OpType::CMPI) {
            Common::Longword imm = 0;
            if (inst.size == OperandSize::LONG) {
                Common::Word hi = bus->ReadWord(cpu.GetPC()); cpu.SetPC(cpu.GetPC() + 2);
                Common::Word lo = bus->ReadWord(cpu.GetPC()); cpu.SetPC(cpu.GetPC() + 2);
                imm = (static_cast<Common::Longword>(hi) << 16) | lo;
            } else {
                imm = (inst.size == OperandSize::WORD) ? bus->ReadWord(cpu.GetPC()) : (bus->ReadWord(cpu.GetPC()) & 0xFF);
                cpu.SetPC(cpu.GetPC() + 2);
            }
            Common::Word sr = cpu.GetSR();
            M68kCoreInstructions::ExecuteCMP(M68kAddressing::ReadOperand(inst.destMode, inst.destRegister, inst.size, cpu, bus), imm, inst.size, sr);
            cpu.SetSR(sr);
            return (inst.size == OperandSize::LONG) ? 14 : 8;
        }

        if (inst.type == OpType::CMP) {
            Common::Longword srcVal = M68kAddressing::ReadOperand(inst.srcMode, inst.srcRegister, inst.size, cpu, bus);
            Common::Longword destVal = M68kAddressing::ReadOperand(inst.destMode, inst.destRegister, inst.size, cpu, bus);
            
            // In CMPA (Compare Address), Word operations sign-extend the source operand
            // to 32 bits and perform a Long comparison.
            OperandSize cmpSize = inst.size;
            if (inst.destMode == AddressingMode::AddressRegisterDirect && inst.size == OperandSize::WORD) {
                std::int16_t signedSrc = static_cast<std::int16_t>(srcVal & 0xFFFF);
                srcVal = static_cast<Common::Longword>(static_cast<std::int32_t>(signedSrc));
                cmpSize = OperandSize::LONG; // Override internal comparison to 32-bit
            }

            Common::Word sr = cpu.GetSR();
            M68kCoreInstructions::ExecuteCMP(destVal, srcVal, cmpSize, sr);
            cpu.SetSR(sr);
            
            return (inst.size == OperandSize::LONG) ? 8 : 4;
        }

        // --- 5. Add/Sub Quick (ADDQ / SUBQ) ---
        if (inst.type == OpType::ADDQ || inst.type == OpType::SUBQ) {
            Common::Longword srcVal  = inst.immediateData;
            
            // Operations on Address Registers modify the entire 32-bit register and do NOT update CCR.
            // We must bypass size-masked ReadOperand to protect the upper 16 bits of pointers.
            if (inst.destMode == AddressingMode::AddressRegisterDirect) {
                Common::Longword destVal = cpu.GetARegister(inst.destRegister);
                Common::Longword result = (inst.type == OpType::ADDQ) ? destVal + srcVal : destVal - srcVal;
                cpu.SetARegister(inst.destRegister, result);
            } else {
                Common::Longword destVal = M68kAddressing::ReadOperand(inst.destMode, inst.destRegister, inst.size, cpu, bus);
                Common::Word sr = cpu.GetSR();
                Common::Longword result = (inst.type == OpType::ADDQ) ? M68kArithmetic::ExecuteADD(destVal, srcVal, inst.size, sr) : M68kArithmetic::ExecuteSUB(destVal, srcVal, inst.size, sr);
                cpu.SetSR(sr);
                M68kAddressing::WriteOperand(inst.destMode, inst.destRegister, inst.size, result, cpu, bus);
            }
            return (inst.size == OperandSize::LONG) ? 8 : 4;
        }

        // --- 5.5. Negations (NEG / NEGX) ---
        if (inst.type == OpType::NEG) {
            Common::Longword value = M68kAddressing::ReadOperand(inst.destMode, inst.destRegister, inst.size, cpu, bus);
            Common::Word sr = cpu.GetSR();
            Common::Longword result = M68kCoreInstructions::ExecuteNEG(value, inst.size, sr);
            cpu.SetSR(sr);
            M68kAddressing::WriteOperand(inst.destMode, inst.destRegister, inst.size, result, cpu, bus);
            return (inst.destMode == AddressingMode::DataRegisterDirect) ? ((inst.size == OperandSize::LONG) ? 6 : 4) : ((inst.size == OperandSize::LONG) ? 20 : 12);
        }

        if (inst.type == OpType::NEGX) {
            Common::Longword value = M68kAddressing::ReadOperand(inst.destMode, inst.destRegister, inst.size, cpu, bus);
            Common::Word sr = cpu.GetSR();
            Common::Longword result = M68kCoreInstructions::ExecuteNEGX(value, inst.size, sr);
            cpu.SetSR(sr);
            M68kAddressing::WriteOperand(inst.destMode, inst.destRegister, inst.size, result, cpu, bus);
            return (inst.destMode == AddressingMode::DataRegisterDirect) ? ((inst.size == OperandSize::LONG) ? 6 : 4) : ((inst.size == OperandSize::LONG) ? 20 : 12);
        }

        // --- 6. Standard ADD / SUB ---
        if (inst.type == OpType::ADD || inst.type == OpType::SUB) {
            Common::Longword srcVal  = M68kAddressing::ReadOperand(inst.srcMode, inst.srcRegister, inst.size, cpu, bus);
            
            // ADDA / SUBA (Target is an Address Register) - CCR is not modified
            if (inst.destMode == AddressingMode::AddressRegisterDirect) {
                if (inst.size == OperandSize::WORD) {
                    // Sign extend word source to 32 bits
                    std::int16_t signedSrc = static_cast<std::int16_t>(srcVal & 0xFFFF);
                    srcVal = static_cast<Common::Longword>(static_cast<std::int32_t>(signedSrc));
                }
                Common::Longword destVal = cpu.GetARegister(inst.destRegister);
                Common::Longword res = (inst.type == OpType::ADD) ? destVal + srcVal : destVal - srcVal;
                cpu.SetARegister(inst.destRegister, res);
                return (inst.size == OperandSize::LONG) ? 12 : 8;
            } else {
                // Standard ADD / SUB
                Common::Word sr = cpu.GetSR();
                Common::Longword destVal = M68kAddressing::ReadOperand(inst.destMode, inst.destRegister, inst.size, cpu, bus);
                Common::Longword result = (inst.type == OpType::ADD) ? M68kArithmetic::ExecuteADD(destVal, srcVal, inst.size, sr) : M68kArithmetic::ExecuteSUB(destVal, srcVal, inst.size, sr);
                cpu.SetSR(sr);
                M68kAddressing::WriteOperand(inst.destMode, inst.destRegister, inst.size, result, cpu, bus);
                return (inst.srcMode == AddressingMode::Immediate) ? ((inst.size == OperandSize::LONG) ? 16 : 8) : 4; 
            }
        }

        // --- 7. Extended ADDX / SUBX ---
        if (inst.type == OpType::ADDX || inst.type == OpType::SUBX) {
            Common::Longword srcVal  = M68kAddressing::ReadOperand(inst.srcMode, inst.srcRegister, inst.size, cpu, bus);
            Common::Longword destVal = M68kAddressing::ReadOperand(inst.destMode, inst.destRegister, inst.size, cpu, bus);
            Common::Longword ext = cpu.GetFlagExtend() ? 1 : 0;
            Common::Word sr = cpu.GetSR();
            Common::Longword result = (inst.type == OpType::ADDX) ? M68kArithmetic::ExecuteADD(destVal, srcVal + ext, inst.size, sr) : M68kArithmetic::ExecuteSUB(destVal, srcVal + ext, inst.size, sr);
            cpu.SetSR(sr);
            M68kAddressing::WriteOperand(inst.destMode, inst.destRegister, inst.size, result, cpu, bus);
            return (inst.size == OperandSize::LONG) ? 8 : 4;
        }

        return 4;
    }
};

} // namespace GenesisEmu::Core::Domain::M68k::Executors