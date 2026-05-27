// ==============================================================================
// GenesisEmu - M68k Logical Operations Executor (Core Domain)
// ==============================================================================
// This file executes the Logical instructions: AND, OR, EOR, ANDI_TO_SR, ORI_TO_SR, EORI_TO_SR, NOT, CLR.
//
// SOLID Compliance:
// 1. Single Responsibility Principle (SRP):
//    It is solely responsible for implementing CPU logical and bitwise transformations.
// ==============================================================================

#pragma once

#include "../M68k.h"
#include "../M68kArithmetic.h"
#include "../M68kCoreInstructions.h"
#include "../M68kAddressing.h"

namespace GenesisEmu::Core::Domain::M68k::Executors {

/**
 * @class LogicalExecutor
 * @brief Stateless executor for logical, bit-clearing, and status-modification operations.
 */
class LogicalExecutor {
public:
    LogicalExecutor() = delete;

    /**
     * @brief Executes AND, OR, EOR, ANDI_TO_SR, ORI_TO_SR, EORI_TO_SR, NOT, or CLR instruction.
     * @return Clock cycles consumed by the operation.
     */
    static int Execute(const DecodedInstruction& inst, M68k& cpu, Common::IBus* bus, Common::Word opcode) {
        (void)opcode;

        if (inst.type == OpType::ANDI_TO_SR) {
            Common::Word val = bus->ReadWord(cpu.GetPC()); cpu.SetPC(cpu.GetPC() + 2);
            cpu.SetSR(cpu.GetSR() & val);
            return 12;
        }

        if (inst.type == OpType::ORI_TO_SR) {
            Common::Word val = bus->ReadWord(cpu.GetPC()); cpu.SetPC(cpu.GetPC() + 2);
            cpu.SetSR(cpu.GetSR() | val);
            return 12;
        }

        if (inst.type == OpType::EORI_TO_SR) {
            Common::Word val = bus->ReadWord(cpu.GetPC()); cpu.SetPC(cpu.GetPC() + 2);
            cpu.SetSR(cpu.GetSR() ^ val);
            return 12;
        }

        if (inst.type == OpType::NOT) {
            Common::Longword value = M68kAddressing::ReadOperand(inst.destMode, inst.destRegister, inst.size, cpu, bus);
            Common::Word sr = cpu.GetSR();
            Common::Longword result = M68kCoreInstructions::ExecuteNOT(value, inst.size, sr);
            cpu.SetSR(sr);
            M68kAddressing::WriteOperand(inst.destMode, inst.destRegister, inst.size, result, cpu, bus);
            if (inst.destMode == AddressingMode::DataRegisterDirect) {
                return (inst.size == OperandSize::LONG) ? 6 : 4;
            } else return (inst.size == OperandSize::LONG) ? 20 : 12;
        }

        if (inst.type == OpType::CLR) {
            M68kAddressing::WriteOperand(inst.destMode, inst.destRegister, inst.size, 0, cpu, bus);
            Common::Word sr = cpu.GetSR();
            sr &= ~0x000B; // Clear N, V, C
            sr |= 0x0004;  // Set Z
            cpu.SetSR(sr);
            return (inst.destMode == AddressingMode::DataRegisterDirect) ? 4 : 12;
        }

        if (inst.type == OpType::AND) {
            Common::Word sr = cpu.GetSR();
            Common::Longword result = M68kArithmetic::ExecuteAND(M68kAddressing::ReadOperand(inst.destMode, inst.destRegister, inst.size, cpu, bus), M68kAddressing::ReadOperand(inst.srcMode, inst.srcRegister, inst.size, cpu, bus), inst.size, sr);
            cpu.SetSR(sr);
            
            M68kAddressing::WriteOperand(inst.destMode, inst.destRegister, inst.size, result, cpu, bus);
            return (inst.srcMode == AddressingMode::Immediate) ? ((inst.size == OperandSize::LONG) ? 16 : 8) : 4; 
        }

        if (inst.type == OpType::OR) {
            Common::Word sr = cpu.GetSR();
            Common::Longword result = M68kArithmetic::ExecuteOR(M68kAddressing::ReadOperand(inst.destMode, inst.destRegister, inst.size, cpu, bus), M68kAddressing::ReadOperand(inst.srcMode, inst.srcRegister, inst.size, cpu, bus), inst.size, sr);
            cpu.SetSR(sr);
            
            M68kAddressing::WriteOperand(inst.destMode, inst.destRegister, inst.size, result, cpu, bus);
            return (inst.srcMode == AddressingMode::Immediate) ? ((inst.size == OperandSize::LONG) ? 16 : 8) : 4; 
        }

        if (inst.type == OpType::EOR) {
            Common::Word sr = cpu.GetSR();
            Common::Longword result = M68kArithmetic::ExecuteEOR(M68kAddressing::ReadOperand(inst.destMode, inst.destRegister, inst.size, cpu, bus), M68kAddressing::ReadOperand(inst.srcMode, inst.srcRegister, inst.size, cpu, bus), inst.size, sr);
            cpu.SetSR(sr);
            
            M68kAddressing::WriteOperand(inst.destMode, inst.destRegister, inst.size, result, cpu, bus);
            return (inst.srcMode == AddressingMode::Immediate) ? ((inst.size == OperandSize::LONG) ? 16 : 8) : 4; 
        }

        return 4;
    }
};

} // namespace GenesisEmu::Core::Domain::M68k::Executors