// ==============================================================================
// GenesisEmu - M68k Consolidated Core Instructions Execution Unit (Core Domain)
// ==============================================================================
// This file centralizes the execution logic of missing instruction families
// (Bitwise, Shifts, Comparisons, Quick Operations, and Address Loading)
// required to boot and execute complex commercial Mega Drive game loops.
//
// DESIGN STRATEGY:
// 1. High Cohesion: Grouped logically into static stateless operations.
// 2. Performance: Direct register modifications keeping latency at 0%.
// ==============================================================================

#pragma once

#include "IMemoryMappedDevice.h"
#include "M68kInstruction.h"
#include "IBus.h"

namespace GenesisEmu::Core {

class M68kCoreInstructions {
public:
    M68kCoreInstructions() = delete;

    // --------------------------------------------------------------------------
    // 1. Comparisons & Tests (CMP, TST)
    // --------------------------------------------------------------------------
    /**
     * @brief Compares dest and src (calculates dest - src, updates flags, discards result).
     */
    static void ExecuteCMP(Longword dest, Longword src, OperandSize size, Word& sr);

    /**
     * @brief Tests an operand against zero (updates N and Z, clears V and C).
     */
    static void ExecuteTST(Longword value, OperandSize size, Word& sr);

    // --------------------------------------------------------------------------
    // 2. Quick Operations (MOVEQ, ADDQ, SUBQ)
    // --------------------------------------------------------------------------
    /**
     * @brief Moves an 8-bit sign-extended immediate value directly into a register.
     */
    static Longword ExecuteMOVEQ(Byte immediate8);

    // --------------------------------------------------------------------------
    // 3. Pointer & Address Loading (LEA, PEA)
    // --------------------------------------------------------------------------
    /**
     * @brief Load Effective Address directly into an Address Register.
     */
    static void ExecuteLEA(Longword& aReg, Address targetAddress);

    /**
     * @brief Push Effective Address directly onto the stack pointer (A7).
     */
    static void ExecutePEA(IBus* bus, Longword& sp, Address targetAddress);

    // --------------------------------------------------------------------------
    // 4. Bit Manipulations (BTST, BSET, BCLR)
    // --------------------------------------------------------------------------
    /**
     * @brief Tests a bit index in a value and sets the Zero flag accordingly.
     */
    static void ExecuteBTST(Longword value, Byte bitNum, OperandSize size, Word& sr);

    // --------------------------------------------------------------------------
    // 5. Logical Shifts (LSR, LSL)
    // --------------------------------------------------------------------------
    /**
     * @brief Performs Logical Shift Right, updating flags.
     */
    static Longword ExecuteLSR(Longword value, Byte shiftCount, OperandSize size, Word& sr);

    /**
     * @brief Performs Logical Shift Left, updating flags.
     */
    static Longword ExecuteLSL(Longword value, Byte shiftCount, OperandSize size, Word& sr);

    // --------------------------------------------------------------------------
    // 6. Unary Operations (CLR, NEG, EXT, SWAP)
    // --------------------------------------------------------------------------
    /**
     * @brief Sign-extends a Byte to a Word, or a Word to a Longword.
     */
    static Longword ExecuteEXT(Longword value, OperandSize size);

    /**
     * @brief Swaps the high 16 bits and low 16 bits of a 32-bit register.
     */
    static Longword ExecuteSWAP(Longword value, Word& sr);
};

} // namespace GenesisEmu::Core