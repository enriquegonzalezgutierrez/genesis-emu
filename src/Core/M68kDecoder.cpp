// ==============================================================================
// GenesisEmu - Motorola 68000 Instruction Decoder Implementation (Updated)
// ==============================================================================
// Added decoding support for ADDQ and SUBQ (Add/Subtract Quick) instructions.
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

    // 4. Detect ADDQ / SUBQ (Quick Operations)
    // Immediate data is encoded in bits 11-9 of the opcode — NO extension word is read.
    // Emits ADDQ/SUBQ (not ADD/SUB) so the executor uses immediateData directly.
    if ((opcode & 0xF000) == 0x5000 && ((opcode >> 6) & 0x3) != 0x3) {
        // Bit 8 determines ADDQ (0) or SUBQ (1)
        inst.type = ((opcode & 0x0100) == 0) ? OpType::ADDQ : OpType::SUBQ;

        Byte sizeBits = (opcode >> 6) & 0x3;
        if (sizeBits == 0x0) inst.size = OperandSize::BYTE;
        else if (sizeBits == 0x1) inst.size = OperandSize::WORD;
        else inst.size = OperandSize::LONG;

        // Quick immediate lives in bits 11-9; 0 encodes 8.
        Byte quickData = (opcode >> 9) & 0x7;
        if (quickData == 0) quickData = 8;
        inst.immediateData = quickData;

        Byte destMode = (opcode >> 3) & 0x7;
        Byte destReg  = opcode & 0x7;
        inst.destMode     = ParseAddressingMode(destMode, destReg);
        inst.destRegister = destReg;

        return inst;
    }

    // 5. Detect Shift Register Immediate (LSR, LSL)
    if ((opcode & 0xF038) == 0xE008) {
        bool isLeft = (opcode & 0x0100) != 0; 
        inst.type = isLeft ? OpType::LSL : OpType::LSR;

        Byte sizeBits = (opcode >> 6) & 0x3; 
        if (sizeBits == 0x0) inst.size = OperandSize::BYTE;
        else if (sizeBits == 0x1) inst.size = OperandSize::WORD;
        else if (sizeBits == 0x2) inst.size = OperandSize::LONG;

        Byte shiftCount = (opcode >> 9) & 0x7; 
        if (shiftCount == 0) shiftCount = 8;   

        inst.srcMode = AddressingMode::Immediate;
        inst.immediateData = shiftCount;

        inst.destMode = AddressingMode::DataRegisterDirect;
        inst.destRegister = opcode & 0x7; 

        return inst;
    }

    // 5.5. Detect Immediate instructions (ORI, ANDI, SUBI, ADDI, EORI)
    Word base00 = opcode & 0xFF00;
    if (base00 == 0x0000 || base00 == 0x0200 || base00 == 0x0400 || base00 == 0x0600 || base00 == 0x0A00) {
        Byte sizeBits = (opcode >> 6) & 0x3;
        if (sizeBits != 0x3) { 
            if (base00 == 0x0000)      inst.type = OpType::OR;
            else if (base00 == 0x0200) inst.type = OpType::AND;
            else if (base00 == 0x0400) inst.type = OpType::SUB;
            else if (base00 == 0x0600) inst.type = OpType::ADD;
            else if (base00 == 0x0A00) inst.type = OpType::EOR;

            if (sizeBits == 0x0)      inst.size = OperandSize::BYTE;
            else if (sizeBits == 0x1) inst.size = OperandSize::WORD;
            else                      inst.size = OperandSize::LONG;

            inst.srcMode = AddressingMode::Immediate;

            Byte destMode = (opcode >> 3) & 0x7;
            Byte destReg  = opcode & 0x7;
            inst.destMode = ParseAddressingMode(destMode, destReg);
            inst.destRegister = destReg;

            return inst;
        }
    }

    // 6. Detect CMPI (Compare Immediate)
    Word base0C = opcode & 0xFFC0;
    if (base0C == 0x0C00 || base0C == 0x0C40 || base0C == 0x0C80) {
        inst.type = OpType::CMPI;
        if (base0C == 0x0C00)      inst.size = OperandSize::BYTE;
        else if (base0C == 0x0C40) inst.size = OperandSize::WORD;
        else if (base0C == 0x0C80) inst.size = OperandSize::LONG;

        Byte destMode = (opcode >> 3) & 0x7;
        Byte destReg  = opcode & 0x7;

        inst.destMode = ParseAddressingMode(destMode, destReg);
        inst.destRegister = destReg;
        return inst;
    }

    // 7a. Detect SWAP (0x4840-0x4847) — must come before PEA (same mask base)
    // SWAP Dn: 0100 1000 0100 0nnn (EA mode = 0 = DataRegisterDirect)
    if ((opcode & 0xFFF8) == 0x4840) {
        inst.type = OpType::SWAP;
        inst.size = OperandSize::LONG;
        inst.destMode     = AddressingMode::DataRegisterDirect;
        inst.destRegister = opcode & 0x7;
        return inst;
    }

    // 7b. Detect EXT — sign extend Dn byte->word (0x4880-0x4887) or word->long (0x48C0-0x48C7)
    if ((opcode & 0xFFF8) == 0x4880 || (opcode & 0xFFF8) == 0x48C0) {
        inst.type = OpType::EXT;
        inst.size = ((opcode & 0xFFF8) == 0x4880) ? OperandSize::WORD : OperandSize::LONG;
        inst.destMode     = AddressingMode::DataRegisterDirect;
        inst.destRegister = opcode & 0x7;
        return inst;
    }

    // 7c. Detect PEA — EA modes 2-7 only (mode 0 = SWAP, mode 1 = illegal)
    if ((opcode & 0xFFC0) == 0x4840) {
        Byte eaMode = (opcode >> 3) & 0x7;
        if (eaMode >= 2) {
            inst.type = OpType::PEA;
            inst.size = OperandSize::LONG;

            Byte destReg  = opcode & 0x7;
            inst.destMode     = ParseAddressingMode(eaMode, destReg);
            inst.destRegister = destReg;
            return inst;
        }
    }

    // 8. Detect CLR
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

    // 9. Detect DBF
    if ((opcode & 0xFFF8) == 0x51C8) {
        inst.type = OpType::DBF;
        inst.size = OperandSize::WORD;
        inst.srcRegister = opcode & 0x0007; 
        inst.destMode = AddressingMode::ProgramCounterDisplacement;
        return inst;
    }

    // 10. Detect TST
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

    // 11. Detect BTST Static
    if ((opcode & 0xFFC0) == 0x0800) {
        inst.type = OpType::BTST;
        inst.srcMode = AddressingMode::Immediate; 
        
        Byte destMode = (opcode >> 3) & 0x7;
        Byte destReg  = opcode & 0x7;

        inst.destMode = ParseAddressingMode(destMode, destReg);
        inst.destRegister = destReg;

        if (inst.destMode == AddressingMode::DataRegisterDirect) {
            inst.size = OperandSize::LONG;
        } else {
            inst.size = OperandSize::BYTE;
        }
        return inst;
    }

    // 12. Detect MOVE_TO_SR
    if ((opcode & 0xFFC0) == 0x46C0) {
        inst.type = OpType::MOVE_TO_SR;
        inst.size = OperandSize::WORD;

        Byte srcMode = (opcode >> 3) & 0x7;
        Byte srcReg  = opcode & 0x7;

        inst.srcMode = ParseAddressingMode(srcMode, srcReg);
        inst.srcRegister = srcReg;
        return inst;
    }

    // 13. Detect JSR
    if ((opcode & 0xFFC0) == 0x4E80) {
        inst.type = OpType::JSR;
        inst.size = OperandSize::NONE;

        Byte destMode = (opcode >> 3) & 0x7;
        Byte destReg  = opcode & 0x7;

        inst.destMode     = ParseAddressingMode(destMode, destReg);
        inst.destRegister = destReg;
        return inst;
    }

    // 14. Detect standard JMP instruction
    if ((opcode & 0xFFC0) == 0x4EC0) {
        inst.type = OpType::JMP;
        inst.size = OperandSize::NONE;

        Byte destMode = (opcode >> 3) & 0x7;
        Byte destReg  = opcode & 0x7;

        inst.destMode     = ParseAddressingMode(destMode, destReg);
        inst.destRegister = destReg;
        return inst;
    }

    // 15. Detect BSR
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

    // 16. Detect relative branch family (Bcc)
    if ((opcode & 0xF000) == 0x6000) {
        Byte condition = (opcode >> 8) & 0x0F; 
        Byte disp8     = opcode & 0xFF;        
        bool validBranch = false;

        switch (condition) {
            case 0x0:
                inst.type = OpType::BRA;
                validBranch = true;
                break;
            case 0x2: 
                inst.type = OpType::BHI;
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

    // 17. Detect ADD / ADDA
    if ((opcode & 0xF000) == 0xD000) {
        Byte opmode = (opcode >> 6) & 0x7;
        if (opmode == 0x3 || opmode == 0x7) {
            inst.type = OpType::ADD;
            inst.size = (opmode == 0x3) ? OperandSize::WORD : OperandSize::LONG;
            Byte srcMode = (opcode >> 3) & 0x7;
            Byte srcReg  = opcode & 0x7;
            inst.srcMode = ParseAddressingMode(srcMode, srcReg);
            inst.srcRegister = srcReg;
            inst.destMode = AddressingMode::AddressRegisterDirect;
            inst.destRegister = (opcode >> 9) & 0x7;
            return inst;
        } else if (opmode != 0x4) {
            inst.type = OpType::ADD;
            inst.size = (opmode == 0x0 || opmode == 0x5) ? OperandSize::BYTE :
                        (opmode == 0x1 || opmode == 0x6) ? OperandSize::WORD : OperandSize::LONG;
            bool directionToRegister = (opmode & 0x4) == 0;
            Byte reg = (opcode >> 9) & 0x7;
            Byte eaMode = (opcode >> 3) & 0x7;
            Byte eaReg  = opcode & 0x7;
            if (directionToRegister) {
                inst.srcMode = ParseAddressingMode(eaMode, eaReg);
                inst.srcRegister = eaReg;
                inst.destMode = AddressingMode::DataRegisterDirect;
                inst.destRegister = reg;
            } else {
                inst.srcMode = AddressingMode::DataRegisterDirect;
                inst.srcRegister = reg;
                inst.destMode = ParseAddressingMode(eaMode, eaReg);
                inst.destRegister = eaReg;
            }
            return inst;
        }
    }

    // 18. Detect SUB / SUBA
    if ((opcode & 0xF000) == 0x9000) {
        Byte opmode = (opcode >> 6) & 0x7;
        if (opmode == 0x3 || opmode == 0x7) {
            inst.type = OpType::SUB;
            inst.size = (opmode == 0x3) ? OperandSize::WORD : OperandSize::LONG;
            Byte srcMode = (opcode >> 3) & 0x7;
            Byte srcReg  = opcode & 0x7;
            inst.srcMode = ParseAddressingMode(srcMode, srcReg);
            inst.srcRegister = srcReg;
            inst.destMode = AddressingMode::AddressRegisterDirect;
            inst.destRegister = (opcode >> 9) & 0x7;
            return inst;
        } else if (opmode != 0x4) {
            inst.type = OpType::SUB;
            inst.size = (opmode == 0x0 || opmode == 0x5) ? OperandSize::BYTE :
                        (opmode == 0x1 || opmode == 0x6) ? OperandSize::WORD : OperandSize::LONG;
            bool directionToRegister = (opmode & 0x4) == 0;
            Byte reg = (opcode >> 9) & 0x7;
            Byte eaMode = (opcode >> 3) & 0x7;
            Byte eaReg  = opcode & 0x7;
            if (directionToRegister) {
                inst.srcMode = ParseAddressingMode(eaMode, eaReg);
                inst.srcRegister = eaReg;
                inst.destMode = AddressingMode::DataRegisterDirect;
                inst.destRegister = reg;
            } else {
                inst.srcMode = AddressingMode::DataRegisterDirect;
                inst.srcRegister = reg;
                inst.destMode = ParseAddressingMode(eaMode, eaReg);
                inst.destRegister = eaReg;
            }
            return inst;
        }
    }

    // 20. Detect AND
    if ((opcode & 0xF000) == 0xC000) {
        Byte opmode = (opcode >> 6) & 0x7;
        if (opmode != 0x3 && opmode != 0x7) {
            inst.type = OpType::AND;
            inst.size = (opmode == 0x0 || opmode == 0x4) ? OperandSize::BYTE :
                        (opmode == 0x1 || opmode == 0x5) ? OperandSize::WORD : OperandSize::LONG;
            bool directionToRegister = (opmode & 0x4) == 0;
            Byte reg = (opcode >> 9) & 0x7;
            Byte eaMode = (opcode >> 3) & 0x7;
            Byte eaReg  = opcode & 0x7;
            if (directionToRegister) {
                inst.srcMode = ParseAddressingMode(eaMode, eaReg);
                inst.srcRegister = eaReg;
                inst.destMode = AddressingMode::DataRegisterDirect;
                inst.destRegister = reg;
            } else {
                inst.srcMode = AddressingMode::DataRegisterDirect;
                inst.srcRegister = reg;
                inst.destMode = ParseAddressingMode(eaMode, eaReg);
                inst.destRegister = eaReg;
            }
            return inst;
        }
    }

    // 20.5. Detect MOVEQ
    if ((opcode & 0xF100) == 0x7000) {
        inst.type = OpType::MOVEQ;
        inst.size = OperandSize::LONG;
        inst.srcMode = AddressingMode::Immediate;
        // Sign-extend the 8-bit immediate value
        std::int8_t imm8 = static_cast<std::int8_t>(opcode & 0xFF);
        inst.immediateData = static_cast<Longword>(static_cast<std::int32_t>(imm8));
        inst.destMode = AddressingMode::DataRegisterDirect;
        inst.destRegister = (opcode >> 9) & 0x7;
        return inst;
    }

    // 20.6. Detect LEA
    if ((opcode & 0xF1C0) == 0x41C0) {
        inst.type = OpType::LEA;
        inst.size = OperandSize::LONG;
        Byte srcMode = (opcode >> 3) & 0x7;
        Byte srcReg  = opcode & 0x7;
        inst.srcMode = ParseAddressingMode(srcMode, srcReg);
        inst.srcRegister = srcReg;
        inst.destMode = AddressingMode::AddressRegisterDirect;
        inst.destRegister = (opcode >> 9) & 0x7;
        return inst;
    }

    // 20.7. Detect MOVEM (Store & Load)
    if (((opcode & 0xFB80) == 0x4880) || ((opcode & 0xFB80) == 0x4C80)) {
        Byte eaMode = (opcode >> 3) & 0x7;
        if (eaMode >= 2) {
            inst.type = OpType::MOVEM;
            inst.size = ((opcode & 0x0040) == 0) ? OperandSize::WORD : OperandSize::LONG;
            inst.immediateData = opcode; // Store opcode to identify direction (Load vs Store) in execution
            Byte eaReg = opcode & 0x7;
            AddressingMode resolvedEA = ParseAddressingMode(eaMode, eaReg);
            
            bool isLoad = (opcode & 0x0400) != 0;
            if (isLoad) {
                inst.srcMode = resolvedEA;
                inst.srcRegister = eaReg;
                inst.destMode = AddressingMode::Immediate; // Dummy destination
            } else {
                inst.srcMode = AddressingMode::Immediate; // Dummy source
                inst.destMode = resolvedEA;
                inst.destRegister = eaReg;
            }
            return inst;
        }
    }

    // 20.8. Detect CMP / CMPA
    if ((opcode & 0xF000) == 0xB000) {
        Byte opmode = (opcode >> 6) & 0x7;
        if (opmode == 0x3 || opmode == 0x7) {
            inst.type = OpType::CMP;
            inst.size = (opmode == 0x3) ? OperandSize::WORD : OperandSize::LONG;
            Byte srcMode = (opcode >> 3) & 0x7;
            Byte srcReg  = opcode & 0x7;
            inst.srcMode = ParseAddressingMode(srcMode, srcReg);
            inst.srcRegister = srcReg;
            inst.destMode = AddressingMode::AddressRegisterDirect;
            inst.destRegister = (opcode >> 9) & 0x7;
            return inst;
        } else if ((opmode & 0x3) != 0x3 && (opmode & 0x4) == 0) {
            inst.type = OpType::CMP;
            inst.size = (opmode == 0x0) ? OperandSize::BYTE :
                        (opmode == 0x1) ? OperandSize::WORD : OperandSize::LONG;
            Byte srcMode = (opcode >> 3) & 0x7;
            Byte srcReg  = opcode & 0x7;
            inst.srcMode = ParseAddressingMode(srcMode, srcReg);
            inst.srcRegister = srcReg;
            inst.destMode = AddressingMode::DataRegisterDirect;
            inst.destRegister = (opcode >> 9) & 0x7;
            return inst;
        }
    }

    // 21. Detect standard MOVE and MOVEA instructions
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
        case 0x0: return AddressingMode::DataRegisterDirect; 
        case 0x1: return AddressingMode::AddressRegisterDirect; 
        case 0x2: return AddressingMode::AddressRegisterIndirect; 
        case 0x3: return AddressingMode::AddressRegisterPostincrement; 
        case 0x4: return AddressingMode::AddressRegisterPredecrement; 
        case 0x5: return AddressingMode::AddressRegisterDisplacement; 

        case 0x7: 
            if (regBits == 0x0) return AddressingMode::AbsoluteShort; 
            if (regBits == 0x1) return AddressingMode::AbsoluteLong; 
            if (regBits == 0x2) return AddressingMode::ProgramCounterDisplacement; 
            if (regBits == 0x4) return AddressingMode::Immediate; 
            return AddressingMode::Immediate;

        default:
            return AddressingMode::Immediate;
    }
}

} // namespace GenesisEmu::Core