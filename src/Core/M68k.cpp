// ==============================================================================
// GenesisEmu - Motorola 68000 CPU Implementation (Updated with Memory MOVE)
// ==============================================================================
// This file implements the M68k CPU execution loops. It handles decoding
// raw opcodes and executing instructions (NOP, register & memory indirect MOVEs).
// ==============================================================================

#include "M68k.h"
#include "M68kDecoder.h"
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
    // 1. Fetch the 16-bit instruction word from memory
    Word opcode = FetchCode();

    // 2. Decode the raw opcode into structured metadata
    DecodedInstruction inst = M68kDecoder::Decode(opcode);

    // 3. Execute based on decoded instruction type
    switch (inst.type) {
        case OpType::NOP:
            // NOP does nothing but consume 4 clock cycles
            return 4;

        case OpType::MOVE: {
            Word value = 0;

            // --- Read Source Operand ---
            if (inst.srcMode == AddressingMode::DataRegisterDirect) {
                value = static_cast<Word>(GetDRegister(inst.srcRegister) & 0xFFFF);
            } else {
                std::cerr << "M68k Error: Unhandled source mode for MOVE at " 
                          << "0x" << std::hex << m_pc - 2 << std::endl;
                return 4;
            }

            // --- Write Destination Operand & Calculate Cycles ---
            int cycles = 4;
            if (inst.destMode == AddressingMode::DataRegisterDirect) {
                // Register-to-Register Write (Dn)
                Longword currentDest = GetDRegister(inst.destRegister);
                Longword updatedDest = (currentDest & 0xFFFF0000) | value;
                SetDRegister(inst.destRegister, updatedDest);
                cycles = 4; // MOVE Dn, Dn takes 4 cycles
            } 
            else if (inst.destMode == AddressingMode::AddressRegisterIndirect) {
                // Address Register Indirect Write ((An))
                // Retrieve the pointer address stored in the Address Register
                Address targetAddress = GetARegister(inst.destRegister);
                
                // Write 16-bit Word data to the Bus
                m_bus->WriteWord(targetAddress, value);
                cycles = 8; // MOVE Dn, (An) takes 8 cycles (4 instruction + 4 bus access)
            } 
            else {
                std::cerr << "M68k Error: Unhandled destination mode for MOVE at " 
                          << "0x" << std::hex << m_pc - 2 << std::endl;
                return 4;
            }

            // --- Update Status Register / Condition Code Register (CCR) ---
            // For MOVE instruction:
            // - V (Overflow) and C (Carry) are always cleared (0)
            // - N (Negative) is set if MSB (bit 15 for Word) of result is 1
            // - Z (Zero) is set if result is 0
            
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

            return cycles;
        }

        default:
            // Unhandled/illegal opcode fallback.
            std::cerr << "M68k Warning: Unhandled Instruction " 
                      << "0x" << std::hex << opcode << " at Address " 
                      << "0x" << m_pc - 2 << std::endl;
            return 4; 
    }
}

} // namespace GenesisEmu::Core