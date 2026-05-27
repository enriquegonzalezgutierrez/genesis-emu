// ==============================================================================
// GenesisEmu - M68k Instruction Decoder Header (Core Domain)
// ==============================================================================
// This file declares the stateless M68kDecoder class.
//
// SOLID Compliance:
// 1. Single Responsibility Principle (SRP):
//    It is solely responsible for parsing raw binary words into structured
//    DecodedInstruction packets. It does not access registers or execute commands.
// ==============================================================================

#pragma once

#include "M68kInstruction.h"

namespace GenesisEmu::Core::Domain::M68k {

/**
 * @class M68kDecoder
 * @brief Stateless utility class dedicated to opcode parsing.
 */
class M68kDecoder {
public:
    M68kDecoder() = delete; // Enforce pure static class design
    ~M68kDecoder() = delete;

    /**
     * @brief Parses a raw 16-bit instruction word.
     * @param opcode The binary word fetched from execution space.
     * @return DecodedInstruction value packet containing operation details.
     */
    static DecodedInstruction Decode(Common::Word opcode);

private:
    /**
     * @brief Translates 3-bit mode and register bits into physical addressing modes.
     */
    static AddressingMode ParseAddressingMode(Common::Byte modeBits, Common::Byte regBits);
};

} // namespace GenesisEmu::Core::Domain::M68k