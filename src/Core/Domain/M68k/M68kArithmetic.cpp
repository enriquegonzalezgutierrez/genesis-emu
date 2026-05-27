// ==============================================================================
// GenesisEmu - M68k Arithmetic Execution Unit Implementation (Core Domain)
// ==============================================================================
// This file implements mathematical ALU logic and status flag evaluations.
//
// SOLID Compliance:
// 1. Single Responsibility Principle (SRP):
//    It is solely responsible for CPU mathematical transformations and CCR calculations.
// ==============================================================================

#include "M68kArithmetic.h"

namespace GenesisEmu::Core::Domain::M68k {

using namespace GenesisEmu::Core::Domain::Common;

// ------------------------------------------------------------------------------
// Mathematical Operations
// ------------------------------------------------------------------------------

Longword M68kArithmetic::ExecuteADD(Longword dest, Longword src, OperandSize size, Word& sr) {
    Longword mask = GetMask(size);
    Longword d = dest & mask;
    Longword s = src & mask;
    Longword result = (d + s) & mask;

    // Reset the lower CCR flag bits: X, N, Z, V, C (bits 4, 3, 2, 1, 0)
    sr &= ~0x001F;

    UpdateNZ(result, size, sr);

    // Calculate Carry (C) and Extend (X)
    // For addition, a carry is produced if the combined sum exceeds the operand size limits
    bool carry = (result < d);
    if (carry) {
        sr |= 0x0001; // Set Carry (C)
        sr |= 0x0010; // Set Extend (X)
    }

    // Calculate Overflow (V)
    // V is set if adding inputs with matching signs produces a result with a different sign
    bool dSign = IsNegative(d, size);
    bool sSign = IsNegative(s, size);
    bool rSign = IsNegative(result, size);
    if (dSign == sSign && dSign != rSign) {
        sr |= 0x0002; // Set Overflow (V)
    }

    return result;
}

Longword M68kArithmetic::ExecuteSUB(Longword dest, Longword src, OperandSize size, Word& sr) {
    Longword mask = GetMask(size);
    Longword d = dest & mask;
    Longword s = src & mask;
    Longword result = (d - s) & mask;

    // Reset the lower CCR flag bits: X, N, Z, V, C (bits 4, 3, 2, 1, 0)
    sr &= ~0x001F;

    UpdateNZ(result, size, sr);

    // Calculate Carry / Borrow (C) and Extend (X)
    // For subtraction, a borrow is produced if the subtrahend (src) is larger than the minuend (dest)
    bool borrow = (d < s);
    if (borrow) {
        sr |= 0x0001; // Set Carry (C)
        sr |= 0x0010; // Set Extend (X)
    }

    // Calculate Overflow (V)
    // V is set if subtracting differing signs produces a result with a sign opposite to dest
    bool dSign = IsNegative(d, size);
    bool sSign = IsNegative(s, size);
    bool rSign = IsNegative(result, size);
    if (dSign != sSign && dSign != rSign) {
        sr |= 0x0002; // Set Overflow (V)
    }

    return result;
}

// ------------------------------------------------------------------------------
// Logical Operations
// ------------------------------------------------------------------------------

Longword M68kArithmetic::ExecuteAND(Longword dest, Longword src, OperandSize size, Word& sr) {
    Longword mask = GetMask(size);
    Longword d = dest & mask;
    Longword s = src & mask;
    Longword result = (d & s) & mask;

    // Logical operations always clear Carry (C) and Overflow (V). Extend (X) is unaffected.
    sr &= ~0x0003;

    UpdateNZ(result, size, sr);

    return result;
}

Longword M68kArithmetic::ExecuteOR(Longword dest, Longword src, OperandSize size, Word& sr) {
    Longword mask = GetMask(size);
    Longword d = dest & mask;
    Longword s = src & mask;
    Longword result = (d | s) & mask;

    // Logical operations always clear Carry (C) and Overflow (V). Extend (X) is unaffected.
    sr &= ~0x0003;

    UpdateNZ(result, size, sr);

    return result;
}

Longword M68kArithmetic::ExecuteEOR(Longword dest, Longword src, OperandSize size, Word& sr) {
    Longword mask = GetMask(size);
    Longword d = dest & mask;
    Longword s = src & mask;
    Longword result = (d ^ s) & mask;

    // Logical operations always clear Carry (C) and Overflow (V). Extend (X) is unaffected.
    sr &= ~0x0003;

    UpdateNZ(result, size, sr);

    return result;
}

// ------------------------------------------------------------------------------
// Private Helper Engines
// ------------------------------------------------------------------------------

void M68kArithmetic::UpdateNZ(Longword result, OperandSize size, Word& sr) {
    // Zero (Z) Flag: Active if result is 0
    if (result == 0) {
        sr |= 0x0004;
    } else {
        sr &= ~0x0004;
    }

    // Negative (N) Flag: Active if the MSB of the current operand size is set
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

} // namespace GenesisEmu::Core::Domain::M68k