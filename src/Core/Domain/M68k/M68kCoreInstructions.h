// ==============================================================================
// GenesisEmu - M68k Core Instructions Header (Core Domain)
// ==============================================================================
// This file declares the static M68kCoreInstructions class. It isolates specialized
// instruction executions, updating register files and status registers (SR).
// ==============================================================================

#pragma once

#include "../Common/IMemoryMappedDevice.h"
#include "../Common/IBus.h"
#include "M68kInstruction.h"

namespace GenesisEmu::Core::Domain::M68k {

// Forward declaration of M68k processor class to prevent circular inclusions
class M68k;

/**
 * @class M68kCoreInstructions
 * @brief Stateless execution unit for shifts, logical tests, and address loadings.
 */
class M68kCoreInstructions {
public:
    M68kCoreInstructions() = delete; // Enforce pure static design
    ~M68kCoreInstructions() = delete;

    // --------------------------------------------------------------------------
    // 1. Comparisons & Tests (CMP, TST)
    // --------------------------------------------------------------------------
    static void ExecuteCMP(Common::Longword dest, Common::Longword src, OperandSize size, Common::Word& sr);
    static void ExecuteTST(Common::Longword value, OperandSize size, Common::Word& sr);

    // --------------------------------------------------------------------------
    // 2. Quick Operations (MOVEQ)
    // --------------------------------------------------------------------------
    static Common::Longword ExecuteMOVEQ(Common::Byte immediate8);

    // --------------------------------------------------------------------------
    // 3. Address and Pointer Operations (LEA, PEA)
    // --------------------------------------------------------------------------
    static void ExecuteLEA(Common::Longword& aReg, Common::Address targetAddress);
    static void ExecutePEA(Common::IBus* bus, Common::Longword& sp, Common::Address targetAddress);

    // --------------------------------------------------------------------------
    // 4. Bit Manipulations (BTST, BCHG, BCLR, BSET)
    // --------------------------------------------------------------------------
    static void ExecuteBTST(Common::Longword value, Common::Byte bitNum, OperandSize size, Common::Word& sr);

    /**
     * @brief Tests a bit index (setting Zero flag) and then inverts (toggles) the bit.
     * @return The modified 32-bit value.
     */
    static Common::Longword ExecuteBCHG(Common::Longword value, Common::Byte bitNum, OperandSize size, Common::Word& sr);

    /**
     * @brief Tests a bit index (setting Zero flag) and then clears (resets to 0) the bit.
     * @return The modified 32-bit value.
     */
    static Common::Longword ExecuteBCLR(Common::Longword value, Common::Byte bitNum, OperandSize size, Common::Word& sr);

    /**
     * @brief Tests a bit index (setting Zero flag) and then sets (resets to 1) the bit.
     * @return The modified 32-bit value.
     */
    static Common::Longword ExecuteBSET(Common::Longword value, Common::Byte bitNum, OperandSize size, Common::Word& sr);

    // --------------------------------------------------------------------------
    // 5. Shifts and Rotates (LSR, LSL, ASR, ASL, ROR, ROL, ROXR, ROXL)
    // --------------------------------------------------------------------------
    static Common::Longword ExecuteLSR(Common::Longword value, Common::Byte shiftCount, OperandSize size, Common::Word& sr);
    static Common::Longword ExecuteLSL(Common::Longword value, Common::Byte shiftCount, OperandSize size, Common::Word& sr);
    static Common::Longword ExecuteASR(Common::Longword value, Common::Byte shiftCount, OperandSize size, Common::Word& sr);
    static Common::Longword ExecuteASL(Common::Longword value, Common::Byte shiftCount, OperandSize size, Common::Word& sr);
    static Common::Longword ExecuteROR(Common::Longword value, Common::Byte shiftCount, OperandSize size, Common::Word& sr);
    static Common::Longword ExecuteROL(Common::Longword value, Common::Byte shiftCount, OperandSize size, Common::Word& sr);
    static Common::Longword ExecuteROXR(Common::Longword value, Common::Byte shiftCount, OperandSize size, Common::Word& sr);
    static Common::Longword ExecuteROXL(Common::Longword value, Common::Byte shiftCount, OperandSize size, Common::Word& sr);

    // --------------------------------------------------------------------------
    // 6. Unary Operations (NOT, EXT, SWAP)
    // --------------------------------------------------------------------------
    static Common::Longword ExecuteNOT(Common::Longword value, OperandSize size, Common::Word& sr);
    static Common::Longword ExecuteEXT(Common::Longword value, OperandSize size);
    static Common::Longword ExecuteSWAP(Common::Longword value, Common::Word& sr);
};

} // namespace GenesisEmu::Core::Domain::M68k