// ==============================================================================
// GenesisEmu - M68k Arithmetic Execution Unit Implementation (Core Domain)
// ==============================================================================
// This file implements mathematical ALU logic and status flag evaluations.
//
// SOLID Compliance:
// 1. Single Responsibility Principle (SRP):
//    It is solely responsible for CPU mathematical transformations and CCR calculations.
// ==============================================================================

#include "M68kCoreInstructions.h"

namespace GenesisEmu::Core::Domain::M68k {

using namespace GenesisEmu::Core::Domain::Common;

// --- Local Helpers ---

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

    sr &= ~0x000F; // Reset lower CCR flags (N, Z, V, C)

    if (result == 0) sr |= 0x0004; // Set Zero (Z)
    if (IsSignBitSet(result, size)) sr |= 0x0008; // Set Negative (N)
    if (d < s) sr |= 0x0001; // Set Carry (C/Borrow)

    bool dSign = IsSignBitSet(d, size);
    bool sSign = IsSignBitSet(s, size);
    bool rSign = IsSignBitSet(result, size);
    if (dSign != sSign && dSign != rSign) {
        sr |= 0x0002; // Set Overflow (V)
    }
}

void M68kCoreInstructions::ExecuteTST(Longword value, OperandSize size, Word& sr) {
    Longword mask = GetSizeMask(size);
    Longword v = value & mask;

    sr &= ~0x000F; // Clears V and C. Extend (X) remains unaffected.

    if (v == 0) sr |= 0x0004;
    if (IsSignBitSet(v, size)) sr |= 0x0008;
}

// ------------------------------------------------------------------------------
// 2. Quick Operations (MOVEQ)
// ------------------------------------------------------------------------------

Longword M68kCoreInstructions::ExecuteMOVEQ(Byte immediate8) {
    std::int8_t signedVal = static_cast<std::int8_t>(immediate8);
    std::int32_t signExtended = static_cast<std::int32_t>(signedVal);
    return static_cast<Longword>(signExtended);
}

// ------------------------------------------------------------------------------
// 3. Address and Pointer Operations (LEA, PEA)
// ------------------------------------------------------------------------------

void M68kCoreInstructions::ExecuteLEA(Longword& aReg, Address targetAddress) {
    aReg = targetAddress; 
}

void M68kCoreInstructions::ExecutePEA(IBus* bus, Longword& sp, Address targetAddress) {
    sp -= 4; 
    bus->WriteLongword(sp, targetAddress); 
}

// ------------------------------------------------------------------------------
// 4. Bit Manipulations (BTST, BCHG, BCLR, BSET)
// ------------------------------------------------------------------------------

void M68kCoreInstructions::ExecuteBTST(Longword value, Byte bitNum, OperandSize size, Word& sr) {
    Byte modulo = (size == OperandSize::BYTE) ? 8 : 32;
    Byte actualBit = bitNum % modulo;

    bool bitSet = (value & (1 << actualBit)) != 0;

    if (!bitSet) {
        sr |= 0x0004; // Set Zero (Z) if the bit was 0
    } else {
        sr &= ~0x0004; // Clear Zero if the bit was 1
    }
}

Longword M68kCoreInstructions::ExecuteBCHG(Longword value, Byte bitNum, OperandSize size, Word& sr) {
    Byte modulo = (size == OperandSize::BYTE) ? 8 : 32;
    Byte actualBit = bitNum % modulo;

    // Test the bit (sets Z flag based on original bit value)
    bool bitSet = (value & (1 << actualBit)) != 0;
    if (!bitSet) {
        sr |= 0x0004;
    } else {
        sr &= ~0x0004;
    }

    // Invert/Toggle the bit
    return value ^ (1 << actualBit);
}

Longword M68kCoreInstructions::ExecuteBCLR(Longword value, Byte bitNum, OperandSize size, Word& sr) {
    Byte modulo = (size == OperandSize::BYTE) ? 8 : 32;
    Byte actualBit = bitNum % modulo;

    bool bitSet = (value & (1 << actualBit)) != 0;
    if (!bitSet) {
        sr |= 0x0004;
    } else {
        sr &= ~0x0004;
    }

    // Clear the bit (set to 0)
    return value & ~(1 << actualBit);
}

