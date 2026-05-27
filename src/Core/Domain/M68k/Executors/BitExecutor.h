// ==============================================================================
// GenesisEmu - M68k Bit Operations Executor (Core Domain)
// ==============================================================================
// This file executes the Bit Manipulation instructions: BTST, BCHG, BCLR, BSET.
//
// SOLID Compliance:
// 1. Single Responsibility Principle (SRP):
//    It is solely responsible for implementing bitwise logic mutations.
// ==============================================================================

#pragma once

#include "../M68k.h"
#include "../M68kCoreInstructions.h"
#include "../M68kAddressing.h"

namespace GenesisEmu::Core::Domain::M68k::Executors {

/**
 * @class BitExecutor
 * @brief Stateless executor for dynamic and static bit operations.
 */
class BitExecutor {
public:
    BitExecutor() = delete;

    /**
     * @brief Executes BTST, BCHG, BCLR, or BSET instruction.
     * @return Clock cycles consumed by the operation.
     */
    static int Execute(const DecodedInstruction& inst, M68k& cpu, Common::IBus* bus, Common::Word opcode) {
        (void)opcode;
        Common::Byte bitNum = 0;

        // Fetch bit index (from static extension word, or from Data Register)
        if (inst.srcMode == AddressingMode::Immediate) {
            bitNum = static_cast<Common::Byte>(bus->ReadWord(cpu.GetPC()) & 0xFF);
            cpu.SetPC(cpu.GetPC() + 2);
        } else {
            bitNum = static_cast<Common::Byte>(cpu.GetDRegister(inst.srcRegister) & 0xFF);
        }

        Common::Longword targetValue = M68kAddressing::ReadOperand(inst.destMode, inst.destRegister, inst.size, cpu, bus);
        Common::Word sr = cpu.GetSR();
        Common::Longword result = targetValue;

        if (inst.type == OpType::BTST) {
            M68kCoreInstructions::ExecuteBTST(targetValue, bitNum, inst.size, sr);
        } else if (inst.type == OpType::BCHG) {
            result = M68kCoreInstructions::ExecuteBCHG(targetValue, bitNum, inst.size, sr);
            M68kAddressing::WriteOperand(inst.destMode, inst.destRegister, inst.size, result, cpu, bus);
        } else if (inst.type == OpType::BCLR) {
            result = M68kCoreInstructions::ExecuteBCLR(targetValue, bitNum, inst.size, sr);
            M68kAddressing::WriteOperand(inst.destMode, inst.destRegister, inst.size, result, cpu, bus);
        } else if (inst.type == OpType::BSET) {
            result = M68kCoreInstructions::ExecuteBSET(targetValue, bitNum, inst.size, sr);
            M68kAddressing::WriteOperand(inst.destMode, inst.destRegister, inst.size, result, cpu, bus);
        }

        cpu.SetSR(sr);

        // Standard execution cycle measurements
        int cycles = (inst.destMode == AddressingMode::DataRegisterDirect) ? 6 : 8;
        if (inst.type == OpType::BTST) {
            cycles = (inst.destMode == AddressingMode::DataRegisterDirect) ? 6 : 4;
        }
        if (inst.srcMode == AddressingMode::Immediate) {
            cycles += 4; // Add static extension cycles
        }
        return cycles;
    }
};

} // namespace GenesisEmu::Core::Domain::M68k::Executors