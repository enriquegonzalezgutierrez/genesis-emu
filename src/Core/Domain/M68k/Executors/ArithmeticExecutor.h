// ==============================================================================
// GenesisEmu - M68k Arithmetic Operations Executor (Core Domain)
// ==============================================================================
// This file executes the M68k Arithmetic family instructions: 
// ADD, SUB, ADDQ, SUBQ, ADDX, SUBX, CMP, CMPI, TST, MULU, MULS, DIVU, DIVS, NEG, NEGX.
//
// SOLID Compliance:
// 1. Single Responsibility Principle (SRP):
//    It is solely responsible for implementing CPU arithmetic operations and 
//    dispatching arithmetic hardware exceptions (like Divide-by-Zero).
// ==============================================================================

#pragma once

#include "../M68k.h"
#include "../M68kArithmetic.h"
#include "../M68kCoreInstructions.h"
#include "../M68kAddressing.h"

namespace GenesisEmu::Core::Domain::M68k::Executors {

/**
 * @class ArithmeticExecutor
 * @brief Stateless executor for standard, multi-precision, and hardware exception-guarded math.
 */
class ArithmeticExecutor {
public:
    ArithmeticExecutor() = delete; // Static utility executor

    /**
     * @brief Executes Arithmetic family instructions.
     * @return Clock cycles consumed by the operation.
     */
    static int Execute(const DecodedInstruction& inst, M68k& cpu, Common::IBus* bus, Common::Word opcode) {
        (void)opcode;

        // --- 1. Basic Tests (TST) ---
        if (inst.type == OpType::TST) {
            Common::Longword value = M68kAddressing::ReadOperand(inst.srcMode, inst.srcRegister, inst.size, cpu, bus);
            Common::Word sr = cpu.GetSR();
            M68kCoreInstructions::ExecuteTST(value, inst.size, sr);
            cpu.SetSR(sr);
            return (inst.srcMode == AddressingMode::DataRegisterDirect) ? 4 : 8;
        }

        // --- 2. Multiplications (MULU / MULS) ---
        if (inst.type == OpType::MULU || inst.type == OpType::MULS) {
            Common::Word src = static_cast<Common::Word>(M68kAddressing::ReadOperand(inst.srcMode, inst.srcRegister, OperandSize::WORD, cpu, bus) & 0xFFFF);
            Common::Word dest = static_cast<Common::Word>(cpu.GetDRegister(inst.destRegister) & 0xFFFF);
            Common::Longword result = 0;

            if (inst.type == OpType::MULU) {
                result = static_cast<Common::Longword>(src) * static_cast<Common::Longword>(dest);
            } else {
                // Signed multiplication requires sign-extending the 16-bit operands to 32-bit before multiplying
                std::int32_t s_src  = static_cast<std::int32_t>(static_cast<std::int16_t>(src));
                std::int32_t s_dest = static_cast<std::int32_t>(static_cast<std::int16_t>(dest));
                result = static_cast<Common::Longword>(s_src * s_dest);
            }

            cpu.SetDRegister(inst.destRegister, result);
            
            Common::Word sr = cpu.GetSR();
            sr &= ~0x000F; // Multiplication always clears Overflow (V) and Carry (C)
            if (result == 0) sr |= 0x0004; // Zero (Z)
            if ((result & 0x80000000) != 0) sr |= 0x0008; // Negative (N)
            cpu.SetSR(sr);
            
            // Standard hardware execution cycles for multiplication on the 68000
            return 70; 
        }

        // --- 3. Divisions (DIVU / DIVS) ---
        if (inst.type == OpType::DIVU || inst.type == OpType::DIVS) {
            Common::Word divisor = static_cast<Common::Word>(M68kAddressing::ReadOperand(inst.srcMode, inst.srcRegister, OperandSize::WORD, cpu, bus) & 0xFFFF);
            
            // CRITICAL: Hardware Exception - Division by zero triggers Exception Vector 5
            if (divisor == 0) {
                cpu.Exception(5); 
                return 42; // Exception processing overhead cycles
            }

            Common::Longword dividend = cpu.GetDRegister(inst.destRegister);
            Common::Longword quotient = 0;
            Common::Longword remainder = 0;

            if (inst.type == OpType::DIVU) {
                quotient = dividend / divisor;
                remainder = dividend % divisor;
                
                // V-Flag Quirk: If quotient exceeds 16 bits (0xFFFF), overflow occurs.
                // Hardware behavior: Destination register is NOT modified.
                if (quotient > 0xFFFF) { 
                    Common::Word sr = cpu.GetSR();
                    sr |= 0x0002; // Set Overflow (V), others unaffected
                    cpu.SetSR(sr);
                    return 10; // Instantly aborts with 10 cycles
                }
            } else {
                std::int32_t s_dividend = static_cast<std::int32_t>(dividend);
                std::int16_t s_divisor  = static_cast<std::int16_t>(divisor);
                std::int32_t s_quotient = s_dividend / s_divisor;
                remainder = static_cast<Common::Longword>(s_dividend % s_divisor);
                
                // Signed division overflow check (Must fit in signed 16-bit: -32768 to 32767)
                if (s_quotient > 32767 || s_quotient < -32768) {
                    Common::Word sr = cpu.GetSR();
                    sr |= 0x0002; // Set Overflow (V)
                    cpu.SetSR(sr);
                    return 10;
                }
                quotient = static_cast<Common::Longword>(s_quotient & 0xFFFF);
            }

            // Write results to target register: Remainder in upper word, Quotient in lower word
            cpu.SetDRegister(inst.destRegister, (remainder << 16) | (quotient & 0xFFFF));
            
            Common::Word sr = cpu.GetSR();
            sr &= ~0x000F; // Clears V and C
            if ((quotient & 0xFFFF) == 0) sr |= 0x0004; // Z
            if ((quotient & 0x8000) != 0) sr |= 0x0008; // N
            cpu.SetSR(sr);
            
            return (inst.type == OpType::DIVU) ? 140 : 158; 
        }

        // --- 4. Comparisons (CMP / CMPI) ---
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
            
            // Address Register Direct Targets (CMPA): Word operations sign-extend the 
            // source operand to 32 bits and perform a Long comparison.
            OperandSize cmpSize = inst.size;
            if (inst.destMode == AddressingMode::AddressRegisterDirect) {
                if (inst.size == OperandSize::WORD) {
                    std::int16_t signedSrc = static_cast<std::int16_t>(srcVal & 0xFFFF);
                    srcVal = static_cast<Common::Longword>(static_cast<std::int32_t>(signedSrc));
                    cmpSize = OperandSize::LONG; // Force 32-bit comparison
                }
            }

            Common::Word sr = cpu.GetSR();
            M68kCoreInstructions::ExecuteCMP(destVal, srcVal, cmpSize, sr);
            cpu.SetSR(sr);
            
            return (inst.size == OperandSize::LONG) ? 8 : 4;
        }

