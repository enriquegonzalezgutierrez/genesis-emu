// ==============================================================================
// GenesisEmu - Motorola 68000 Instruction Decoder Implementation (Updated)
// ==============================================================================
// This file implements the bit-mask parsing logic for standard M68k opcodes.
// Added decoding support for JSR, BSR, and RTS.
// ==============================================================================

#include "M68kDecoder.h"

namespace GenesisEmu::Core {

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

    // 2. Detect RTS (Exactly 0x4E75)
    if (opcode == 0x4E75) {
        inst.type = OpType::RTS;
        inst.size = OperandSize::NONE;
        return inst;
    }

    // 3. Detect JSR (Jump to Subroutine)
    // Bit pattern: 0100 1110 10mm mrrr (Hex 0x4E80 with mask 0xFFC0)
    if ((opcode & 0xFFC0) == 0x4E80) {
        inst.type = OpType::JSR;
        inst.size = OperandSize::NONE;

        Byte destMode = (opcode >> 3) & 0x7;
        Byte destReg  = opcode & 0x7;

        inst.destMode     = ParseAddressingMode(destMode, destReg);
        inst.destRegister = destReg;
        return inst;
    }

    // 4. Detect standard JMP instruction
    // Bit pattern: 0100 1110 11mm mrrr (Hex 0x4EC0 with mask 0xFFC0)
    if ((opcode & 0xFFC0) == 0x4EC0) {
        inst.type = OpType::JMP;
        inst.size = OperandSize::NONE;

        Byte destMode = (opcode >> 3) & 0x7;
        Byte destReg  = opcode & 0x7;

        inst.destMode     = ParseAddressingMode(destMode, destReg);
        inst.destRegister = destReg;
        return inst;
    }

    // 5. Detect BSR (Branch to Subroutine)
    // Bit pattern: 0110 0001 dddd dddd (Hex 0x6100 with mask 0xFF00)
    if ((opcode & 0xFF00) == 0x6100) {
        inst.type = OpType::BSR;
        Byte disp8 = opcode & 0xFF;

        if (disp8 == 0) {
            // 16-bit displacement
            inst.size     = OperandSize::WORD;
            inst.destMode = AddressingMode::ProgramCounterDisplacement;
        } else {
            // 8-bit displacement
            inst.size     = OperandSize::BYTE;
            inst.destMode = AddressingMode::ProgramCounterDisplacement;
        }
        return inst;
    }

    // 6. Detect relative branch family (Bcc)
    // Bit pattern: 0110 cccc dddd dddd (Opcode starts with Hex 0x6)
    if ((opcode & 0xF000) == 0x6000) {
        Byte condition = (opcode >> 8) & 0x0F; // Extract Condition Code (Bits 11-8)
        Byte disp8     = opcode & 0xFF;        // Extract 8-bit Displacement (Bits 7-0)
        bool validBranch = false;

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
                inst.size     = OperandSize::WORD;
                inst.destMode = AddressingMode::ProgramCounterDisplacement;
            } else {
                inst.size     = OperandSize::BYTE;
                inst.destMode = AddressingMode::ProgramCounterDisplacement;
            }
            return inst;
        }
    }

    // 7. Detect ADD.W (Register-to-Register Addition)
    if ((opcode & 0xF000) == 0xD000 && ((opcode >> 8) & 0x1) == 0 && ((opcode >> 6) & 0x3) == 0x1) {
        inst.type = OpType::ADD;
        inst.size = OperandSize::WORD;

        Byte destReg  = (opcode >> 9) & 0x7; 
        Byte srcMode  = (opcode >> 3) & 0x7; 
        Byte srcReg   = opcode & 0x7;        

        inst.srcMode      = ParseAddressingMode(srcMode, srcReg);
        inst.srcRegister  = srcReg;
        inst.destMode     = AddressingMode::DataRegisterDirect;
        inst.destRegister = destReg;

        return inst;
    }

    // 8. Detect SUB.W (Register-to-Register Subtraction)
    if ((opcode & 0xF000) == 0x9000 && ((opcode >> 8) & 0x1) == 0 && ((opcode >> 6) & 0x3) == 0x1) {
        inst.type = OpType::SUB;
        inst.size = OperandSize::WORD;

        Byte destReg  = (opcode >> 9) & 0x7; 
        Byte srcMode  = (opcode >> 3) & 0x7; 
        Byte srcReg   = opcode & 0x7;        

        inst.srcMode      = ParseAddressingMode(srcMode, srcReg);
        inst.srcRegister  = srcReg;
        inst.destMode     = AddressingMode::DataRegisterDirect;
        inst.destRegister = destReg;

        return inst;
    }

    // 9. Detect AND.W (Register-to-Register Logical AND)
    if ((opcode & 0xF000) == 0xC000 && ((opcode >> 8) & 0x1) == 0 && ((opcode >> 6) & 0x3) == 0x1) {
        inst.type = OpType::AND;
        inst.size = OperandSize::WORD;

        Byte destReg  = (opcode >> 9) & 0x7; 
        Byte srcMode  = (opcode >> 3) & 0x7; 
        Byte srcReg   = opcode & 0x7;        

        inst.srcMode      = ParseAddressingMode(srcMode, srcReg);
        inst.srcRegister  = srcReg;
        inst.destMode     = AddressingMode::DataRegisterDirect;
        inst.destRegister = destReg;

        return inst;
    }

    // 10. Detect standard MOVE and MOVEA instructions
    if ((opcode & 0xC000) == 0x0000 && (opcode & 0x3000) != 0x0000) {
        inst.type = OpType::MOVE;

        Word sizeBits = (opcode >> 12) & 0x3;
        if (sizeBits == 0x01) inst.size = OperandSize::BYTE;
        else if (sizeBits == 0x03) inst.size = OperandSize::WORD;
        else if (sizeBits == 0x02) inst.size = OperandSize::LONG;

        Byte destReg  = (opcode >> 9) & 0x7;  
        Byte destMode = (opcode >> 6) & 0x7;  
        Byte srcMode  = (opcode >> 3) & 0x7;  
        Byte srcReg   = opcode & 0x7;         

        inst.srcMode      = ParseAddressingMode(srcMode, srcReg);
        inst.srcRegister  = srcReg;
        
        inst.destMode     = ParseAddressingMode(destMode, destReg);
        inst.destRegister = destReg;

        return inst;
    }

    return inst;
}

AddressingMode M68kDecoder::ParseAddressingMode(Byte modeBits, Byte regBits) {
    switch (modeBits) {
        case 0x0: 
            return AddressingMode::DataRegisterDirect; // Dn
        case 0x1: 
            return AddressingMode::AddressRegisterDirect; // An
        case 0x2: 
            return AddressingMode::AddressRegisterIndirect; // (An)
        case 0x3: 
            return AddressingMode::AddressRegisterPostincrement; // (An)+
            
        case 0x7: 
            if (regBits == 0x4) {
                return AddressingMode::Immediate; // #<data>
            }
            if (regBits == 0x1) {
                return AddressingMode::AbsoluteLong; // (xxx).L
            }
            return AddressingMode::Immediate;

        default:
            return AddressingMode::Immediate;
    }
}

} // namespace GenesisEmu::Core