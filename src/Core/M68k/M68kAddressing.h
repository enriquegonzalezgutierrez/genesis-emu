// ==============================================================================
// GenesisEmu - M68k Addressing Mode Resolver (Core Domain)
// ==============================================================================
// This file abstracts the resolution, reading, and writing of operands 
// across all 12 Motorola 68000 addressing modes.
//
// DESIGN STRATEGY:
// 1. SRP: Completely isolates effective address (EA) calculations from opcode execution.
// 2. Anti-God File: Permanently prevents M68k.cpp from growing due to redundant 
//    addressing mode logic inside instruction switches.
// ==============================================================================

#pragma once

#include "IMemoryMappedDevice.h"
#include "M68kInstruction.h"
#include "IBus.h"

namespace GenesisEmu::Core {

// Forward declaration of M68k CPU to avoid circular dependencies
class M68k;

/**
 * @class M68kAddressing
 * @brief Utility to resolve effective addresses (EA) and execute memory/register 
 *        operand reads and writes.
 */
class M68kAddressing {
public:
    M68kAddressing() = delete;

    /**
     * @brief Resolves the final physical address for memory-indirect modes.
     */
    static Address ResolveAddress(AddressingMode mode, Byte reg, OperandSize size, M68k& cpu, IBus* bus);

    /**
     * @brief Reads an operand value (Byte, Word, or Long) according to its addressing mode.
     */
    static Longword ReadOperand(AddressingMode mode, Byte reg, OperandSize size, M68k& cpu, IBus* bus);

    /**
     * @brief Writes a value to the specified destination addressing mode.
     */
    static void WriteOperand(AddressingMode mode, Byte reg, OperandSize size, Longword value, M68k& cpu, IBus* bus);
};

} // namespace GenesisEmu::Core