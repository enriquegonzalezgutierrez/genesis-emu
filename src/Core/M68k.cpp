// ==============================================================================
// GenesisEmu - Motorola 68000 CPU Implementation (Core Domain)
// ==============================================================================
// This file implements the main M68k CPU execution loops, decoding opcodes and
// delegating mathematical operations to specialized execution units.
// ==============================================================================

#include "M68k.h"
#include "M68kDecoder.h"
#include "M68kArithmetic.h"
#include <iostream>

namespace GenesisEmu::Core {

M68k::M68k(IBus* bus) 
    : m_bus(bus), m_pc(0), m_sr(0x2700) {
    
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
    // Vector 0 (Address $000000): Initial Stack Pointer (SSP)
    m_a[7] = m_bus->ReadLongword(0x000000);

    // Vector 1 (Address $000004): Initial Program Counter (PC)
    m_pc = m_bus->ReadLongword(0x000004);

    // Default Status Register value on Reset (Supervisor Mode, Interrupts Masked)
    m_sr = 0x2700;
}

// ------------------------------------------------------------------------------
// Instruction Pipeline Helpers
// ------------------------------------------------------------------------------
Word M68k::FetchCode() {
    Word opcode = m_bus->ReadWord(m_pc);
    m_pc += 2;
    return opcode;
}

// ------------------------------------------------------------------------------
// Main Execution Step (Decode & Execute)
// ------------------------------------------------------------------------------
int M68k::Step() {
    // Calculate the Program Counter of the current instruction (before Fetch advances it)
    Address instructionPC = m_pc;

    // 1. Fetch the 16-bit instruction word from memory
    Word opcode = FetchCode();

    // 2. Decode the raw opcode into structured metadata
    DecodedInstruction inst = M68kDecoder::Decode(opcode);

    // 3. Execute based on decoded instruction type
    switch (inst.type) {
        case OpType::NOP:
            // NOP does nothing but consume 4 clock cycles
            return 4;

        case OpType::JMP: {
            // Jump Instruction Implementation
            if (inst.destMode == AddressingMode::AbsoluteLong) {
                // JMP (xxx).L: Target address is stored in 2 extension words (32-bit)
                Word highWord = FetchCode();
                Word lowWord  = FetchCode();
                
                // Combine into a single 32-bit physical address
                Address targetAddress = (static_cast<Longword>(highWord) << 16) | lowWord;
                
                // Redirect CPU execution flow directly to the target address
                m_pc = targetAddress;
                
                // JMP (xxx).L takes exactly 16 clock cycles
                return 16;
            }

            // Fallback for unhandled addressing modes of JMP
            std::cerr << "M68k Error: Unhandled addressing mode for JMP at " 
                      << "0x" << std::hex << instructionPC << std::endl;
            return 4;
        }

        case OpType::BRA:
        case OpType::BNE:
        case OpType::BEQ:
        case OpType::BPL:
        case OpType::BMI: {
            // Branch Family (Relative Jumps) Unified Engine
            bool takeBranch = false;

            // Evaluate branch condition based on decoded instruction type and CCR flags
            if (inst.type == OpType::BRA) {
                takeBranch = true; // BRA: Branch Always
            } else if (inst.type == OpType::BNE) {
                takeBranch = !GetFlagZero(); // BNE: Branch if Z flag is 0
            } else if (inst.type == OpType::BEQ) {
                takeBranch = GetFlagZero();  // BEQ: Branch if Z flag is 1
            } else if (inst.type == OpType::BPL) {
                takeBranch = !GetFlagNegative(); // BPL: Branch if N flag is 0
            } else if (inst.type == OpType::BMI) {
                takeBranch = GetFlagNegative();  // BMI: Branch if N flag is 1
            }

            if (inst.size == OperandSize::WORD) {
                // Read 16-bit signed displacement from extension word (PC + 2)
                std::int16_t displacement = static_cast<std::int16_t>(FetchCode());

                if (takeBranch) {
                    // Branch Taken: target address = (Instruction PC + 2) + displacement
                    m_pc = (instructionPC + 2) + displacement;
                    return 10; // Bcc.W taken takes exactly 10 clock cycles
                } else {
                    // Branch Not Taken: PC simply stays past the extension word
                    // Bcc.W not taken takes exactly 8 clock cycles
                    return 8;
                }
            }

            std::cerr << "M68k Error: Unhandled size for Branch at " 
                      << "0x" << std::hex << instructionPC << std::endl;
            return 4;
        }

        case OpType::ADD: {
            // Register-to-Register ADD implementation delegated to M68kArithmetic unit
            if (inst.srcMode == AddressingMode::DataRegisterDirect &&
                inst.destMode == AddressingMode::DataRegisterDirect) {
                
                Longword srcVal  = GetDRegister(inst.srcRegister);
                Longword destVal = GetDRegister(inst.destRegister);
                
                Longword result = M68kArithmetic::ExecuteADD(destVal, srcVal, inst.size, m_sr);

                // Write result back to destination register preserving high bits depending on size
                if (inst.size == OperandSize::WORD) {
                    SetDRegister(inst.destRegister, (destVal & 0xFFFF0000) | (result & 0xFFFF));
                } else if (inst.size == OperandSize::BYTE) {
                    SetDRegister(inst.destRegister, (destVal & 0xFFFFFF00) | (result & 0xFF));
                } else if (inst.size == OperandSize::LONG) {
                    SetDRegister(inst.destRegister, result);
                }

                return 4; // ADD Dn, Dn takes 4 clock cycles
            }
            return 4;
        }

        case OpType::SUB: {
            // Register-to-Register SUB implementation delegated to M68kArithmetic unit
            if (inst.srcMode == AddressingMode::DataRegisterDirect &&
                inst.destMode == AddressingMode::DataRegisterDirect) {
                
                Longword srcVal  = GetDRegister(inst.srcRegister);
                Longword destVal = GetDRegister(inst.destRegister);
                
                Longword result = M68kArithmetic::ExecuteSUB(destVal, srcVal, inst.size, m_sr);

                // Write result back to destination register preserving high bits depending on size
                if (inst.size == OperandSize::WORD) {
                    SetDRegister(inst.destRegister, (destVal & 0xFFFF0000) | (result & 0xFFFF));
                } else if (inst.size == OperandSize::BYTE) {
                    SetDRegister(inst.destRegister, (destVal & 0xFFFFFF00) | (result & 0xFF));
                } else if (inst.size == OperandSize::LONG) {
                    SetDRegister(inst.destRegister, result);
                }

                return 4; // SUB Dn, Dn takes 4 clock cycles
            }
            return 4;
        }

        case OpType::AND: {
            // Register-to-Register AND implementation delegated to M68kArithmetic unit
            if (inst.srcMode == AddressingMode::DataRegisterDirect &&
                inst.destMode == AddressingMode::DataRegisterDirect) {
                
                Longword srcVal  = GetDRegister(inst.srcRegister);
                Longword destVal = GetDRegister(inst.destRegister);
                
                Longword result = M68kArithmetic::ExecuteAND(destVal, srcVal, inst.size, m_sr);

                // Write result back to destination register preserving high bits depending on size
                if (inst.size == OperandSize::WORD) {
                    SetDRegister(inst.destRegister, (destVal & 0xFFFF0000) | (result & 0xFFFF));
                } else if (inst.size == OperandSize::BYTE) {
                    SetDRegister(inst.destRegister, (destVal & 0xFFFFFF00) | (result & 0xFF));
                } else if (inst.size == OperandSize::LONG) {
                    SetDRegister(inst.destRegister, result);
                }

                return 4; // AND Dn, Dn takes 4 clock cycles
            }
            return 4;
        }

        case OpType::MOVE: {
            Word value = 0;
            bool updateFlags = true; 
            int extraCycles = 0; // Tracks additional memory accesses for source operands

            // --- Read Source Operand ---
            if (inst.srcMode == AddressingMode::DataRegisterDirect) {
                value = static_cast<Word>(GetDRegister(inst.srcRegister) & 0xFFFF);
            } 
            else if (inst.srcMode == AddressingMode::Immediate) {
                // Immediate Mode: Read extension word following the opcode
                value = FetchCode();
                extraCycles = 4; // Fetching immediate data takes 4 extra CPU clock cycles
            }
            else if (inst.srcMode == AddressingMode::AddressRegisterPostincrement) {
                // Address Register Indirect with Postincrement ((An)+)
                Address targetAddress = GetARegister(inst.srcRegister);
                
                // Read 16-bit Word data from the Bus
                value = m_bus->ReadWord(targetAddress);
                
                // Calculate physical register auto-increment based on operand size
                int increment = 2; // Word size is 2 bytes
                if (inst.size == OperandSize::BYTE) increment = 1;
                else if (inst.size == OperandSize::LONG) increment = 4;
                
                // Special hardware rule: Stack Pointer (A7) byte accesses are forced to 2-byte 
                // increment to keep stack strictly word-aligned.
                if (inst.srcRegister == 7 && increment == 1) {
                    increment = 2;
                }

                // Update the Address Register value with the calculated increment
                SetARegister(inst.srcRegister, targetAddress + increment);
                
                extraCycles = 4; // Memory read takes 4 extra clock cycles
            }
            else {
                std::cerr << "M68k Error: Unhandled source mode for MOVE at " 
                      << "0x" << std::hex << instructionPC << std::endl;
                return 4;
            }

            // --- Write Destination Operand & Calculate Base Cycles ---
            int baseCycles = 4;
            if (inst.destMode == AddressingMode::DataRegisterDirect) {
                // Register-to-Register Write (Dn)
                Longword currentDest = GetDRegister(inst.destRegister);
                Longword updatedDest = (currentDest & 0xFFFF0000) | value;
                SetDRegister(inst.destRegister, updatedDest);
                baseCycles = 4; // MOVE Dn, Dn takes 4 cycles
            } 
            else if (inst.destMode == AddressingMode::AddressRegisterDirect) {
                // MOVEA Instruction (Destination is an Address Register An)
                updateFlags = false; // Rule: MOVEA does NOT alter CCR flags

                if (inst.size == OperandSize::WORD) {
                    // Rule: 16-bit Word values are always sign-extended to 32-bit when written to An
                    std::int16_t signedValue = static_cast<std::int16_t>(value);
                    Longword signExtendedValue = static_cast<Longword>(static_cast<std::int32_t>(signedValue));
                    
                    SetARegister(inst.destRegister, signExtendedValue);
                } else if (inst.size == OperandSize::LONG) {
                    // Long moves do not require sign extension (full 32-bit transfer)
                    SetARegister(inst.destRegister, value);
                }
                baseCycles = 4; // MOVEA Dn, An takes 4 cycles
            }
            else if (inst.destMode == AddressingMode::AddressRegisterIndirect) {
                // Address Register Indirect Write ((An))
                Address targetAddress = GetARegister(inst.destRegister);
                
                // Write 16-bit Word data to the Bus
                m_bus->WriteWord(targetAddress, value);
                baseCycles = 8; // MOVE Dn, (An) takes 8 cycles
            } 
            else {
                std::cerr << "M68k Error: Unhandled destination mode for MOVE at " 
                      << "0x" << std::hex << instructionPC << std::endl;
                return 4;
            }

            // --- Update Status Register / Condition Code Register (CCR) ---
            if (updateFlags) {
                // Clear V and C (bits 1 and 0 of SR)
                m_sr &= ~0x0003; 

                // Update Z (bit 2 of SR)
                if (value == 0) {
                    m_sr |= 0x0004;
                } else {
                    m_sr &= ~0x0004;
                }

                // Update N (bit 3 of SR)
                if ((value & 0x8000) != 0) {
                    m_sr |= 0x0008;
                } else {
                    m_sr &= ~0x0008;
                }
            }

            // Final clock cycles = Base execution cycles + extra memory fetch cycles
            return baseCycles + extraCycles;
        }

        default:
            std::cerr << "M68k Warning: Unhandled Instruction " 
                      << "0x" << std::hex << opcode << " at Address " 
                      << "0x" << instructionPC << std::endl;
            return 4; 
    }
}

} // namespace GenesisEmu::Core