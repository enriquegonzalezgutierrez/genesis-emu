// ==============================================================================
// GenesisEmu - Motorola 68000 Instruction Decoder (Core Domain)
// ==============================================================================
// This file implements raw 16-bit binary opcode decoding into structured
// instruction packets.
// ==============================================================================

#include "M68kDecoder.h"

namespace GenesisEmu::Core::Domain::M68k {

using namespace GenesisEmu::Core::Domain::Common;

DecodedInstruction M68kDecoder::Decode(Word opcode) {
    DecodedInstruction inst;
    inst.type = OpType::UNKNOWN;
    inst.size = OperandSize::NONE;

    // 1. NOP Detection
    if (opcode == 0x4E71) {
        inst.type = OpType::NOP;
        inst.size = OperandSize::NONE;
        return inst;
    }

    // 2. RTS Detection
    if (opcode == 0x4E75) {
        inst.type = OpType::RTS;
        inst.size = OperandSize::NONE;
        return inst;
    }

    // 2.5. RTE Detection (Return from Exception)
    if (opcode == 0x4E73) {
        inst.type = OpType::RTE;
        inst.size = OperandSize::NONE;
        return inst;
    }

    // 3. MOVE_USP Detection
    if ((opcode & 0xFFF0) == 0x4E60) {
        inst.type = OpType::MOVE_USP;
        inst.size = OperandSize::LONG;
        return inst;
    }

    // 4. ADDQ / SUBQ Detection (Quick operations)
    if ((opcode & 0xF000) == 0x5000 && ((opcode >> 6) & 0x3) != 0x3) {
        inst.type = ((opcode & 0x0100) == 0) ? OpType::ADDQ : OpType::SUBQ;

        Byte sizeBits = (opcode >> 6) & 0x3;
        if (sizeBits == 0x0)      inst.size = OperandSize::BYTE;
        else if (sizeBits == 0x1) inst.size = OperandSize::WORD;
        else                      inst.size = OperandSize::LONG;

        Byte quickData = (opcode >> 9) & 0x7;
        if (quickData == 0) quickData = 8;
        inst.immediateData = quickData;

        Byte destMode = (opcode >> 3) & 0x7;
        Byte destReg  = opcode & 0x7;
        inst.destMode     = ParseAddressingMode(destMode, destReg);
        inst.destRegister = destReg;

        return inst;
    }

    // 4.5 MOVEQ Detection
    if ((opcode & 0xF000) == 0x7000) {
        inst.type = OpType::MOVEQ;
        inst.size = OperandSize::LONG;
        inst.srcMode = AddressingMode::Immediate;
        
        std::int8_t imm8 = static_cast<std::int8_t>(opcode & 0xFF);
        inst.immediateData = static_cast<Longword>(static_cast<std::int32_t>(imm8));
        
        inst.destMode = AddressingMode::DataRegisterDirect;
        inst.destRegister = (opcode >> 9) & 0x7;
        return inst;
    }

    // 5. Shifts and Rotates on Data Registers or Memory
    if ((opcode & 0xF000) == 0xE000) {
        Byte sizeBits = (opcode >> 6) & 0x3;
        
        if (sizeBits != 0x3) {
            bool isLeft = (opcode & 0x0100) != 0; 
            Byte typeBits = (opcode >> 3) & 0x3; 
            bool isImmediate = (opcode & 0x0020) == 0;

            if (typeBits == 0x0)      inst.type = isLeft ? OpType::ASL : OpType::ASR;
            else if (typeBits == 0x1) inst.type = isLeft ? OpType::LSL : OpType::LSR;
            else if (typeBits == 0x2) inst.type = isLeft ? OpType::ROXL : OpType::ROXR; 
            else if (typeBits == 0x3) inst.type = isLeft ? OpType::ROL : OpType::ROR;
            else return inst; 

            if (sizeBits == 0x0)      inst.size = OperandSize::BYTE;
            else if (sizeBits == 0x1) inst.size = OperandSize::WORD;
            else                      inst.size = OperandSize::LONG;

            if (isImmediate) {
                Byte shiftCount = (opcode >> 9) & 0x7; 
                if (shiftCount == 0) shiftCount = 8;   
                inst.srcMode = AddressingMode::Immediate;
                inst.immediateData = shiftCount;
            } else {
                Byte dataReg = (opcode >> 9) & 0x7;
                inst.srcMode = AddressingMode::DataRegisterDirect;
                inst.srcRegister = dataReg;
            }

            inst.destMode = AddressingMode::DataRegisterDirect;
            inst.destRegister = opcode & 0x7; 

            return inst;
        } else {
            bool isLeft = (opcode & 0x0100) != 0;
            Byte typeBits = (opcode >> 9) & 0x3;
            
            if (typeBits == 0x0)      inst.type = isLeft ? OpType::ASL : OpType::ASR;
            else if (typeBits == 0x1) inst.type = isLeft ? OpType::LSL : OpType::LSR;
            else if (typeBits == 0x2) inst.type = isLeft ? OpType::ROXL : OpType::ROXR;
            else if (typeBits == 0x3) inst.type = isLeft ? OpType::ROL : OpType::ROR;
            
            inst.size = OperandSize::WORD;
            inst.srcMode = AddressingMode::Immediate;
            inst.immediateData = 1; 
            
            Byte destMode = (opcode >> 3) & 0x7;
            Byte destReg  = opcode & 0x7;
            inst.destMode = ParseAddressingMode(destMode, destReg);
            inst.destRegister = destReg;
            
            return inst;
        }
    }

    // 5.5. Immediate Instructions (ANDI, ORI, EORI, SUBI, ADDI)
    Word base00 = opcode & 0xFF00;
    
    if (opcode == 0x027C) {
        inst.type = OpType::ANDI_TO_SR;
        inst.size = OperandSize::WORD;
        return inst;
    }
    if (opcode == 0x007C) {
        inst.type = OpType::ORI_TO_SR;
        inst.size = OperandSize::WORD;
        return inst;
    }
    if (opcode == 0x0A7C) {
        inst.type = OpType::EORI_TO_SR;
        inst.size = OperandSize::WORD;
        return inst;
    }

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

    // 6. CMPI Detection (Compare Immediate)
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

    // 7a. SWAP Detection
    if ((opcode & 0xFFF8) == 0x4840) {
        inst.type = OpType::SWAP;
        inst.size = OperandSize::LONG;
        inst.destMode     = AddressingMode::DataRegisterDirect;
        inst.destRegister = opcode & 0x7;
        return inst;
    }

    // 7b. EXT Detection
    if ((opcode & 0xFFF8) == 0x4880 || (opcode & 0xFFF8) == 0x48C0) {
        inst.type = OpType::EXT;
        inst.size = ((opcode & 0xFFF8) == 0x4880) ? OperandSize::WORD : OperandSize::LONG;
        inst.destMode     = AddressingMode::DataRegisterDirect;
        inst.destRegister = opcode & 0x7;
        return inst;
    }

    // 7c. PEA Detection
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

    // 8. CLR Detection
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

    // 8.5. NOT Detection
    Word base46 = opcode & 0xFF00;
    if (base46 == 0x4600) {
        Byte sizeBits = (opcode >> 6) & 0x3;
        if (sizeBits != 0x3) { 
            inst.type = OpType::NOT;
            if (sizeBits == 0x0)      inst.size = OperandSize::BYTE;
            else if (sizeBits == 0x1) inst.size = OperandSize::WORD;
            else if (sizeBits == 0x2) inst.size = OperandSize::LONG;

            Byte destMode = (opcode >> 3) & 0x7;
            Byte destReg  = opcode & 0x7;
            inst.destMode = ParseAddressingMode(destMode, destReg);
            inst.destRegister = destReg;
            return inst;
        }
    }

    // 9. DBF Detection
    if ((opcode & 0xFFF8) == 0x51C8) {
        inst.type = OpType::DBF;
        inst.size = OperandSize::WORD;
        inst.srcRegister = opcode & 0x0007; 
        inst.destMode = AddressingMode::ProgramCounterDisplacement;
        return inst;
    }

    // 10. TST Detection
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

    // 11. Dynamic Bit Manipulations (BCLR, BSET, BCHG, BTST Dn, <ea>)
    if ((opcode & 0xF100) == 0x0100) {
        Byte opmode = (opcode >> 6) & 0x03;
        if (opmode == 0x0)      inst.type = OpType::BTST;
        else if (opmode == 0x1) inst.type = OpType::BCHG;
        else if (opmode == 0x2) inst.type = OpType::BCLR;
        else if (opmode == 0x3) inst.type = OpType::BSET;

        inst.srcMode = AddressingMode::DataRegisterDirect;
        inst.srcRegister = (opcode >> 9) & 0x07;

        Byte destMode = (opcode >> 3) & 0x07;
        Byte destReg  = opcode & 0x07;
        inst.destMode = ParseAddressingMode(destMode, destReg);
        inst.destRegister = destReg;

        // Size is LONG if destination is Data Register direct, else BYTE
        inst.size = (inst.destMode == AddressingMode::DataRegisterDirect) ? OperandSize::LONG : OperandSize::BYTE;
        return inst;
    }

    // 11.5. Static Bit Manipulations (BCLR, BSET, BCHG, BTST #imm, <ea>)
    if ((opcode & 0xFF00) == 0x0800) {
        Byte opmode = (opcode >> 6) & 0x03;
        if (opmode == 0x0)      inst.type = OpType::BTST;
        else if (opmode == 0x1) inst.type = OpType::BCHG;
        else if (opmode == 0x2) inst.type = OpType::BCLR;
        else if (opmode == 0x3) inst.type = OpType::BSET;

        inst.srcMode = AddressingMode::Immediate; 

        Byte destMode = (opcode >> 3) & 0x07;
        Byte destReg  = opcode & 0x07;
        inst.destMode = ParseAddressingMode(destMode, destReg);
        inst.destRegister = destReg;

        inst.size = (inst.destMode == AddressingMode::DataRegisterDirect) ? OperandSize::LONG : OperandSize::BYTE;
        return inst;
    }

    // 12. MOVE_TO_SR Detection
    if ((opcode & 0xFFC0) == 0x46C0) {
        inst.type = OpType::MOVE_TO_SR;
        inst.size = OperandSize::WORD;

        Byte srcMode = (opcode >> 3) & 0x7;
        Byte srcReg  = opcode & 0x7;

        inst.srcMode = ParseAddressingMode(srcMode, srcReg);
        inst.srcRegister = srcReg;
        return inst;
    }

    // 12.1. MOVE_TO_CCR Detection
    if ((opcode & 0xFFC0) == 0x44C0) {
        inst.type = OpType::MOVE_TO_CCR;
        inst.size = OperandSize::WORD;

        Byte srcMode = (opcode >> 3) & 0x7;
        Byte srcReg  = opcode & 0x7;

        inst.srcMode = ParseAddressingMode(srcMode, srcReg);
        inst.srcRegister = srcReg;
        return inst;
    }

    // 12.5. MOVE_FROM_SR Detection
    if ((opcode & 0xFFC0) == 0x40C0) {
        inst.type = OpType::MOVE_FROM_SR;
        inst.size = OperandSize::WORD;

        inst.srcMode = AddressingMode::Immediate; 
        
        Byte destMode = (opcode >> 3) & 0x7;
        Byte destReg  = opcode & 0x7;
        inst.destMode = ParseAddressingMode(destMode, destReg);
        inst.destRegister = destReg;
        return inst;
    }

    // 13. JSR Detection
    if ((opcode & 0xFFC0) == 0x4E80) {
        inst.type = OpType::JSR;
        inst.size = OperandSize::NONE;

        Byte destMode = (opcode >> 3) & 0x7;
        Byte destReg  = opcode & 0x7;

        inst.destMode     = ParseAddressingMode(destMode, destReg);
        inst.destRegister = destReg;
        return inst;
    }

    // 14. JMP Detection
    if ((opcode & 0xFFC0) == 0x4EC0) {
        inst.type = OpType::JMP;
        inst.size = OperandSize::NONE;

        Byte destMode = (opcode >> 3) & 0x7;
        Byte destReg  = opcode & 0x7;

        inst.destMode     = ParseAddressingMode(destMode, destReg);
        inst.destRegister = destReg;
        return inst;
    }

    // 15. BSR Detection
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

    // 16. Branch Family Detection (Bcc)
    if ((opcode & 0xF000) == 0x6000) {
        Byte condition = (opcode >> 8) & 0x0F; 
        Byte disp8     = opcode & 0xFF;        
        bool validBranch = false;

        switch (condition) {
            case 0x0: inst.type = OpType::BRA; validBranch = true; break;
            case 0x2: inst.type = OpType::BHI; validBranch = true; break;
            case 0x3: inst.type = OpType::BLS; validBranch = true; break;
            case 0x4: inst.type = OpType::BCC; validBranch = true; break;
            case 0x5: inst.type = OpType::BCS; validBranch = true; break;
            case 0x6: inst.type = OpType::BNE; validBranch = true; break;
            case 0x7: inst.type = OpType::BEQ; validBranch = true; break;
            case 0x8: inst.type = OpType::BVC; validBranch = true; break;
            case 0x9: inst.type = OpType::BVS; validBranch = true; break;
            case 0xA: inst.type = OpType::BPL; validBranch = true; break;
            case 0xB: inst.type = OpType::BMI; validBranch = true; break;
            case 0xC: inst.type = OpType::BGE; validBranch = true; break;
            case 0xD: inst.type = OpType::BLT; validBranch = true; break;
            case 0xE: inst.type = OpType::BGT; validBranch = true; break;
            case 0xF: inst.type = OpType::BLE; validBranch = true; break;
            default: break;
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

    // 22. Condition Setter Detection (Scc)
    if ((opcode & 0xF0C0) == 0x50C0) {
        inst.type = OpType::SCC;
        inst.size = OperandSize::BYTE; 
        inst.immediateData = (opcode >> 8) & 0x0F; 

        Byte destMode = (opcode >> 3) & 0x07;
        Byte destReg  = opcode & 0x07;
        inst.destMode     = ParseAddressingMode(destMode, destReg);
        inst.destRegister = destReg;
        return inst;
    }

    // 17. ADD / ADDA / ADDX Detection
    if ((opcode & 0xF000) == 0xD000) {
        if ((opcode & 0xF130) == 0xD100) {
            inst.type = OpType::ADDX;
            Byte sizeBits = (opcode >> 6) & 0x3;
            if (sizeBits == 0x0)      inst.size = OperandSize::BYTE;
            else if (sizeBits == 0x1) inst.size = OperandSize::WORD;
            else                      inst.size = OperandSize::LONG;

            bool isPredec = (opcode & 0x0008) != 0;
            if (isPredec) {
                inst.srcMode = AddressingMode::AddressRegisterPredecrement;
                inst.destMode = AddressingMode::AddressRegisterPredecrement;
            } else {
                inst.srcMode = AddressingMode::DataRegisterDirect;
                inst.destMode = AddressingMode::DataRegisterDirect;
            }
            inst.srcRegister = opcode & 0x7;
            inst.destRegister = (opcode >> 9) & 0x7;
            return inst;
        }

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

    // 18. SUB / SUBA / SUBX Detection
    if ((opcode & 0xF000) == 0x9000) {
        if ((opcode & 0xF130) == 0x9100) {
            inst.type = OpType::SUBX;
            Byte sizeBits = (opcode >> 6) & 0x3;
            if (sizeBits == 0x0)      inst.size = OperandSize::BYTE;
            else if (sizeBits == 0x1) inst.size = OperandSize::WORD;
            else                      inst.size = OperandSize::LONG;

            bool isPredec = (opcode & 0x0008) != 0;
            if (isPredec) {
                inst.srcMode = AddressingMode::AddressRegisterPredecrement;
                inst.destMode = AddressingMode::AddressRegisterPredecrement;
            } else {
                inst.srcMode = AddressingMode::DataRegisterDirect;
                inst.destMode = AddressingMode::DataRegisterDirect;
            }
            inst.srcRegister = opcode & 0x7;
            inst.destRegister = (opcode >> 9) & 0x7;
            return inst;
        }

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

    // 20. AND, EXG, MULU & MULS Detection
    if ((opcode & 0xF000) == 0xC000) {
        // Check for EXG instructions first to prevent collisions with AND
        // 1. EXG Dn, Dm (opmode 01000)
        if ((opcode & 0xF130) == 0xC140) {
            inst.type = OpType::EXG;
            inst.size = OperandSize::LONG;
            inst.srcMode = AddressingMode::DataRegisterDirect;
            inst.srcRegister = opcode & 0x7; 
            inst.destMode = AddressingMode::DataRegisterDirect;
            inst.destRegister = (opcode >> 9) & 0x7; 
            return inst;
        }
        // 2. EXG An, Am (opmode 01001)
        if ((opcode & 0xF130) == 0xC148) {
            inst.type = OpType::EXG;
            inst.size = OperandSize::LONG;
            inst.srcMode = AddressingMode::AddressRegisterDirect;
            inst.srcRegister = opcode & 0x7; 
            inst.destMode = AddressingMode::AddressRegisterDirect;
            inst.destRegister = (opcode >> 9) & 0x7; 
            return inst;
        }
        // 3. EXG Dn, An (opmode 10001)
        if ((opcode & 0xF130) == 0xC188) {
            inst.type = OpType::EXG;
            inst.size = OperandSize::LONG;
            inst.srcMode = AddressingMode::DataRegisterDirect;
            inst.srcRegister = (opcode >> 9) & 0x7; 
            inst.destMode = AddressingMode::AddressRegisterDirect;
            inst.destRegister = opcode & 0x7; 
            return inst;
        }

        Byte opmode = (opcode >> 6) & 0x7;
        
        // MULU.W / MULS.W (opmode 0x3 / 0x7)
        if (opmode == 0x3 || opmode == 0x7) {
            inst.type = (opmode == 0x3) ? OpType::MULU : OpType::MULS;
            inst.size = OperandSize::WORD;
            Byte srcMode = (opcode >> 3) & 0x7;
            Byte srcReg  = opcode & 0x7;
            inst.srcMode = ParseAddressingMode(srcMode, srcReg);
            inst.srcRegister = srcReg;
            inst.destMode = AddressingMode::DataRegisterDirect;
            inst.destRegister = (opcode >> 9) & 0x7;
            return inst;
        }

        // Standard AND
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

    // 20.1. OR, DIVU, DIVS & CMPM Detection
    if ((opcode & 0xF000) == 0x8000) {
        Byte opmode = (opcode >> 6) & 0x7;
        
        // DIVU.W / DIVS.W (opmode 0x3 / 0x7)
        if (opmode == 0x3 || opmode == 0x7) {
            inst.type = (opmode == 0x3) ? OpType::DIVU : OpType::DIVS;
            inst.size = OperandSize::WORD;
            Byte srcMode = (opcode >> 3) & 0x7;
            Byte srcReg  = opcode & 0x7;
            inst.srcMode = ParseAddressingMode(srcMode, srcReg);
            inst.srcRegister = srcReg;
            inst.destMode = AddressingMode::DataRegisterDirect;
            inst.destRegister = (opcode >> 9) & 0x7;
            return inst;
        }

        inst.type = OpType::OR;
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

    if ((opcode & 0xF100) == 0xB100) {
        Byte opmode = (opcode >> 6) & 0x07;
        Byte destMode = (opcode >> 3) & 0x07;
        Byte destReg  = opcode & 0x07;

        if (opmode == 0x4 || opmode == 0x5 || opmode == 0x6) {
            // EOR opcode overlaps with CMPM. 
            // If the destination effective address mode is 001 (Postincrement), it's CMPM!
            if (destMode == 0x01) {
                inst.type = OpType::CMP; // Map to standard CMP execution
                inst.size = (opmode == 0x4) ? OperandSize::BYTE :
                            (opmode == 0x5) ? OperandSize::WORD : OperandSize::LONG;
                
                // CMPM (Ay)+, (Ax)+
                inst.srcMode = AddressingMode::AddressRegisterPostincrement;
                inst.srcRegister = destReg; 
                
                inst.destMode = AddressingMode::AddressRegisterPostincrement;
                inst.destRegister = (opcode >> 9) & 0x07; 
                return inst;
            } else {
                inst.type = OpType::EOR;
                inst.size = (opmode == 0x4) ? OperandSize::BYTE :
                            (opmode == 0x5) ? OperandSize::WORD : OperandSize::LONG;

                inst.srcMode = AddressingMode::DataRegisterDirect;
                inst.srcRegister = (opcode >> 9) & 0x07;

                inst.destMode = ParseAddressingMode(destMode, destReg);
                inst.destRegister = destReg;
                return inst;
            }
        }
    }

    // 20.6. LEA / PEA / CMPA Fix - Ensuring Broad Masks
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
            inst.immediateData = opcode; 
            Byte eaReg = opcode & 0x7;
            AddressingMode resolvedEA = ParseAddressingMode(eaMode, eaReg);
            
            bool isLoad = (opcode & 0x0400) != 0;
            if (isLoad) {
                inst.srcMode = resolvedEA;
                inst.srcRegister = eaReg;
                inst.destMode = AddressingMode::Immediate; 
            } else {
                inst.srcMode = AddressingMode::Immediate; 
                inst.destMode = resolvedEA;
                inst.destRegister = eaReg;
            }
            return inst;
        }
    }

    // Broadened CMP family masking logic
    if ((opcode & 0xF000) == 0xB000) {
        Byte opmode = (opcode >> 6) & 0x7;
        if (opmode == 0x3 || opmode == 0x7) { // CMPA
            inst.type = OpType::CMP;
            inst.size = (opmode == 0x3) ? OperandSize::WORD : OperandSize::LONG;
            Byte srcMode = (opcode >> 3) & 0x7;
            Byte srcReg  = opcode & 0x7;
            inst.srcMode = ParseAddressingMode(srcMode, srcReg);
            inst.srcRegister = srcReg;
            inst.destMode = AddressingMode::AddressRegisterDirect;
            inst.destRegister = (opcode >> 9) & 0x7;
            return inst;
        } else { // Standard CMP
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

    // 21. Standard MOVE & MOVEA Detection
    if ((opcode & 0xC000) == 0x0000 && (opcode & 0x3000) != 0x0000) {
        inst.type = OpType::MOVE;

        Word sizeBits = (opcode >> 12) & 0x3;
        if (sizeBits == 0x01)      inst.size = OperandSize::BYTE;
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
        case 0x6: return AddressingMode::AddressRegisterIndex;

        case 0x7: 
            if (regBits == 0x0) return AddressingMode::AbsoluteShort; 
            if (regBits == 0x1) return AddressingMode::AbsoluteLong; 
            if (regBits == 0x2) return AddressingMode::ProgramCounterDisplacement; 
            if (regBits == 0x3) return AddressingMode::ProgramCounterIndex; 
            if (regBits == 0x4) return AddressingMode::Immediate; 
            return AddressingMode::Immediate;

        default:
            return AddressingMode::Immediate;
    }
}

} // namespace GenesisEmu::Core::Domain::M68k