Longword M68kCoreInstructions::ExecuteBSET(Longword value, Byte bitNum, OperandSize size, Word& sr) {
    Byte modulo = (size == OperandSize::BYTE) ? 8 : 32;
    Byte actualBit = bitNum % modulo;

    bool bitSet = (value & (1 << actualBit)) != 0;
    if (!bitSet) {
        sr |= 0x0004;
    } else {
        sr &= ~0x0004;
    }

    // Set the bit (set to 1)
    return value | (1 << actualBit);
}

// ------------------------------------------------------------------------------
// 5. Shifts and Rotates (LSR, LSL, ASR, ASL, ROR, ROL, ROXR, ROXL)
// ------------------------------------------------------------------------------

Longword M68kCoreInstructions::ExecuteLSR(Longword value, Byte shiftCount, OperandSize size, Word& sr) {
    if (shiftCount == 0) {
        sr &= ~0x0003; // Clears V and C
        return value;
    }

    Longword mask = GetSizeMask(size);
    Longword val = value & mask;
    Longword result = val;
    bool lastOut = false;

    for (int i = 0; i < shiftCount; ++i) {
        lastOut = (result & 1) != 0;
        result >>= 1;
    }
    result &= mask;

    Longword outValue = (value & ~mask) | result;
    sr &= ~0x001F; // Clear lower CCR status bits

    if (result == 0) sr |= 0x0004; 
    if (IsSignBitSet(result, size)) sr |= 0x0008; 
    
    if (lastOut) {
        sr |= 0x0001; // Set Carry (C)
        sr |= 0x0010; // Set Extend (X)
    }

    return outValue;
}

Longword M68kCoreInstructions::ExecuteLSL(Longword value, Byte shiftCount, OperandSize size, Word& sr) {
    if (shiftCount == 0) {
        sr &= ~0x0003; // Clears V and C
        return value;
    }

    Longword mask = GetSizeMask(size);
    Longword val = value & mask;
    Longword result = val;
    bool lastOut = false;
    Longword msbCheck = (size == OperandSize::BYTE) ? 0x80 : (size == OperandSize::WORD) ? 0x8000 : 0x80000000;

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
        sr |= 0x0001; 
        sr |= 0x0010; 
    }

    return outValue;
}

Longword M68kCoreInstructions::ExecuteASR(Longword value, Byte shiftCount, OperandSize size, Word& sr) {
    if (shiftCount == 0) {
        sr &= ~0x0003;
        return value;
    }

    Longword mask = GetSizeMask(size);
    Longword val = value & mask;
    Longword result = val;
    bool lastOut = false;
    Longword msb = (size == OperandSize::BYTE) ? 0x80 : (size == OperandSize::WORD) ? 0x8000 : 0x80000000;

    for (int i = 0; i < shiftCount; ++i) {
        lastOut = (result & 1) != 0;
        bool sign = (result & msb) != 0;
        result >>= 1;
        if (sign) result |= msb; // Preserve the sign bit (Arithmetic shift)
    }
    result &= mask;

    Longword outValue = (value & ~mask) | result;
    sr &= ~0x001F;

    if (result == 0) sr |= 0x0004;
    if (IsSignBitSet(result, size)) sr |= 0x0008;
    
    if (lastOut) {
        sr |= 0x0001;
        sr |= 0x0010;
    }

    return outValue;
}

Longword M68kCoreInstructions::ExecuteASL(Longword value, Byte shiftCount, OperandSize size, Word& sr) {
    if (shiftCount == 0) {
        sr &= ~0x0003; 
        return value;
    }

    Longword mask = GetSizeMask(size);
    Longword val = value & mask;
    Longword result = val;
    bool lastOut = false;
    bool overflow = false;
    Longword msb = (size == OperandSize::BYTE) ? 0x80 : (size == OperandSize::WORD) ? 0x8000 : 0x80000000;

    for (int i = 0; i < shiftCount; ++i) {
        bool initialSign = (result & msb) != 0;
        lastOut = (result & msb) != 0;
        result <<= 1;
        bool finalSign = (result & msb) != 0;
        if (initialSign != finalSign) overflow = true;
    }
    result &= mask;

    Longword outValue = (value & ~mask) | result;
    sr &= ~0x001F;

    if (result == 0) sr |= 0x0004; 
    if (IsSignBitSet(result, size)) sr |= 0x0008; 
    if (lastOut) { sr |= 0x0001; sr |= 0x0010; }
    if (overflow) sr |= 0x0002;

    return outValue;
}

