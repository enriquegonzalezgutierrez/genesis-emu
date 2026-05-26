// ==============================================================================
// GenesisEmu - Motorola 68000 Instruction Decoder Header
// ==============================================================================
// This utility class provides static methods to parse raw 16-bit opcodes
// fetched from ROM into fully structured DecodedInstruction packets (SOLID).
// ==============================================================================

#pragma once

#include "M68kInstruction.h"

namespace GenesisEmu::Core {

class M68kDecoder {
public:
    // Delete constructor and destructor as this is a pure static utility class
    M68kDecoder() = delete;
    ~M68kDecoder() = delete;

    // --------------------------------------------------------------------------
    // Public Decoding Gateway
    // --------------------------------------------------------------------------
    // Decodes a raw 16-bit instruction word into a structured Value Object
    static DecodedInstruction Decode(Word opcode);

private:
    // Helper method to parse addressing modes for standard instructions
    static AddressingMode ParseAddressingMode(Byte modeBits, Byte regBits);
};

} // namespace GenesisEmu::Core