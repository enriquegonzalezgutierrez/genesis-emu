// ==============================================================================
// GenesisEmu - M68k Arithmetic Execution Unit Implementation (Core Domain)
// ==============================================================================
// This file implements the bitwise math algorithms and status flag updates 
// conforming to Motorola 68000 hardware specifications.
// ==============================================================================

#include "M68kArithmetic.h"

namespace GenesisEmu::Core {

// --- Public Execution APIs ---

Longword M68kArithmetic::ExecuteADD(Longword dest, Longword src, OperandSize size, Word& sr) {
    Longword mask = GetMask(size);
    Longword d = dest & mask;
    Longword s = src & mask;
    Longword result = (d + s) & mask;

    // Clear lower CCR flags: X, N, Z, V, C (bits 4, 3, 2, 1, 0)
    sr &= ~0x001F;

    // Update Negative (N) and Zero (Z) flags
    UpdateNZ(result, size, sr);

    // Calculate Carry (C) and Extend (X)
    // For addition, a carry is generated if the result is smaller than either operand
    bool carry = (result < d);
    if (carry) {
        sr |= 0x0001; // C-flag
        sr |= 0x0010; // X-flag
    }

    // Calculate Overflow (V)
    // V is set if adding two values of the same sign yields a result with a different sign
    bool dSign = IsNegative(d, size);
    bool sSign = IsNegative(s, size);
    bool rSign = IsNegative(result, size);
    if (dSign == sSign && dSign != rSign) {
        sr |= 0x0002; // V-flag
    }

    return result;
}

Longword M68kArithmetic::ExecuteSUB(Longword dest, Longword src, OperandSize size, Word& sr) {
    Longword mask = GetMask(size);
    Longword d = dest & mask;
    Longword s = src & mask;
    Longword result = (d - s) & mask;

    // Clear lower CCR flags: X, N, Z, V, C (bits 4, 3, 2, 1, 0)
    sr &= ~0x001F;

    // Update Negative (N) and Zero (Z) flags
    UpdateNZ(result, size, sr);

    // Calculate Carry / Borrow (C) and Extend (X)
    // For subtraction, a borrow is generated if the subtrahend (src) is greater than the minuend (dest)
    bool borrow = (d < s);
    if (borrow) {
        sr |= 0x0001; // C-flag
        sr |= 0x0010; // X-flag
    }

    // Calculate Overflow (V)
    // V is set if subtracting different sign values yields a result with a different sign than dest
    bool dSign = IsNegative(d, size);
    bool sSign = IsNegative(s, size);
    bool rSign = IsNegative(result, size);
    if (dSign != sSign && dSign != rSign) {
        sr |= 0x0002; // V-flag
    }

    return result;
}

Longword M68kArithmetic::ExecuteAND(Longword dest, Longword src, OperandSize size, Word& sr) {
    Longword mask = GetMask(size);
    Longword d = dest & mask;
    Longword s = src & mask;
    Longword result = (d & s) & mask;

    // Logical operations always clear Carry (C) and Overflow (V). Extend (X) is unaffected.
    sr &= ~0x0003;

    // Update Negative (N) and Zero (Z) flags
    UpdateNZ(result, size, sr);

    return result;
}

// --- Private Helpers ---

void M68kArithmetic::UpdateNZ(Longword result, OperandSize size, Word& sr) {
    // Zero (Z) Flag: Set if the masked result is 0
    if (result == 0) {
        sr |= 0x0004;
    } else {
        sr &= ~0x0004;
    }

    // Negative (N) Flag: Set if the MSB of the operand size is 1
    if (IsNegative(result, size)) {
        sr |= 0x0008;
    } else {
        sr &= ~0x0008;
    }
}

Longword M68kArithmetic::GetMask(OperandSize size) {
    switch (size) {
        case OperandSize::BYTE: return 0x000000FF;
        case OperandSize::WORD: return 0x0000FFFF;
        case OperandSize::LONG: return 0xFFFFFFFF;
        default:                return 0x00000000;
    }
}

bool M68kArithmetic::IsNegative(Longword value, OperandSize size) {
    switch (size) {
        case OperandSize::BYTE: return (value & 0x00000080) != 0;
        case OperandSize::WORD: return (value & 0x00008000) != 0;
        case OperandSize::LONG: return (value & 0x80000000) != 0;
        default:                return false;
    }
}

} // namespace GenesisEmu::Core