Longword M68kCoreInstructions::ExecuteROR(Longword value, Byte shiftCount, OperandSize size, Word& sr) {
    if (shiftCount == 0) {
        sr &= ~0x0003;
        return value;
    }

    Longword mask = GetSizeMask(size);
    Longword val = value & mask;
    Longword result = val;
    bool lastOut = false;
    int bitSize = (size == OperandSize::BYTE) ? 8 : (size == OperandSize::WORD) ? 16 : 32;
    Longword msb = (size == OperandSize::BYTE) ? 0x80 : (size == OperandSize::WORD) ? 0x8000 : 0x80000000;

    Byte actualShift = shiftCount % bitSize;
    if (actualShift > 0) {
        for (int i = 0; i < actualShift; ++i) {
            lastOut = (result & 1) != 0;
            result >>= 1;
            if (lastOut) result |= msb;
        }
    } else {
        lastOut = (val & msb) != 0;
    }
    result &= mask;

    Longword outValue = (value & ~mask) | result;
    sr &= ~0x000F;
    if (result == 0) sr |= 0x0004; 
    if (IsSignBitSet(result, size)) sr |= 0x0008; 
    if (lastOut) sr |= 0x0001;

    return outValue;
}

Longword M68kCoreInstructions::ExecuteROL(Longword value, Byte shiftCount, OperandSize size, Word& sr) {
    if (shiftCount == 0) {
        sr &= ~0x0003;
        return value;
    }

    Longword mask = GetSizeMask(size);
    Longword val = value & mask;
    Longword result = val;
    bool lastOut = false;
    int bitSize = (size == OperandSize::BYTE) ? 8 : (size == OperandSize::WORD) ? 16 : 32;
    Longword msb = (size == OperandSize::BYTE) ? 0x80 : (size == OperandSize::WORD) ? 0x8000 : 0x80000000;

    Byte actualShift = shiftCount % bitSize;
    if (actualShift > 0) {
        for (int i = 0; i < actualShift; ++i) {
            lastOut = (result & msb) != 0;
            result <<= 1;
            if (lastOut) result |= 1;
        }
    } else {
        lastOut = (val & 1) != 0;
    }
    result &= mask;

    Longword outValue = (value & ~mask) | result;
    sr &= ~0x000F;
    if (result == 0) sr |= 0x0004; 
    if (IsSignBitSet(result, size)) sr |= 0x0008; 
    if (lastOut) sr |= 0x0001;

    return outValue;
}

Longword M68kCoreInstructions::ExecuteROXR(Longword value, Byte shiftCount, OperandSize size, Word& sr) {
    Longword mask = GetSizeMask(size);
    Longword val = value & mask;

    if (shiftCount == 0) {
        sr &= ~0x000F; // Clears N, Z, V, C. X remains unaffected.
        if (val == 0) sr |= 0x0004;
        if (IsSignBitSet(val, size)) sr |= 0x0008;
        return value;
    }

    Longword result = val;
    bool xFlag = (sr & 0x0010) != 0;
    Longword msb = (size == OperandSize::BYTE) ? 0x80 : (size == OperandSize::WORD) ? 0x8000 : 0x80000000;

    for (int i = 0; i < shiftCount; ++i) {
        bool bitOut = (result & 1) != 0;
        result >>= 1;
        if (xFlag) result |= msb;
        xFlag = bitOut;
    }
    result &= mask;

    Longword outValue = (value & ~mask) | result;
    
    sr &= ~0x001F; // Clears lower CCR flags
    if (result == 0) sr |= 0x0004;
    if (IsSignBitSet(result, size)) sr |= 0x0008;
    if (xFlag) {
        sr |= 0x0010; // New X
        sr |= 0x0001; // New C
    }

    return outValue;
}

Longword M68kCoreInstructions::ExecuteROXL(Longword value, Byte shiftCount, OperandSize size, Word& sr) {
    Longword mask = GetSizeMask(size);
    Longword val = value & mask;

    if (shiftCount == 0) {
        sr &= ~0x000F; // Clears N, Z, V, C. X remains unaffected.
        if (val == 0) sr |= 0x0004;
        if (IsSignBitSet(val, size)) sr |= 0x0008;
        return value;
    }

    Longword result = val;
    bool xFlag = (sr & 0x0010) != 0;
    Longword msb = (size == OperandSize::BYTE) ? 0x80 : (size == OperandSize::WORD) ? 0x8000 : 0x80000000;

    for (int i = 0; i < shiftCount; ++i) {
        bool bitOut = (result & msb) != 0;
        result <<= 1;
        if (xFlag) result |= 1;
        xFlag = bitOut;
    }
    result &= mask;

    Longword outValue = (value & ~mask) | result;
    
    sr &= ~0x001F; // Clears lower CCR flags
    if (result == 0) sr |= 0x0004;
    if (IsSignBitSet(result, size)) sr |= 0x0008;
    if (xFlag) {
        sr |= 0x0010; // New X
        sr |= 0x0001; // New C
    }

    return outValue;
}