        // --- 5. Add / Subtract Quick (ADDQ / SUBQ) ---
        if (inst.type == OpType::ADDQ || inst.type == OpType::SUBQ) {
            Common::Longword srcVal  = inst.immediateData;
            
            // Operations on Address Registers modify the entire 32-bit register and do NOT update CCR.
            if (inst.destMode == AddressingMode::AddressRegisterDirect) {
                Common::Longword destVal = cpu.GetARegister(inst.destRegister);
                Common::Longword result = (inst.type == OpType::ADDQ) ? destVal + srcVal : destVal - srcVal;
                cpu.SetARegister(inst.destRegister, result);
            } else {
                Common::Word sr = cpu.GetSR();
                Common::Longword result = 0;

                if (inst.destMode == AddressingMode::DataRegisterDirect) {
                    Common::Longword destVal = cpu.GetDRegister(inst.destRegister);
                    result = (inst.type == OpType::ADDQ) 
                        ? M68kArithmetic::ExecuteADD(destVal, srcVal, inst.size, sr) 
                        : M68kArithmetic::ExecuteSUB(destVal, srcVal, inst.size, sr);
                    cpu.SetDRegister(inst.destRegister, result);
                } else {
                    Common::Address addr = M68kAddressing::ResolveAddress(inst.destMode, inst.destRegister, inst.size, cpu, bus);
                    Common::Longword destVal = (inst.size == OperandSize::LONG) ? bus->ReadLongword(addr) :
                                             (inst.size == OperandSize::WORD) ? bus->ReadWord(addr) : bus->ReadByte(addr);

                    result = (inst.type == OpType::ADDQ) 
                        ? M68kArithmetic::ExecuteADD(destVal, srcVal, inst.size, sr) 
                        : M68kArithmetic::ExecuteSUB(destVal, srcVal, inst.size, sr);

                    if (inst.size == OperandSize::LONG)      bus->WriteLongword(addr, result);
                    else if (inst.size == OperandSize::WORD) bus->WriteWord(addr, static_cast<Common::Word>(result & 0xFFFF));
                    else                                     bus->WriteByte(addr, static_cast<Common::Byte>(result & 0xFF));
                }
                cpu.SetSR(sr);
            }
            return (inst.size == OperandSize::LONG) ? 8 : 4;
        }

