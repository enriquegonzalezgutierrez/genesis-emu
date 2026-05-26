// ==============================================================================
// GenesisEmu - Motorola 68000 Instruction Decoder Implementation (Updated)
// ==============================================================================
// Added decoding support for the PEA (Push Effective Address) instruction.
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

    // 4. Detect PEA (Push Effective Address to Stack)
    // Bit pattern: 0100 1000 01mm mrrr (Hex 0x4840 with mask 0xFFC0)
    // The lower 6 bits (mm mrrr) represent the effective addressing mode
    if ((opcode & 0xFFC0) == 0x4840) {
        inst.type = OpType::PEA;
        inst.size = OperandSize::LONG; // PEA always operates on Longword addresses

        Byte destMode = (opcode >> 3) & 0x7;
        Byte destReg  = opcode & 0x7;

        inst.destMode = ParseAddressingMode(destMode, destReg);
        inst.destRegister = destReg;
        return inst;
    }

    // 5. Detect CLR
    Word base42 = opcode & 0xFFC0;
    if (base42 == 0x4200 || base42 == 0x4240 || base42 == 0x4280) {
        inst.type = OpType::CLR;
        if (base42 == 0x4200)      inst.size = OperandSize::BYTE;
        else if (base42 == 0x4240) inst.size = OperandSize::WORD;
        else if (base42 == 0x4280) inst.size = OperandSize::LONG;

        Byte destMode = (opcode >> 3) & 0x7;
        Byte destReg  = opcode & 0x7;

        inst.destMode = ParseAddressingMode(destMode, destReg);
        inst.destRegister = destReg;
        return inst;
    }

    // 6. Detect DBF
    if ((opcode & 0xFFF8) == 0x51C0) {
        inst.type = OpType::DBF;
        inst.size = OperandSize::WORD;
        inst.srcRegister = opcode & 0x0007; 
        inst.destMode = AddressingMode::ProgramCounterDisplacement;
        return inst;
    }

    // 7. Detect TST
    Word base4A = opcode & 0xFFC0;
    if (base4A == 0x4A00 || base4A == 0x4A40 || base4A == 0x4A80) {
        inst.type = OpType::TST;
        if (base4A == 0x4A00)      inst.size = OperandSize::BYTE;
        else if (base4A == 0x4A40) inst.size = OperandSize::WORD;
        else if (base4A == 0x4A80) inst.size = OperandSize::LONG;

        Byte srcMode = (opcode >> 3) & 0x7;
        Byte srcReg  = opcode & 0x7;

        inst.srcMode = ParseAddressingMode(srcMode, srcReg);
        inst.srcRegister = srcReg;
        return inst;
    }

    // 8. Detect MOVE_TO_SR
    if ((opcode & 0xFFC0) == 0x46C0) {
        inst.type = OpType::MOVE_TO_SR;
        inst.size = OperandSize::WORD;

        Byte srcMode = (opcode >> 3) & 0x7;
        Byte srcReg  = opcode & 0x7;

        inst.srcMode = ParseAddressingMode(srcMode, srcReg);
        inst.srcRegister = srcReg;
        return inst;
    }

    // 9. Detect JSR
    if ((opcode & 0xFFC0) == 0x4E80) {
        inst.type = OpType::JSR;
        inst.size = OperandSize::NONE;

        Byte destMode = (opcode >> 3) & 0x7;
        Byte destReg  = opcode & 0x7;

        inst.destMode     = ParseAddressingMode(destMode, destReg);
        inst.destRegister = destReg;
        return inst;
    }

    // 10. Detect standard JMP instruction
    if ((opcode & 0xFFC0) == 0x4EC0) {
        inst.type = OpType::JMP;
        inst.size = OperandSize::NONE;

        Byte destMode = (opcode >> 3) & 0x7;
        Byte destReg  = opcode & 0x7;

        inst.destMode     = ParseAddressingMode(destMode, destReg);
        inst.destRegister = destReg;
        return inst;
    }

    // 11. Detect BSR
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

    // 12. Detect relative branch family (Bcc)
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

    // 13. Detect ADD.W
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

    // 14. Detect SUBA
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

    // 15. Detect SUB.W
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

    // 16. Detect AND.W
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

    // 17. Detect standard MOVE and MOVEA instructions
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
        case 0x4:
            return AddressingMode::AddressRegisterPredecrement; // -(An)
        case 0x5:
            return AddressingMode::AddressRegisterDisplacement; // (d16, An)

        case 0x7: 
            if (regBits == 0x0) {
                return AddressingMode::AbsoluteShort; 
            }
            if (regBits == 0x1) {
                return AddressingMode::AbsoluteLong; 
            }
            if (regBits == 0x2) {
                return AddressingMode::ProgramCounterDisplacement; 
            }
            if (regBits == 0x4) {
                return AddressingMode::Immediate; 
            }
            return AddressingMode::Immediate;

        default:
            return AddressingMode::Immediate;
    }
}

} // namespace GenesisEmu::Core