// ------------------------------------------------------------------------------
// 6. Unary Operations (NOT, EXT, SWAP, NEG, NEGX)
// ------------------------------------------------------------------------------

Longword M68kCoreInstructions::ExecuteNOT(Longword value, OperandSize size, Word& sr) {
    Longword mask = GetSizeMask(size);
    Longword result = (~value) & mask;

    sr &= ~0x000F; // V and C are always cleared

    if (result == 0) sr |= 0x0004;
    if (IsSignBitSet(result, size)) sr |= 0x0008;

    return (value & ~mask) | result;
}

Longword M68kCoreInstructions::ExecuteEXT(Longword value, OperandSize size) {
    if (size == OperandSize::WORD) {
        std::int8_t byteVal = static_cast<std::int8_t>(value & 0xFF);
        std::int16_t wordExtended = static_cast<std::int16_t>(byteVal);
        return (value & 0xFFFF0000) | (static_cast<Word>(wordExtended) & 0xFFFF);
    } 
    else if (size == OperandSize::LONG) {
        std::int16_t wordVal = static_cast<std::int16_t>(value & 0xFFFF);
        std::int32_t longExtended = static_cast<std::int32_t>(wordVal);
        return static_cast<Longword>(longExtended);
    }
    return value;
}

Longword M68kCoreInstructions::ExecuteSWAP(Longword value, Word& sr) {
    Longword highWord = (value >> 16) & 0xFFFF;
    Longword lowWord  = value & 0xFFFF;
    Longword result = (lowWord << 16) | highWord;

    sr &= ~0x000F; // Clears V and C. Extend is unaffected.
    if (result == 0) sr |= 0x0004;
    if ((result & 0x80000000) != 0) sr |= 0x0008;

    return result;
}

Longword M68kCoreInstructions::ExecuteNEG(Longword value, OperandSize size, Word& sr) {
    Longword mask = GetSizeMask(size);
    Longword val = value & mask;
    Longword result = (0 - val) & mask;

    sr &= ~0x001F; // Clear X, N, Z, V, C flags

    if (result == 0) {
        sr |= 0x0004; // Set Z
    }
    if (IsSignBitSet(result, size)) {
        sr |= 0x0008; // Set N
    }

    // V: set on overflow (negating the maximum negative value of this size)
    Longword maxNegative = (size == OperandSize::BYTE) ? 0x80 : (size == OperandSize::WORD) ? 0x8000 : 0x80000000;
    if (val == maxNegative) {
        sr |= 0x0002; // Set V
    }

    // C and X: set if the original operand is non-zero
    if (val != 0) {
        sr |= 0x0001; // Set C
        sr |= 0x0010; // Set X
    }

    return result;
}

Longword M68kCoreInstructions::ExecuteNEGX(Longword value, OperandSize size, Word& sr) {
    Longword mask = GetSizeMask(size);
    Longword val = value & mask;
    Longword ext = (sr & 0x0010) ? 1 : 0; // Retrieve X flag
    Longword result = (0 - val - ext) & mask;

    bool originalZ = (sr & 0x0004) != 0;
    sr &= ~0x001F; // Clear X, N, Z, V, C flags

    // N flag
    if (IsSignBitSet(result, size)) {
        sr |= 0x0008;
    }

    // Z flag: split-logic for NEGX/SUBX (remains unchanged if result is zero, cleared if non-zero)
    if (result == 0) {
        if (originalZ) sr |= 0x0004;
    }

    // V flag: overflow occurs on signed bounds subtraction.
    // Equivalent to B_sign (src) is negative and R_sign (result) is negative.
    if (IsSignBitSet(val, size) && IsSignBitSet(result, size)) {
        sr |= 0x0002;
    }

    // C and X flags: set if a borrow is required (val + ext > 0)
    if ((val + ext) > 0) {
        sr |= 0x0001; // Set C
        sr |= 0x0010; // Set X
    }

    return result;
}

} // namespace GenesisEmu::Core::Domain::M68k