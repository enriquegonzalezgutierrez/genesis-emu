// ==============================================================================
// GenesisEmu - M68k Consolidated Core Instructions Implementation (Core Domain)
// ==============================================================================
// This file implements the bitwise logic, mathematical comparisons, shifts, 
// and address calculations needed to run high-performance emulated game loops.
// ==============================================================================

#include "M68kCoreInstructions.h"

namespace GenesisEmu::Core {

// --- Private Helpers ---

static Longword GetSizeMask(OperandSize size) {
    switch (size) {
        case OperandSize::BYTE: return 0x000000FF;
        case OperandSize::WORD: return 0x0000FFFF;
        case OperandSize::LONG: return 0xFFFFFFFF;
        default:                return 0x00000000;
    }
}

static bool IsSignBitSet(Longword value, OperandSize size) {
    switch (size) {
        case OperandSize::BYTE: return (value & 0x00000080) != 0;
        case OperandSize::WORD: return (value & 0x00008000) != 0;
        case OperandSize::LONG: return (value & 0x80000000) != 0;
        default:                return false;
    }
}

// ------------------------------------------------------------------------------
// 1. Comparisons & Tests (CMP, TST)
// ------------------------------------------------------------------------------

void M68kCoreInstructions::ExecuteCMP(Longword dest, Longword src, OperandSize size, Word& sr) {
    Longword mask = GetSizeMask(size);
    Longword d = dest & mask;
    Longword s = src & mask;
    Longword result = (d - s) & mask;

    // CMP clears V and C, then updates N and Z. It does NOT affect the X flag.
    sr &= ~0x000F;

    // Zero (Z) flag
    if (result == 0) sr |= 0x0004;

    // Negative (N) flag
    if (IsSignBitSet(result, size)) sr |= 0x0008;

    // Carry (C) flag (borrow generated)
    if (d < s) sr |= 0x0001;

    // Overflow (V) flag
    bool dSign = IsSignBitSet(d, size);
    bool sSign = IsSignBitSet(s, size);
    bool rSign = IsSignBitSet(result, size);
    if (dSign != sSign && dSign != rSign) {
        sr |= 0x0002;
    }
}

void M68kCoreInstructions::ExecuteTST(Longword value, OperandSize size, Word& sr) {
    Longword mask = GetSizeMask(size);
    Longword v = value & mask;

    // TST clears V and C, updates N and Z. X is unaffected.
    sr &= ~0x000F;

    if (v == 0) sr |= 0x0004;
    if (IsSignBitSet(v, size)) sr |= 0x0008;
}

// ------------------------------------------------------------------------------
// 2. Quick Operations (MOVEQ)
// ------------------------------------------------------------------------------

Longword M68kCoreInstructions::ExecuteMOVEQ(Byte immediate8) {
    // MOVEQ always sign-extends the 8-bit immediate value to a 32-bit Longword
    std::int8_t signedVal = static_cast<std::int8_t>(immediate8);
    std::int32_t signExtended = static_cast<std::int32_t>(signedVal);
    return static_cast<Longword>(signExtended);
}

// ------------------------------------------------------------------------------
// 3. Pointer & Address Loading (LEA, PEA)
// ------------------------------------------------------------------------------

void M68kCoreInstructions::ExecuteLEA(Longword& aReg, Address targetAddress) {
    aReg = targetAddress; // Loads the computed address directly into the register
}

void M68kCoreInstructions::ExecutePEA(IBus* bus, Longword& sp, Address targetAddress) {
    sp -= 4; // Decrement stack pointer (A7)
    bus->WriteLongword(sp, targetAddress); // Push the computed address onto the stack
}

// ------------------------------------------------------------------------------
// 4. Bit Manipulations (BTST)
// ------------------------------------------------------------------------------

void M68kCoreInstructions::ExecuteBTST(Longword value, Byte bitNum, OperandSize size, Word& sr) {
    // Bit number is modulo 32 for register targets, or 8 for memory targets
    Byte modulo = (size == OperandSize::BYTE) ? 8 : 32;
    Byte actualBit = bitNum % modulo;

    bool bitSet = (value & (1 << actualBit)) != 0;

    // BTST only affects the Z flag. If the tested bit is 0, Z is set (1), else Z is cleared (0).
    if (!bitSet) {
        sr |= 0x0004; // Set Z flag
    } else {
        sr &= ~0x0004; // Clear Z flag
    }
}

// ------------------------------------------------------------------------------
// 5. Logical Shifts (LSR, LSL)
// ------------------------------------------------------------------------------

Longword M68kCoreInstructions::ExecuteLSR(Longword value, Byte shiftCount, OperandSize size, Word& sr) {
    if (shiftCount == 0) {
        // Shift count of 0 does not modify the flags or values, but clears C and V
        sr &= ~0x0003;
        return value;
    }

    Longword mask = GetSizeMask(size);
    Longword val = value & mask;
    Longword result = val;
    bool lastOut = false;

    // LSR shifts bits to the right, shifting in 0s from the MSB
    for (int i = 0; i < shiftCount; ++i) {
        lastOut = (result & 1) != 0;
        result >>= 1;
    }
    result &= mask;

    // Preserve untouched bits outside operand size limits
    Longword outValue = (value & ~mask) | result;

    // Update Status Flags
    sr &= ~0x001F; // Clear lower flags (X, N, Z, V, C)

    if (result == 0) sr |= 0x0004; // Z
    if (IsSignBitSet(result, size)) sr |= 0x0008; // N
    
    if (lastOut) {
        sr |= 0x0001; // C-flag (last bit shifted out)
        sr |= 0x0010; // X-flag (same as carry)
    }

    return outValue;
}

Longword M68kCoreInstructions::ExecuteLSL(Longword value, Byte shiftCount, OperandSize size, Word& sr) {
    if (shiftCount == 0) {
        sr &= ~0x0003;
        return value;
    }

    Longword mask = GetSizeMask(size);
    Longword val = value & mask;
    Longword result = val;
    bool lastOut = false;
    Longword msbCheck = (size == OperandSize::BYTE) ? 0x80 : (size == OperandSize::WORD) ? 0x8000 : 0x80000000;

    // LSL shifts bits to the left, shifting in 0s from the LSB
    for (int i = 0; i < shiftCount; ++i) {
        lastOut = (result & msbCheck) != 0;
        result <<= 1;
    }
    result &= mask;

    Longword outValue = (value & ~mask) | result;

    sr &= ~0x001F;

    if (result == 0) sr |= 0x0004; 
    if (IsSignBitSet(result, size)) sr |= 0x0008; 
    
    if (lastOut) {
        sr |= 0x0001; // C
        sr |= 0x0010; // X
    }

    return outValue;
}

// ------------------------------------------------------------------------------
// 6. Unary Operations (EXT, SWAP)
// ------------------------------------------------------------------------------

Longword M68kCoreInstructions::ExecuteEXT(Longword value, OperandSize size) {
    if (size == OperandSize::WORD) {
        // Sign-extend low byte of register to a Word (preserving upper 16 bits of Dn)
        std::int8_t byteVal = static_cast<std::int8_t>(value & 0xFF);
        std::int16_t wordExtended = static_cast<std::int16_t>(byteVal);
        return (value & 0xFFFF0000) | (static_cast<Word>(wordExtended) & 0xFFFF);
    } 
    else if (size == OperandSize::LONG) {
        // Sign-extend low word of register to a Longword (replacing entire register)
        std::int16_t wordVal = static_cast<std::int16_t>(value & 0xFFFF);
        std::int32_t longExtended = static_cast<std::int32_t>(wordVal);
        return static_cast<Longword>(longExtended);
    }
    return value;
}

Longword M68kCoreInstructions::ExecuteSWAP(Longword value, Word& sr) {
    // Swaps high and low 16-bit words of the 32-bit register
    Longword highWord = (value >> 16) & 0xFFFF;
    Longword lowWord  = value & 0xFFFF;
    Longword result = (lowWord << 16) | highWord;

    // SWAP clears V and C, and updates N and Z based on the 32-bit result. X is unaffected.
    sr &= ~0x000F;
    if (result == 0) sr |= 0x0004;
    if ((result & 0x80000000) != 0) sr |= 0x0008;

    return result;
}

} // namespace GenesisEmu::Core