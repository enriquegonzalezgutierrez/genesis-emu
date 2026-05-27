// ==============================================================================
// GenesisEmu - M68k Arithmetic Execution Unit Header (Core Domain)
// ==============================================================================
// This file declares the M68kArithmetic class. It executes mathematical and logical
// ALU functions, modifying status flags according to 68000 micro-architectural rules.
//
// SOLID Compliance:
// 1. Single Responsibility Principle (SRP):
//    It is strictly responsible for executing bitwise/mathematical formulas and
//    calculating CCR flags. It does not access CPU registers or system buses.
// ==============================================================================

#pragma once

#include "../Common/IMemoryMappedDevice.h"
#include "M68kInstruction.h"

namespace GenesisEmu::Core::Domain::M68k {

/**
 * @class M68kArithmetic
 * @brief Stateless arithmetic logic unit (ALU) simulator for the M68k.
 */
class M68kArithmetic {
public:
    M68kArithmetic() = delete; // Enforce pure static ALU design
    ~M68kArithmetic() = delete;

    /**
     * @brief Performs addition and updates CCR flags (X, N, Z, V, C).
     * @param dest The destination operand value.
     * @param src The source operand value to add.
     * @param size Operand data size.
     * @param sr Reference to the Status Register (SR) to update flags.
     * @return The 32-bit masked result of the addition.
     */
    static Common::Longword ExecuteADD(Common::Longword dest, Common::Longword src, OperandSize size, Common::Word& sr);

    /**
     * @brief Performs subtraction and updates CCR flags (X, N, Z, V, C).
     * @param dest The destination operand value.
     * @param src The source operand value to subtract.
     * @param size Operand data size.
     * @param sr Reference to the Status Register (SR) to update flags.
     * @return The 32-bit masked result of the subtraction.
     */
    static Common::Longword ExecuteSUB(Common::Longword dest, Common::Longword src, OperandSize size, Common::Word& sr);

    /**
     * @brief Performs bitwise AND, updating CCR flags (N, Z).
     * @note Clears V and C. Extend (X) remains unaffected.
     */
    static Common::Longword ExecuteAND(Common::Longword dest, Common::Longword src, OperandSize size, Common::Word& sr);

    /**
     * @brief Performs bitwise OR, updating CCR flags (N, Z).
     * @note Clears V and C. Extend (X) remains unaffected.
     */
    static Common::Longword ExecuteOR(Common::Longword dest, Common::Longword src, OperandSize size, Common::Word& sr);

    /**
     * @brief Performs bitwise exclusive OR (XOR), updating CCR flags (N, Z).
     * @note Clears V and C. Extend (X) remains unaffected.
     */
    static Common::Longword ExecuteEOR(Common::Longword dest, Common::Longword src, OperandSize size, Common::Word& sr);

private:
    /**
     * @brief Evaluates and modifies the Negative (N) and Zero (Z) status flags.
     */
    static void UpdateNZ(Common::Longword result, OperandSize size, Common::Word& sr);

    /**
     * @brief Helper to generate a bitmask corresponding to operand sizes.
     */
    static Common::Longword GetMask(OperandSize size);

    /**
     * @brief Detects if the sign bit of an operand is active (signed negative).
     */
    static bool IsNegative(Common::Longword value, OperandSize size);
};

} // namespace GenesisEmu::Core::Domain::M68k