        // --- 6. Negations (NEG / NEGX) ---
        if (inst.type == OpType::NEG || inst.type == OpType::NEGX) {
            Common::Word sr = cpu.GetSR();
            Common::Longword result = 0;

            if (inst.destMode == AddressingMode::DataRegisterDirect) {
                Common::Longword value = cpu.GetDRegister(inst.destRegister);
                result = (inst.type == OpType::NEG) 
                    ? M68kCoreInstructions::ExecuteNEG(value, inst.size, sr) 
                    : M68kCoreInstructions::ExecuteNEGX(value, inst.size, sr);
                cpu.SetDRegister(inst.destRegister, result);
            } else {
                Common::Address addr = M68kAddressing::ResolveAddress(inst.destMode, inst.destRegister, inst.size, cpu, bus);
                Common::Longword value = (inst.size == OperandSize::LONG) ? bus->ReadLongword(addr) :
                                         (inst.size == OperandSize::WORD) ? bus->ReadWord(addr) : bus->ReadByte(addr);

                result = (inst.type == OpType::NEG) 
                    ? M68kCoreInstructions::ExecuteNEG(value, inst.size, sr) 
                    : M68kCoreInstructions::ExecuteNEGX(value, inst.size, sr);

                if (inst.size == OperandSize::LONG)      bus->WriteLongword(addr, result);
                else if (inst.size == OperandSize::WORD) bus->WriteWord(addr, static_cast<Common::Word>(result & 0xFFFF));
                else                                     bus->WriteByte(addr, static_cast<Common::Byte>(result & 0xFF));
            }
            cpu.SetSR(sr);
            return (inst.destMode == AddressingMode::DataRegisterDirect) 
                ? ((inst.size == OperandSize::LONG) ? 6 : 4) 
                : ((inst.size == OperandSize::LONG) ? 20 : 12);
        }

