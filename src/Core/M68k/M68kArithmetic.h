// ==============================================================================
// GenesisEmu - M68k Arithmetic Execution Unit (Core Domain)
// ==============================================================================
// This component handles all mathematical operations (ADD, SUB, NEG, etc.)
// for the Motorola 68000. 
//
// DESIGN STRATEGY:
// 1. SRP (Single Responsibility): It only knows how to perform math and 
//    calculate CCR flags. It does not know about the Bus or Instruction Fetching.
// 2. Anti-God File: Prevents M68k.cpp from becoming a monolithic switch-case.
// 3. Deterministic: Pure logic that is highly testable in isolation.
// ==============================================================================

#pragma once

#include "IMemoryMappedDevice.h"
#include "M68kInstruction.h"

namespace GenesisEmu::Core {

/**
 * @class M68kArithmetic
 * @brief Static execution unit for M68k arithmetic operations.
 */
class M68kArithmetic {
public:
    // Delete constructor to enforce static utility usage
    M68kArithmetic() = delete;

    /**
     * @brief Performs addition and updates CCR flags (X, N, Z, V, C).
     * @param dest The current destination value.
     * @param src The value to add.
     * @param size The operand size (Byte, Word, Long).
     * @param sr Reference to the Status Register to update flags.
     * @return The resulting value.
     */
    static Longword ExecuteADD(Longword dest, Longword src, OperandSize size, Word& sr);

    /**
     * @brief Performs subtraction and updates CCR flags (X, N, Z, V, C).
     * @param dest The current destination value.
     * @param src The value to subtract.
     * @param size The operand size.
     * @param sr Reference to the Status Register to update flags.
     * @return The resulting value.
     */
    static Longword ExecuteSUB(Longword dest, Longword src, OperandSize size, Word& sr);

    /**
     * @brief Performs logical AND and updates CCR flags (N, Z, V, C).
     * @note V and C are always cleared. X is not affected.
     */
    static Longword ExecuteAND(Longword dest, Longword src, OperandSize size, Word& sr);

    /**
     * @brief Performs logical OR and updates CCR flags (N, Z, V, C).
     * @note V and C are always cleared. X is not affected.
     */
    static Longword ExecuteOR(Longword dest, Longword src, OperandSize size, Word& sr);

    /**
     * @brief Performs logical EOR (XOR) and updates CCR flags (N, Z, V, C).
     * @note V and C are always cleared. X is not affected.
     */
    static Longword ExecuteEOR(Longword dest, Longword src, OperandSize size, Word& sr);

private:
    /**
     * @brief Helper to update Negative and Zero flags based on result and size.
     */
    static void UpdateNZ(Longword result, OperandSize size, Word& sr);

    /**
     * @brief Helper to mask values based on M68k operand size.
     */
    static Longword GetMask(OperandSize size);

    /**
     * @brief Helper to check the sign bit (MSB) based on size.
     */
    static bool IsNegative(Longword value, OperandSize size);
};

} // namespace GenesisEmu::Core