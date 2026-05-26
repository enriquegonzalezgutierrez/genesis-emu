// ==============================================================================
// GenesisEmu - Motorola 68000 CPU Implementation
// ==============================================================================
// This file implements the M68k CPU state transitions, the Reset vector sequence,
// and the primary instruction decoding step.
// ==============================================================================

#include "M68k.h"
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
    // Fetch the 16-bit instruction word
    Word opcode = FetchCode();

    // Decode the instruction
    switch (opcode) {
        case 0x4E71: // NOP (No Operation)
            // NOP does absolutely nothing but consume 4 clock cycles
            return 4;

        default:
            // Unhandled/illegal opcode fallback.
            // In a complete emulator, this triggers an Illegal Instruction Exception.
            // For now, we print a warning, consume default cycles, and continue.
            std::cerr << "M68k Warning: Unhandled Opcode " 
                      << "0x" << std::hex << opcode << " at Address " 
                      << "0x" << m_pc - 2 << std::endl;
            return 4; 
    }
}

} // namespace GenesisEmu::Core