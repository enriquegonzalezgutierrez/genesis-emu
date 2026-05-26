// ==============================================================================
// GenesisEmu - M68k Consolidated Core Instructions Implementation (Core Domain)
// ==============================================================================
// This file implements the bitwise logic, mathematical comparisons, shifts, 
// and address calculations needed to run high-performance emulated game loops.
// Added exact ASR, ASL, ROR, and ROL execution logic.
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

    sr &= ~0x000F;

    if (result == 0) sr |= 0x0004;
    if (IsSignBitSet(result, size)) sr |= 0x0008;
    if (d < s) sr |= 0x0001;

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

    sr &= ~0x000F;

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
// 3. Pointer & Address Loading (LEA, PEA)
// ------------------------------------------------------------------------------

void M68kCoreInstructions::ExecuteLEA(Longword& aReg, Address targetAddress) {
    aReg = targetAddress; 
}

void M68kCoreInstructions::ExecutePEA(IBus* bus, Longword& sp, Address targetAddress) {
    sp -= 4; 
    bus->WriteLongword(sp, targetAddress); 
}

// ------------------------------------------------------------------------------
// 4. Bit Manipulations (BTST)
// ------------------------------------------------------------------------------

void M68kCoreInstructions::ExecuteBTST(Longword value, Byte bitNum, OperandSize size, Word& sr) {
    Byte modulo = (size == OperandSize::BYTE) ? 8 : 32;
    Byte actualBit = bitNum % modulo;

    bool bitSet = (value & (1 << actualBit)) != 0;

    if (!bitSet) {
        sr |= 0x0004; 
    } else {
        sr &= ~0x0004; 
    }
}

// ------------------------------------------------------------------------------
// 5. Shifts & Rotates (LSR, LSL, ASR, ASL, ROR, ROL)
// ------------------------------------------------------------------------------

Longword M68kCoreInstructions::ExecuteLSR(Longword value, Byte shiftCount, OperandSize size, Word& sr) {
    if (shiftCount == 0) {
        sr &= ~0x0003;
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

    sr &= ~0x001F; 

    if (result == 0) sr |= 0x0004; 
    if (IsSignBitSet(result, size)) sr |= 0x0008; 
    
    if (lastOut) {
        sr |= 0x0001; 
        sr |= 0x0010; 
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
        sr &= ~0x0003; // Clear C and V, X remains unaffected
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
        if (sign) {
            result |= msb; // Arithmetic replication of sign bit
        }
    }
    result &= mask;

    Longword outValue = (value & ~mask) | result;

    sr &= ~0x001F; // Clear lower CCR flags

    if (result == 0) sr |= 0x0004; // Z
    if (IsSignBitSet(result, size)) sr |= 0x0008; // N
    
    if (lastOut) {
        sr |= 0x0001; // C-flag
        sr |= 0x0010; // X-flag
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
        
        // V is set if sign bit changes state at any point during shift
        if (initialSign != finalSign) {
            overflow = true;
        }
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
    if (overflow) {
        sr |= 0x0002; // V-flag
    }

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
            if (lastOut) {
                result |= msb; // Wrap rotated bit to MSB
            }
        }
    } else {
        lastOut = (val & msb) != 0;
    }
    result &= mask;

    Longword outValue = (value & ~mask) | result;

    sr &= ~0x000F; // X is unaffected by ROR

    if (result == 0) sr |= 0x0004; 
    if (IsSignBitSet(result, size)) sr |= 0x0008; 
    if (lastOut) {
        sr |= 0x0001; // C-flag
    }

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
            if (lastOut) {
                result |= 1; // Wrap rotated bit to LSB
            }
        }
    } else {
        lastOut = (val & 1) != 0;
    }
    result &= mask;

    Longword outValue = (value & ~mask) | result;

    sr &= ~0x000F; // X is unaffected by ROL

    if (result == 0) sr |= 0x0004; 
    if (IsSignBitSet(result, size)) sr |= 0x0008; 
    if (lastOut) {
        sr |= 0x0001; // C-flag
    }

    return outValue;
}

// ------------------------------------------------------------------------------
// 6. Unary Operations (EXT, SWAP)
// ------------------------------------------------------------------------------

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

    sr &= ~0x000F;
    if (result == 0) sr |= 0x0004;
    if ((result & 0x80000000) != 0) sr |= 0x0008;

    return result;
}

} // namespace GenesisEmu::Core