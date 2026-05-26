// ==============================================================================
// GenesisEmu - Motorola 68000 Instruction Decoder Implementation (Updated)
// ==============================================================================
// Added decoding support for Absolute Short addressing modes.
// ==============================================================================

#include "M68kDecoder.h"

namespace GenesisEmu::Core {

DecodedInstruction M68kDecoder::Decode(Word opcode) {
    DecodedInstruction inst;
    inst.type = OpType::UNKNOWN;
    inst.size = OperandSize::NONE;

    // 1. Detect NOP
    if (opcode == 0x4E71) {
        inst.type = OpType::NOP;
        inst.size = OperandSize::NONE;
        return inst;
    }

    // 2. Detect RTS
    if (opcode == 0x4E75) {
        inst.type = OpType::RTS;
        inst.size = OperandSize::NONE;
        return inst;
    }

    // 3. Detect MOVE_USP
    if ((opcode & 0xFFF0) == 0x4E60) {
        inst.type = OpType::MOVE_USP;
        inst.size = OperandSize::LONG;
        return inst;
    }

    // 4. Detect MOVE_TO_SR
    if ((opcode & 0xFFC0) == 0x46C0) {
        inst.type = OpType::MOVE_TO_SR;
        inst.size = OperandSize::WORD;

        Byte srcMode = (opcode >> 3) & 0x7;
        Byte srcReg  = opcode & 0x7;

        inst.srcMode = ParseAddressingMode(srcMode, srcReg);
        inst.srcRegister = srcReg;
        return inst;
    }

    // 5. Detect JSR
    if ((opcode & 0xFFC0) == 0x4E80) {
        inst.type = OpType::JSR;
        inst.size = OperandSize::NONE;

        Byte destMode = (opcode >> 3) & 0x7;
        Byte destReg  = opcode & 0x7;

        inst.destMode     = ParseAddressingMode(destMode, destReg);
        inst.destRegister = destReg;
        return inst;
    }

    // 6. Detect standard JMP instruction
    if ((opcode & 0xFFC0) == 0x4EC0) {
        inst.type = OpType::JMP;
        inst.size = OperandSize::NONE;

        Byte destMode = (opcode >> 3) & 0x7;
        Byte destReg  = opcode & 0x7;

        inst.destMode     = ParseAddressingMode(destMode, destReg);
        inst.destRegister = destReg;
        return inst;
    }

    // 7. Detect BSR
    if ((opcode & 0xFF00) == 0x6100) {
        inst.type = OpType::BSR;
        Byte disp8 = opcode & 0xFF;

        if (disp8 == 0) {
            inst.size     = OperandSize::WORD;
            inst.destMode = AddressingMode::ProgramCounterDisplacement;
        } else {
            inst.size     = OperandSize::BYTE;
            inst.destMode = AddressingMode::ProgramCounterDisplacement;
        }
        return inst;
    }

    // 8. Detect relative branch family (Bcc)
    if ((opcode & 0xF000) == 0x6000) {
        Byte condition = (opcode >> 8) & 0x0F; 
        Byte disp8     = opcode & 0xFF;        
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

    // 9. Detect ADD.W
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

    // 10. Detect SUBA (Subtract Address)
    if ((opcode & 0xF1C0) == 0x90C0) {
        inst.type = OpType::SUB;
        inst.size = ((opcode & 0x0100) != 0) ? OperandSize::LONG : OperandSize::WORD;

        Byte destReg  = (opcode >> 9) & 0x7;
        Byte srcMode  = (opcode >> 3) & 0x7;
        Byte srcReg   = opcode & 0x7;

        inst.srcMode      = ParseAddressingMode(srcMode, srcReg);
        inst.srcRegister  = srcReg;
        inst.destMode     = AddressingMode::AddressRegisterDirect; 
        inst.destRegister = destReg;
        return inst;
    }

    // 11. Detect SUB.W
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

    // 12. Detect AND.W
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

    // 13. Detect standard MOVE and MOVEA instructions
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
            if (regBits == 0x0) {
                return AddressingMode::AbsoluteShort; // Added: (xxx).W (16-bit address)
            }
            if (regBits == 0x1) {
                return AddressingMode::AbsoluteLong; // (xxx).L (32-bit address)
            }
            if (regBits == 0x4) {
                return AddressingMode::Immediate; // #<data>
            }
            return AddressingMode::Immediate;

        default:
            return AddressingMode::Immediate;
    }
}

} // namespace GenesisEmu::Core