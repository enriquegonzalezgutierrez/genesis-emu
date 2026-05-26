// ==============================================================================
// GenesisEmu - Motorola 68000 CPU Implementation (Core Domain)
// ==============================================================================
// This file implements the main M68k CPU execution loops.
// Added debug diagnostic printing for address 0x0216.
// ==============================================================================

#include "M68k.h"
#include "M68kDecoder.h"
#include "M68kArithmetic.h"
#include "M68kFlowControl.h"
#include <iostream>

namespace GenesisEmu::Core {

M68k::M68k(IBus* bus) 
    : m_bus(bus), m_pc(0), m_sr(0x2700), m_usp(0), m_halted(false) {
    
    // Clear all registers on cold boot
    for (int i = 0; i < 8; ++i) {
        m_d[i] = 0;
        m_a[i] = 0;
    }
}

// ------------------------------------------------------------------------------
// CPU Lifecycle
// ------------------------------------------------------------------------------
void M68k::Reset() {
    m_a[7] = m_bus->ReadLongword(0x000000);
    m_pc = m_bus->ReadLongword(0x000004);
    m_sr = 0x2700;
    m_usp = 0;
    m_halted = false;
}

Word M68k::FetchCode() {
    Word opcode = m_bus->ReadWord(m_pc);
    m_pc += 2;
    return opcode;
}

// ------------------------------------------------------------------------------
// Main Execution Step
// ------------------------------------------------------------------------------
int M68k::Step() {
    if (m_halted) {
        return 4;
    }

    Address instructionPC = m_pc;

    // Fetch and Decode
    Word opcode = FetchCode();
    DecodedInstruction inst = M68kDecoder::Decode(opcode);

    // --- 0x0216 TARGET DIAGNOSTIC PRINT ---
    if (instructionPC == 0x0216) {
        std::cout << "\n----------------------------------------------------" << std::endl;
        std::cout << "[DEBUG 0x0216] Rom Instruction Diagnostic Pull:" << std::endl;
        std::cout << "Raw Opcode:  0x" << std::hex << std::uppercase << opcode << std::endl;
        std::cout << "DecodedType: " << static_cast<int>(inst.type) << std::endl;
        std::cout << "SrcMode:     " << static_cast<int>(inst.srcMode) << " (Reg: " << static_cast<int>(inst.srcRegister) << ")" << std::endl;
        std::cout << "DestMode:    " << static_cast<int>(inst.destMode) << " (Reg: " << static_cast<int>(inst.destRegister) << ")" << std::endl;
        std::cout << "Size:        " << static_cast<int>(inst.size) << std::endl;
        std::cout << "----------------------------------------------------\n" << std::dec << std::endl;
    }

    switch (inst.type) {
        case OpType::NOP:
            return 4;

        case OpType::JMP: {
            if (inst.destMode == AddressingMode::AbsoluteLong) {
                Word highWord = FetchCode();
                Word lowWord  = FetchCode();
                Address targetAddress = (static_cast<Longword>(highWord) << 16) | lowWord;
                m_pc = targetAddress;
                return 16;
            }
            std::cerr << "M68k Error: Unhandled addressing mode for JMP at " 
                      << "0x" << std::hex << instructionPC << std::endl;
            return 4;
        }

        case OpType::BRA:
        case OpType::BNE:
        case OpType::BEQ:
        case OpType::BPL:
        case OpType::BMI: {
            bool takeBranch = false;

            if (inst.type == OpType::BRA) {
                takeBranch = true; 
            } else if (inst.type == OpType::BNE) {
                takeBranch = !GetFlagZero(); 
            } else if (inst.type == OpType::BEQ) {
                takeBranch = GetFlagZero();  
            } else if (inst.type == OpType::BPL) {
                takeBranch = !GetFlagNegative(); 
            } else if (inst.type == OpType::BMI) {
                takeBranch = GetFlagNegative();  
            }

            if (inst.size == OperandSize::WORD) {
                std::int16_t displacement = static_cast<std::int16_t>(FetchCode());

                if (takeBranch) {
                    m_pc = (instructionPC + 2) + displacement;
                    return 10; 
                } else {
                    return 8;
                }
            }

            std::cerr << "M68k Error: Unhandled size for Branch at " 
                      << "0x" << std::hex << instructionPC << std::endl;
            return 4;
        }

        case OpType::JSR: {
            if (inst.destMode == AddressingMode::AbsoluteLong) {
                Word highWord = FetchCode();
                Word lowWord  = FetchCode();
                Address targetAddress = (static_cast<Longword>(highWord) << 16) | lowWord;
                
                return M68kFlowControl::ExecuteJSR(m_bus, m_pc, m_a[7], targetAddress);
            }
            std::cerr << "M68k Error: Unhandled addressing mode for JSR" << std::endl;
            return 4;
        }

        case OpType::BSR: {
            if (inst.size == OperandSize::WORD) {
                std::int16_t displacement = static_cast<std::int16_t>(FetchCode());
                return M68kFlowControl::ExecuteBSR(m_bus, m_pc, m_a[7], displacement, instructionPC);
            }
            std::cerr << "M68k Error: Unhandled size for BSR" << std::endl;
            return 4;
        }

        case OpType::RTS: {
            return M68kFlowControl::ExecuteRTS(m_bus, m_pc, m_a[7]);
        }

        case OpType::MOVE_TO_SR: {
            Word val = 0;
            if (inst.srcMode == AddressingMode::Immediate) {
                val = FetchCode();
            } else {
                std::cerr << "M68k Error: Unhandled src mode for MOVE_TO_SR" << std::endl;
                return 4;
            }
            SetSR(val);
            return 12; 
        }

        case OpType::MOVE_USP: {
            bool directionToUsp = (opcode & 0x0008) != 0;
            int regIndex = opcode & 0x0007;

            if (directionToUsp) {
                m_usp = GetARegister(regIndex);
            } else {
                SetARegister(regIndex, m_usp);
            }
            return 4; 
        }

        case OpType::ADD: {
            if (inst.srcMode == AddressingMode::DataRegisterDirect &&
                inst.destMode == AddressingMode::DataRegisterDirect) {
                
                Longword srcVal  = GetDRegister(inst.srcRegister);
                Longword destVal = GetDRegister(inst.destRegister);
                Longword result = M68kArithmetic::ExecuteADD(destVal, srcVal, inst.size, m_sr);

                if (inst.size == OperandSize::WORD) {
                    SetDRegister(inst.destRegister, (destVal & 0xFFFF0000) | (result & 0xFFFF));
                } else if (inst.size == OperandSize::BYTE) {
                    SetDRegister(inst.destRegister, (destVal & 0xFFFFFF00) | (result & 0xFF));
                } else if (inst.size == OperandSize::LONG) {
                    SetDRegister(inst.destRegister, result);
                }
                return 4; 
            }
            return 4;
        }

        case OpType::SUB: {
            // --- SUBA branch ---
            if (inst.destMode == AddressingMode::AddressRegisterDirect) {
                Longword srcVal = 0;
                
                if (inst.srcMode == AddressingMode::Immediate) {
                    if (inst.size == OperandSize::LONG) {
                        Word hi = FetchCode();
                        Word lo = FetchCode();
                        srcVal = (static_cast<Longword>(hi) << 16) | lo;
                    } else {
                        std::int16_t val16 = static_cast<std::int16_t>(FetchCode());
                        srcVal = static_cast<Longword>(static_cast<std::int32_t>(val16));
                    }
                } else {
                    std::cerr << "M68k Error: Unhandled src mode for SUBA at " 
                              << std::hex << instructionPC << std::endl;
                    return 4;
                }

                Longword destVal = GetARegister(inst.destRegister);
                SetARegister(inst.destRegister, destVal - srcVal);

                return (inst.size == OperandSize::LONG) ? 12 : 8;
            }

            // --- Standard SUB branch ---
            if (inst.srcMode == AddressingMode::DataRegisterDirect &&
                inst.destMode == AddressingMode::DataRegisterDirect) {
                
                Longword srcVal  = GetDRegister(inst.srcRegister);
                Longword destVal = GetDRegister(inst.destRegister);
                Longword result = M68kArithmetic::ExecuteSUB(destVal, srcVal, inst.size, m_sr);

                if (inst.size == OperandSize::WORD) {
                    SetDRegister(inst.destRegister, (destVal & 0xFFFF0000) | (result & 0xFFFF));
                } else if (inst.size == OperandSize::BYTE) {
                    SetDRegister(inst.destRegister, (destVal & 0xFFFFFF00) | (result & 0xFF));
                } else if (inst.size == OperandSize::LONG) {
                    SetDRegister(inst.destRegister, result);
                }
                return 4; 
            }
            return 4;
        }

        case OpType::AND: {
            if (inst.srcMode == AddressingMode::DataRegisterDirect &&
                inst.destMode == AddressingMode::DataRegisterDirect) {
                
                Longword srcVal  = GetDRegister(inst.srcRegister);
                Longword destVal = GetDRegister(inst.destRegister);
                Longword result = M68kArithmetic::ExecuteAND(destVal, srcVal, inst.size, m_sr);

                if (inst.size == OperandSize::WORD) {
                    SetDRegister(inst.destRegister, (destVal & 0xFFFF0000) | (result & 0xFFFF));
                } else if (inst.size == OperandSize::BYTE) {
                    SetDRegister(inst.destRegister, (destVal & 0xFFFFFF00) | (result & 0xFF));
                } else if (inst.size == OperandSize::LONG) {
                    SetDRegister(inst.destRegister, result);
                }
                return 4; 
            }
            return 4;
        }

        case OpType::MOVE: {
            Longword value = 0; 
            bool updateFlags = true; 
            int extraCycles = 0; 

            // --- Read Source Operand ---
            if (inst.srcMode == AddressingMode::DataRegisterDirect) {
                if (inst.size == OperandSize::LONG) {
                    value = GetDRegister(inst.srcRegister);
                } else {
                    value = GetDRegister(inst.srcRegister) & 0xFFFF;
                }
            } 
            else if (inst.srcMode == AddressingMode::Immediate) {
                if (inst.size == OperandSize::LONG) {
                    Word hi = FetchCode();
                    Word lo = FetchCode();
                    value = (static_cast<Longword>(hi) << 16) | lo;
                    extraCycles = 8;
                } else {
                    value = FetchCode();
                    extraCycles = 4; 
                }
            }
            else if (inst.srcMode == AddressingMode::AddressRegisterPostincrement) {
                Address targetAddress = GetARegister(inst.srcRegister);
                
                if (inst.size == OperandSize::LONG) {
                    value = m_bus->ReadLongword(targetAddress);
                } else {
                    value = m_bus->ReadWord(targetAddress);
                }
                
                int increment = 2; 
                if (inst.size == OperandSize::BYTE) increment = 1;
                else if (inst.size == OperandSize::LONG) increment = 4;
                
                if (inst.srcRegister == 7 && increment == 1) {
                    increment = 2;
                }
                SetARegister(inst.srcRegister, targetAddress + increment);
                extraCycles = (inst.size == OperandSize::LONG) ? 8 : 4; 
            }
            else {
                std::cerr << "M68k Error: Unhandled source mode for MOVE at " 
                      << "0x" << std::hex << instructionPC << std::endl;
                return 4;
            }

            // --- Write Destination Operand & Calculate Base Cycles ---
            int baseCycles = 4;
            if (inst.destMode == AddressingMode::DataRegisterDirect) {
                Longword currentDest = GetDRegister(inst.destRegister);
                if (inst.size == OperandSize::LONG) {
                    SetDRegister(inst.destRegister, value);
                } else {
                    Longword updatedDest = (currentDest & 0xFFFF0000) | (value & 0xFFFF);
                    SetDRegister(inst.destRegister, updatedDest);
                }
                baseCycles = 4; 
            } 
            else if (inst.destMode == AddressingMode::AddressRegisterDirect) {
                updateFlags = false; 

                if (inst.size == OperandSize::WORD) {
                    std::int16_t signedValue = static_cast<std::int16_t>(value);
                    Longword signExtendedValue = static_cast<Longword>(static_cast<std::int32_t>(signedValue));
                    SetARegister(inst.destRegister, signExtendedValue);
                } else if (inst.size == OperandSize::LONG) {
                    SetARegister(inst.destRegister, value);
                }
                baseCycles = 4; 
            }
            else if (inst.destMode == AddressingMode::AddressRegisterIndirect) {
                Address targetAddress = GetARegister(inst.destRegister);
                if (inst.size == OperandSize::LONG) {
                    m_bus->WriteLongword(targetAddress, value);
                    baseCycles = 12;
                } else {
                    m_bus->WriteWord(targetAddress, value & 0xFFFF);
                    baseCycles = 8; 
                }
            } 
            else if (inst.destMode == AddressingMode::AbsoluteShort) {
                std::int16_t shortAddr = static_cast<std::int16_t>(FetchCode());
                Address targetAddress = static_cast<Address>(static_cast<std::int32_t>(shortAddr));

                if (inst.size == OperandSize::LONG) {
                    m_bus->WriteLongword(targetAddress, value);
                    baseCycles = 12;
                } else {
                    m_bus->WriteWord(targetAddress, value & 0xFFFF);
                    baseCycles = 8;
                }
            }
            else if (inst.destMode == AddressingMode::AbsoluteLong) {
                Word highWord = FetchCode();
                Word lowWord  = FetchCode();
                Address targetAddress = (static_cast<Longword>(highWord) << 16) | lowWord;

                if (inst.size == OperandSize::LONG) {
                    m_bus->WriteLongword(targetAddress, value);
                    baseCycles = 16;
                } else {
                    m_bus->WriteWord(targetAddress, value & 0xFFFF);
                    baseCycles = 12;
                }
            }
            else {
                std::cerr << "M68k Error: Unhandled destination mode for MOVE at " 
                      << "0x" << std::hex << instructionPC << std::endl;
                return 4;
            }

            if (updateFlags) {
                m_sr &= ~0x0003; 
                
                Longword checkValue = (inst.size == OperandSize::LONG) ? value : (value & 0xFFFF);
                Longword msbCheck = (inst.size == OperandSize::LONG) ? 0x80000000 : 0x8000;

                if (checkValue == 0) m_sr |= 0x0004;
                else                 m_sr &= ~0x0004;

                if ((checkValue & msbCheck) != 0) m_sr |= 0x0008;
                else                              m_sr &= ~0x0008;
            }

            return baseCycles + extraCycles;
        }

        default: {
            m_halted = true;
            
            std::cerr << "\n====================================================" << std::endl;
            std::cerr << "[CPU DIAGNOSTIC HALT] Unhandled Opcode encountered!" << std::endl;
            std::cerr << "====================================================" << std::endl;
            std::cerr << "PC:         0x" << std::hex << std::uppercase << instructionPC << std::endl;
            std::cerr << "Opcode:     0x" << opcode << std::endl;
            std::cerr << "Status Reg: 0x" << m_sr << std::endl;
            std::cerr << "----------------------------------------------------" << std::endl;
            for (int i = 0; i < 8; ++i) {
                std::cerr << "D" << i << ": 0x" << m_d[i] << "   A" << i << ": 0x" << m_a[i] << std::endl;
            }
            std::cerr << "====================================================\n" << std::dec << std::endl;
            return 4;
        }
    }
}

} // namespace GenesisEmu::Core