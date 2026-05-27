// ==============================================================================
// GenesisEmu - M68k Shift and Rotate Operations Executor (Core Domain)
// ==============================================================================
// This file executes the Shift instructions: LSR, LSL, ASR, ASL, ROR, ROL, ROXR, ROXL, SWAP, EXT.
//
// SOLID Compliance:
// 1. Single Responsibility Principle (SRP):
//    It is solely responsible for implementing CPU bit shifting, swappings, and extensions.
// ==============================================================================

#pragma once

#include "../M68k.h"
#include "../M68kCoreInstructions.h"
#include "../M68kAddressing.h"

namespace GenesisEmu::Core::Domain::M68k::Executors {

/**
 * @class ShiftExecutor
 * @brief Stateless executor for standard, multi-precision, and direct register shifts.
 */
class ShiftExecutor {
public:
    ShiftExecutor() = delete;

    /**
     * @brief Executes LSR, LSL, ASR, ASL, ROR, ROL, ROXR, ROXL, SWAP, or EXT instruction.
     * @return Clock cycles consumed by the operation.
     */
    static int Execute(const DecodedInstruction& inst, M68k& cpu, Common::IBus* bus, Common::Word opcode) {
        (void)opcode;

        if (inst.type == OpType::SWAP) {
            Common::Longword val = cpu.GetDRegister(inst.destRegister);
            Common::Word sr = cpu.GetSR();
            Common::Longword result = M68kCoreInstructions::ExecuteSWAP(val, sr);
            cpu.SetSR(sr);
            cpu.SetDRegister(inst.destRegister, result);
            return 4;
        }

        if (inst.type == OpType::EXT) {
            Common::Longword val = cpu.GetDRegister(inst.destRegister);
            Common::Longword result = M68kCoreInstructions::ExecuteEXT(val, inst.size);
            
            Common::Word sr = cpu.GetSR();
            sr &= ~0x000F; // Clears V and C. Extend is unaffected.
            Common::Longword mask = (inst.size == OperandSize::WORD) ? 0xFFFF : 0xFFFFFFFF;
            Common::Longword msb  = (inst.size == OperandSize::WORD) ? 0x8000 : 0x80000000;
            if ((result & mask) == 0) sr |= 0x0004; 
            if ((result & msb) != 0) sr |= 0x0008; 
            cpu.SetSR(sr);
            
            cpu.SetDRegister(inst.destRegister, result);
            return 4;
        }

        // Shifts and Rotates (LSR, LSL, ASR, ASL, ROR, ROL, ROXR, ROXL)
        Common::Byte shiftCount = 0;
        if (inst.srcMode == AddressingMode::Immediate) {
            shiftCount = static_cast<Common::Byte>(inst.immediateData);
        } else {
            shiftCount = static_cast<Common::Byte>(cpu.GetDRegister(inst.srcRegister) & 63);
        }

        Common::Longword targetValue = M68kAddressing::ReadOperand(inst.destMode, inst.destRegister, inst.size, cpu, bus);
        Common::Longword result = 0;
        Common::Word sr = cpu.GetSR();
        
        if (inst.type == OpType::LSR)        result = M68kCoreInstructions::ExecuteLSR(targetValue, shiftCount, inst.size, sr);
        else if (inst.type == OpType::LSL)   result = M68kCoreInstructions::ExecuteLSL(targetValue, shiftCount, inst.size, sr);
        else if (inst.type == OpType::ASR)   result = M68kCoreInstructions::ExecuteASR(targetValue, shiftCount, inst.size, sr);
        else if (inst.type == OpType::ASL)   result = M68kCoreInstructions::ExecuteASL(targetValue, shiftCount, inst.size, sr);
        else if (inst.type == OpType::ROR)   result = M68kCoreInstructions::ExecuteROR(targetValue, shiftCount, inst.size, sr);
        else if (inst.type == OpType::ROL)   result = M68kCoreInstructions::ExecuteROL(targetValue, shiftCount, inst.size, sr);
        else if (inst.type == OpType::ROXR)  result = M68kCoreInstructions::ExecuteROXR(targetValue, shiftCount, inst.size, sr);
        else if (inst.type == OpType::ROXL)  result = M68kCoreInstructions::ExecuteROXL(targetValue, shiftCount, inst.size, sr);

        cpu.SetSR(sr);
        M68kAddressing::WriteOperand(inst.destMode, inst.destRegister, inst.size, result, cpu, bus);
        
        // Return memory shift latency immediately if destination targets RAM
        if (inst.destMode != AddressingMode::DataRegisterDirect) {
            return 16; 
        }
        
        int baseCycles = (inst.size == OperandSize::LONG) ? 8 : 6;
        if (inst.srcMode == AddressingMode::DataRegisterDirect) baseCycles += 2;
        return baseCycles + (2 * shiftCount);
    }
};

} // namespace GenesisEmu::Core::Domain::M68k::Executors