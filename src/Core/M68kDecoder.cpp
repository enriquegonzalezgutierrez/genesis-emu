// ==============================================================================
// GenesisEmu - Motorola 68000 Instruction Decoder Implementation (Updated)
// ==============================================================================
// This file implements the bit-mask parsing logic for standard M68k opcodes.
// Note: Support for AddressRegisterPostincrement mode (0x3) has been added.
// ==============================================================================

#include "M68kDecoder.h"

namespace GenesisEmu::Core {

// ------------------------------------------------------------------------------
// Central Opcode Decoder
// ------------------------------------------------------------------------------
DecodedInstruction M68kDecoder::Decode(Word opcode) {
    DecodedInstruction inst;
    inst.type = OpType::UNKNOWN;
    inst.size = OperandSize::NONE;

    // 1. Detect NOP (Exactly 0x4E71)
    if (opcode == 0x4E71) {
        inst.type = OpType::NOP;
        inst.size = OperandSize::NONE;
        return inst;
    }

    // 2. Detect standard JMP instruction
    // Bit pattern: 0100 1110 11mm mrrr
    // - Bits 15-6 are 0100 1110 11 (Hex 0x4EC0 with mask 0xFFC0)
    if ((opcode & 0xFFC0) == 0x4EC0) {
        inst.type = OpType::JMP;
        inst.size = OperandSize::NONE;

        Byte destMode = (opcode >> 3) & 0x7;
        Byte destReg  = opcode & 0x7;

        inst.destMode     = ParseAddressingMode(destMode, destReg);
        inst.destRegister = destReg;

        return inst;
    }

    // 3. Detect relative branch family (Bcc and BRA)
    // Bit pattern: 0110 cccc dddd dddd (Opcode starts with Hex 0x6)
    if ((opcode & 0xF000) == 0x6000) {
        Byte condition = (opcode >> 8) & 0x0F; // Extract Condition Code (Bits 11-8)
        Byte disp8     = opcode & 0xFF;        // Extract 8-bit Displacement (Bits 7-0)
        bool validBranch = false;

        // Route the branch condition code to the corresponding OpType
        switch (condition) {
            case 0x0:
                inst.type = OpType::BRA;
                validBranch = true;
                break;
            case 0x6:
                inst.type = OpType::BNE;
                validBranch = true;
                break;
            case 0x7:
                inst.type = OpType::BEQ;
                validBranch = true;
                break;
            case 0xA:
                inst.type = OpType::BPL;
                validBranch = true;
                break;
            case 0xB:
                inst.type = OpType::BMI;
                validBranch = true;
                break;
            default:
                break;
        }

        if (validBranch) {
            if (disp8 == 0) {
                // If 8-bit displacement is 0, it's a 16-bit Word displacement
                inst.size     = OperandSize::WORD;
                inst.destMode = AddressingMode::ProgramCounterDisplacement;
            } else {
                // Standard 8-bit Byte displacement
                inst.size     = OperandSize::BYTE;
                inst.destMode = AddressingMode::ProgramCounterDisplacement;
            }
            return inst;
        }
    }

    // 4. Detect standard MOVE and MOVEA instructions
    // Bit pattern: 00 ss ddd mmm mms sss
    // - Bits 15-14 are 00 (Opcode identifier for MOVE)
    // - Bits 13-12 are size (01 = Byte, 11 = Word, 10 = Long). Cannot be 00.
    if ((opcode & 0xC000) == 0x0000 && (opcode & 0x3000) != 0x0000) {
        inst.type = OpType::MOVE;

        // Parse Operand Size (Bits 13-12)
        Word sizeBits = (opcode >> 12) & 0x3;
        if (sizeBits == 0x01) inst.size = OperandSize::BYTE;
        else if (sizeBits == 0x03) inst.size = OperandSize::WORD;
        else if (sizeBits == 0x02) inst.size = OperandSize::LONG;

        // Extract raw register and mode bits from opcode
        Byte destReg  = (opcode >> 9) & 0x7;  // Destination Register (Bits 11-9)
        Byte destMode = (opcode >> 6) & 0x7;  // Destination Mode (Bits 8-6)
        Byte srcMode  = (opcode >> 3) & 0x7;  // Source Mode (Bits 5-3)
        Byte srcReg   = opcode & 0x7;         // Source Register (Bits 2-0)

        // Parse Addressing Modes using our physical hardware translation helper
        inst.srcMode      = ParseAddressingMode(srcMode, srcReg);
        inst.srcRegister  = srcReg;
        
        inst.destMode     = ParseAddressingMode(destMode, destReg);
        inst.destRegister = destReg;

        return inst;
    }

    // Default returns OpType::UNKNOWN
    return inst;
}

// ------------------------------------------------------------------------------
// Addressing Mode Hardware Decoder
// ------------------------------------------------------------------------------
AddressingMode M68kDecoder::ParseAddressingMode(Byte modeBits, Byte regBits) {
    switch (modeBits) {
        case 0x0: // Binary 000
            return AddressingMode::DataRegisterDirect; // Dn
        case 0x1: // Binary 001
            return AddressingMode::AddressRegisterDirect; // An
        case 0x2: // Binary 010
            return AddressingMode::AddressRegisterIndirect; // (An)
        case 0x3: // Binary 011
            return AddressingMode::AddressRegisterPostincrement; // (An)+
            
        case 0x7: // Binary 111 (Special Modes)
            if (regBits == 0x4) {
                return AddressingMode::Immediate; // #<data>
            }
            if (regBits == 0x1) {
                return AddressingMode::AbsoluteLong; // (xxx).L
            }
            // Fallback for other Mode 7 extensions (Absolute Short, PC)
            return AddressingMode::Immediate;

        default:
            // Fallback for complex modes (predecrement, etc.)
            return AddressingMode::Immediate;
    }
}

} // namespace GenesisEmu::Core