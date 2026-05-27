// ==============================================================================
// GenesisEmu - M68k Arithmetic Operations Executor (Core Domain)
// ==============================================================================
// This file executes the Arithmetic instructions: ADD, SUB, ADDQ, SUBQ, ADDX, SUBX, CMP, CMPI, TST.
//
// SOLID Compliance:
// 1. Single Responsibility Principle (SRP):
//    It is solely responsible for implementing CPU arithmetic operations and comparisons.
// ==============================================================================

#pragma once

#include "../M68k.h"
#include "../M68kArithmetic.h"
#include "../M68kCoreInstructions.h"
#include "../M68kAddressing.h"

namespace GenesisEmu::Core::Domain::M68k::Executors {

/**
 * @class ArithmeticExecutor
 * @brief Stateless executor for standard, multi-precision, and quick arithmetic.
 */
class ArithmeticExecutor {
public:
    ArithmeticExecutor() = delete;

    /**
     * @brief Executes ADD, SUB, ADDQ, SUBQ, ADDX, SUBX, CMP, CMPI, or TST instruction.
     * @return Clock cycles consumed by the operation.
     */
    static int Execute(const DecodedInstruction& inst, M68k& cpu, Common::IBus* bus, Common::Word opcode) {
        (void)opcode;
        
        if (inst.type == OpType::TST) {
            Common::Longword value = M68kAddressing::ReadOperand(inst.srcMode, inst.srcRegister, inst.size, cpu, bus);
            Common::Word sr = cpu.GetSR();
            M68kCoreInstructions::ExecuteTST(value, inst.size, sr);
            cpu.SetSR(sr);
            return 8;
        }

        if (inst.type == OpType::CMPI) {
            Common::Longword immediateValue = 0;
            Common::Word sr = cpu.GetSR();
            
            if (inst.size == OperandSize::LONG) {
                Common::Word hi = bus->ReadWord(cpu.GetPC()); cpu.SetPC(cpu.GetPC() + 2);
                Common::Word lo = bus->ReadWord(cpu.GetPC()); cpu.SetPC(cpu.GetPC() + 2);
                immediateValue = (static_cast<Common::Longword>(hi) << 16) | lo;
                M68kCoreInstructions::ExecuteCMP(M68kAddressing::ReadOperand(inst.destMode, inst.destRegister, inst.size, cpu, bus), immediateValue, inst.size, sr);
                cpu.SetSR(sr);
                return 14;
            } else {
                immediateValue = (inst.size == OperandSize::WORD) ? bus->ReadWord(cpu.GetPC()) : (bus->ReadWord(cpu.GetPC()) & 0xFF);
                cpu.SetPC(cpu.GetPC() + 2);
                M68kCoreInstructions::ExecuteCMP(M68kAddressing::ReadOperand(inst.destMode, inst.destRegister, inst.size, cpu, bus), immediateValue, inst.size, sr);
                cpu.SetSR(sr);
                return 8;
            }
        }

        if (inst.type == OpType::CMP) {
            Common::Word sr = cpu.GetSR();
            M68kCoreInstructions::ExecuteCMP(M68kAddressing::ReadOperand(inst.destMode, inst.destRegister, inst.size, cpu, bus), M68kAddressing::ReadOperand(inst.srcMode, inst.srcRegister, inst.size, cpu, bus), inst.size, sr);
            cpu.SetSR(sr);
            return (inst.size == OperandSize::LONG) ? 8 : 4;
        }

        if (inst.type == OpType::ADDQ) {
            Common::Longword srcVal  = inst.immediateData;
            Common::Longword destVal = M68kAddressing::ReadOperand(inst.destMode, inst.destRegister, inst.size, cpu, bus);
            Common::Word sr = cpu.GetSR();
            
            Common::Longword result  = M68kArithmetic::ExecuteADD(destVal, srcVal, inst.size, sr);
            cpu.SetSR(sr);
            
            M68kAddressing::WriteOperand(inst.destMode, inst.destRegister, inst.size, result, cpu, bus);
            return (inst.size == OperandSize::LONG) ? 8 : 4;
        }

        if (inst.type == OpType::ADD) {
            Common::Longword srcVal  = M68kAddressing::ReadOperand(inst.srcMode, inst.srcRegister, inst.size, cpu, bus);
            if (inst.destMode == AddressingMode::AddressRegisterDirect) {
                if (inst.size == OperandSize::WORD) srcVal = static_cast<Common::Longword>(static_cast<std::int32_t>(static_cast<std::int16_t>(srcVal & 0xFFFF)));
                cpu.SetARegister(inst.destRegister, cpu.GetARegister(inst.destRegister) + srcVal);
                return (inst.size == OperandSize::LONG) ? 12 : 8;
            } else {
                Common::Word sr = cpu.GetSR();
                Common::Longword result = M68kArithmetic::ExecuteADD(M68kAddressing::ReadOperand(inst.destMode, inst.destRegister, inst.size, cpu, bus), srcVal, inst.size, sr);
                cpu.SetSR(sr);
                
                M68kAddressing::WriteOperand(inst.destMode, inst.destRegister, inst.size, result, cpu, bus);
                return (inst.srcMode == AddressingMode::Immediate) ? ((inst.size == OperandSize::LONG) ? 16 : 8) : 4; 
            }
        }

        if (inst.type == OpType::SUBQ) {
            Common::Longword srcVal = inst.immediateData;
            if (inst.destMode == AddressingMode::AddressRegisterDirect) {
                cpu.SetARegister(inst.destRegister, cpu.GetARegister(inst.destRegister) - srcVal);
            } else {
                Common::Word sr = cpu.GetSR();
                Common::Longword result = M68kArithmetic::ExecuteSUB(M68kAddressing::ReadOperand(inst.destMode, inst.destRegister, inst.size, cpu, bus), srcVal, inst.size, sr);
                cpu.SetSR(sr);
                
                M68kAddressing::WriteOperand(inst.destMode, inst.destRegister, inst.size, result, cpu, bus);
            }
            return (inst.size == OperandSize::LONG) ? 8 : 4;
        }

        if (inst.type == OpType::SUB) {
            Common::Longword srcVal  = M68kAddressing::ReadOperand(inst.srcMode, inst.srcRegister, inst.size, cpu, bus);
            if (inst.destMode == AddressingMode::AddressRegisterDirect) {
                if (inst.size == OperandSize::WORD) srcVal = static_cast<Common::Longword>(static_cast<std::int32_t>(static_cast<std::int16_t>(srcVal & 0xFFFF)));
                cpu.SetARegister(inst.destRegister, cpu.GetARegister(inst.destRegister) - srcVal);
                return (inst.size == OperandSize::LONG) ? 12 : 8;
            } else {
                Common::Word sr = cpu.GetSR();
                Common::Longword result = M68kArithmetic::ExecuteSUB(M68kAddressing::ReadOperand(inst.destMode, inst.destRegister, inst.size, cpu, bus), srcVal, inst.size, sr);
                cpu.SetSR(sr);
                
                M68kAddressing::WriteOperand(inst.destMode, inst.destRegister, inst.size, result, cpu, bus);
                return (inst.srcMode == AddressingMode::Immediate) ? ((inst.size == OperandSize::LONG) ? 16 : 8) : 4; 
            }
        }

        if (inst.type == OpType::ADDX) {
            Common::Longword srcVal  = M68kAddressing::ReadOperand(inst.srcMode, inst.srcRegister, inst.size, cpu, bus);
            Common::Longword destVal = M68kAddressing::ReadOperand(inst.destMode, inst.destRegister, inst.size, cpu, bus);
            Common::Longword ext = cpu.GetFlagExtend() ? 1 : 0;
            Common::Longword mask = (inst.size == OperandSize::BYTE) ? 0xFF : (inst.size == OperandSize::WORD) ? 0xFFFF : 0xFFFFFFFF;
            Common::Longword d = destVal & mask;
            Common::Longword s = srcVal & mask;
            Common::Longword result = (d + s + ext) & mask;

            auto IsSignBitSetLocal = [](Common::Longword val, OperandSize sz) {
                if (sz == OperandSize::BYTE) return (val & 0x80) != 0;
                if (sz == OperandSize::WORD) return (val & 0x8000) != 0;
                return (val & 0x80000000) != 0;
            };

            bool dSign = IsSignBitSetLocal(d, inst.size);
            bool sSign = IsSignBitSetLocal(s, inst.size);
            bool rSign = IsSignBitSetLocal(result, inst.size);

            Common::Word sr = cpu.GetSR();
            sr &= ~0x001B; 
            if (rSign) sr |= 0x0008; 
            if (result != 0) sr &= ~0x0004; 
            if (dSign == sSign && dSign != rSign) sr |= 0x0002;
            
            bool carry = false;
            if (inst.size == OperandSize::BYTE) carry = (destVal & 0xFF) + (srcVal & 0xFF) + ext > 0xFF;
            else if (inst.size == OperandSize::WORD) carry = (destVal & 0xFFFF) + (srcVal & 0xFFFF) + ext > 0xFFFF;
            else carry = (static_cast<std::uint64_t>(destVal) + srcVal + ext) > 0xFFFFFFFFu;
            
            if (carry) { sr |= 0x0001; sr |= 0x0010; }
            cpu.SetSR(sr);

            M68kAddressing::WriteOperand(inst.destMode, inst.destRegister, inst.size, result, cpu, bus);
            if (inst.destMode == AddressingMode::DataRegisterDirect) {
                return (inst.size == OperandSize::LONG) ? 8 : 4;
            } else return (inst.size == OperandSize::LONG) ? 30 : 18;
        }

        if (inst.type == OpType::SUBX) {
            Common::Longword srcVal  = M68kAddressing::ReadOperand(inst.srcMode, inst.srcRegister, inst.size, cpu, bus);
            Common::Longword destVal = M68kAddressing::ReadOperand(inst.destMode, inst.destRegister, inst.size, cpu, bus);
            Common::Longword ext = cpu.GetFlagExtend() ? 1 : 0;
            Common::Longword mask = (inst.size == OperandSize::BYTE) ? 0xFF : (inst.size == OperandSize::WORD) ? 0xFFFF : 0xFFFFFFFF;
            Common::Longword d = destVal & mask;
            Common::Longword s = srcVal & mask;
            Common::Longword result = (d - s - ext) & mask;

            auto IsSignBitSetLocal = [](Common::Longword val, OperandSize sz) {
                if (sz == OperandSize::BYTE) return (val & 0x80) != 0;
                if (sz == OperandSize::WORD) return (val & 0x8000) != 0;
                return (val & 0x80000000) != 0;
            };

            bool dSign = IsSignBitSetLocal(d, inst.size);
            bool sSign = IsSignBitSetLocal(s, inst.size);
            bool rSign = IsSignBitSetLocal(result, inst.size);

            Common::Word sr = cpu.GetSR();
            sr &= ~0x001B; 
            if (rSign) sr |= 0x0008; 
            if (result != 0) sr &= ~0x0004; 
            if (dSign != sSign && dSign != rSign) sr |= 0x0002;
            
            bool borrow = false;
            if (inst.size == OperandSize::BYTE) borrow = (destVal & 0xFF) < (srcVal & 0xFF) + ext;
            else if (inst.size == OperandSize::WORD) borrow = (destVal & 0xFFFF) < (srcVal & 0xFFFF) + ext;
            else borrow = static_cast<std::uint64_t>(destVal) < static_cast<std::uint64_t>(srcVal) + ext;
            
            if (borrow) { sr |= 0x0001; sr |= 0x0010; }
            cpu.SetSR(sr);

            M68kAddressing::WriteOperand(inst.destMode, inst.destRegister, inst.size, result, cpu, bus);
            if (inst.destMode == AddressingMode::DataRegisterDirect) {
                return (inst.size == OperandSize::LONG) ? 8 : 4;
            } else return (inst.size == OperandSize::LONG) ? 30 : 18;
        }

        return 4;
    }
};

} // namespace GenesisEmu::Core::Domain::M68k::Executors