        // --- 7. Standard ADD / SUB ---
        if (inst.type == OpType::ADD || inst.type == OpType::SUB) {
            Common::Longword srcVal  = M68kAddressing::ReadOperand(inst.srcMode, inst.srcRegister, inst.size, cpu, bus);
            
            // ADDA / SUBA (Target is an Address Register) - CCR flags are unaffected
            if (inst.destMode == AddressingMode::AddressRegisterDirect) {
                if (inst.size == OperandSize::WORD) {
                    std::int16_t signedSrc = static_cast<std::int16_t>(srcVal & 0xFFFF);
                    srcVal = static_cast<Common::Longword>(static_cast<std::int32_t>(signedSrc));
                }
                Common::Longword destVal = cpu.GetARegister(inst.destRegister);
                Common::Longword res = (inst.type == OpType::ADD) ? destVal + srcVal : destVal - srcVal;
                cpu.SetARegister(inst.destRegister, res);
                return (inst.size == OperandSize::LONG) ? 12 : 8;
            } else {
                // Standard register/memory operations
                Common::Word sr = cpu.GetSR();
                Common::Longword result = 0;

                if (inst.destMode == AddressingMode::DataRegisterDirect) {
                    Common::Longword destVal = cpu.GetDRegister(inst.destRegister);
                    result = (inst.type == OpType::ADD) 
                        ? M68kArithmetic::ExecuteADD(destVal, srcVal, inst.size, sr) 
                        : M68kArithmetic::ExecuteSUB(destVal, srcVal, inst.size, sr);
                    cpu.SetDRegister(inst.destRegister, result);
                } else {
                    Common::Address addr = M68kAddressing::ResolveAddress(inst.destMode, inst.destRegister, inst.size, cpu, bus);
                    Common::Longword destVal = (inst.size == OperandSize::LONG) ? bus->ReadLongword(addr) :
                                             (inst.size == OperandSize::WORD) ? bus->ReadWord(addr) : bus->ReadByte(addr);

                    result = (inst.type == OpType::ADD) 
                        ? M68kArithmetic::ExecuteADD(destVal, srcVal, inst.size, sr) 
                        : M68kArithmetic::ExecuteSUB(destVal, srcVal, inst.size, sr);

                    if (inst.size == OperandSize::LONG)      bus->WriteLongword(addr, result);
                    else if (inst.size == OperandSize::WORD) bus->WriteWord(addr, static_cast<Common::Word>(result & 0xFFFF));
                    else                                     bus->WriteByte(addr, static_cast<Common::Byte>(result & 0xFF));
                }
                cpu.SetSR(sr);
                return (inst.srcMode == AddressingMode::Immediate) ? ((inst.size == OperandSize::LONG) ? 16 : 8) : 4; 
            }
        }

        // --- 8. Extended ADDX / SUBX (Multi-precision operations) ---
        if (inst.type == OpType::ADDX || inst.type == OpType::SUBX) {
            Common::Longword srcVal  = M68kAddressing::ReadOperand(inst.srcMode, inst.srcRegister, inst.size, cpu, bus);
            Common::Word sr = cpu.GetSR();
            Common::Longword result = 0;
            
            if (inst.destMode == AddressingMode::DataRegisterDirect) {
                Common::Longword destVal = cpu.GetDRegister(inst.destRegister);
                result = (inst.type == OpType::ADDX) 
                    ? M68kArithmetic::ExecuteADDX(destVal, srcVal, inst.size, sr) 
                    : M68kArithmetic::ExecuteSUBX(destVal, srcVal, inst.size, sr);
                cpu.SetDRegister(inst.destRegister, result);
            } else {
                Common::Address addr = M68kAddressing::ResolveAddress(inst.destMode, inst.destRegister, inst.size, cpu, bus);
                Common::Longword destVal = (inst.size == OperandSize::LONG) ? bus->ReadLongword(addr) :
                                         (inst.size == OperandSize::WORD) ? bus->ReadWord(addr) : bus->ReadByte(addr);

                result = (inst.type == OpType::ADDX) 
                    ? M68kArithmetic::ExecuteADDX(destVal, srcVal, inst.size, sr) 
                    : M68kArithmetic::ExecuteSUBX(destVal, srcVal, inst.size, sr);

                if (inst.size == OperandSize::LONG)      bus->WriteLongword(addr, result);
                else if (inst.size == OperandSize::WORD) bus->WriteWord(addr, static_cast<Common::Word>(result & 0xFFFF));
                else                                     bus->WriteByte(addr, static_cast<Common::Byte>(result & 0xFF));
            }
            cpu.SetSR(sr);
            return (inst.size == OperandSize::LONG) ? 8 : 4;
        }

        return 4;
    }
};

} // namespace GenesisEmu::Core::Domain::M68k::Executors