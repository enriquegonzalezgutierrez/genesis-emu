// ==============================================================================
// GenesisEmu - M68k Processor Orchestrator Implementation (Core Domain)
// ==============================================================================
// This file implements the Fetch-Decode-Execute pipeline, exception stack
// configurations for interrupts, and delegates executions to modular Executors.
// ==============================================================================

#include "M68k.h"
#include "M68kDecoder.h"

// Include modular execution subdomains (The OCP Executors)
#include "Executors/MoveExecutor.h"
#include "Executors/ArithmeticExecutor.h"
#include "Executors/LogicalExecutor.h"
#include "Executors/FlowExecutor.h"
#include "Executors/BitExecutor.h"
#include "Executors/ShiftExecutor.h"

#include <iostream>

namespace GenesisEmu::Core::Domain::M68k {

using namespace GenesisEmu::Core::Domain::Common;

M68k::M68k(IBus* bus) 
    : m_bus(bus)
    , m_registers()
    , m_halted(false) 
{}

// ------------------------------------------------------------------------------
// Processor Lifecycle
// ------------------------------------------------------------------------------
void M68k::Reset() {
    m_registers.WriteA(7, m_bus->ReadLongword(0x000000));
    m_registers.SetPC(m_bus->ReadLongword(0x000004));
    m_registers.SetSR(0x2700); 
    m_registers.SetUSP(0);
    m_halted = false;
}

Word M68k::FetchCode() {
    Word opcode = m_bus->ReadWord(m_registers.GetPC());
    m_registers.SetPC(m_registers.GetPC() + 2);
    return opcode;
}

// ------------------------------------------------------------------------------
// Auto-Vectored Interrupt Handler Exception Engine
// ------------------------------------------------------------------------------
void M68k::TriggerInterrupt(int level) {
    if (level < 1 || level > 7) return;
    
    Byte currentMask = (m_registers.GetSR() >> 8) & 0x07;
    
    if (level > currentMask || level == 7) {
        m_halted = false;
        
        Word sr = m_registers.GetSR();
        sr = (sr & ~0x0700) | (static_cast<Word>(level) << 8); 
        m_registers.SetSR(sr);

        Exception(24 + level); // Auto-vectors trigger exceptions 25 to 31
    }
}

// ------------------------------------------------------------------------------
// Hardware Exception Vector Handler
// ------------------------------------------------------------------------------
void M68k::Exception(int vector) {
    Address vectorAddress = m_bus->ReadLongword(vector * 4);
    Longword sp = m_registers.ReadA(7);
    
    sp -= 4;
    m_bus->WriteLongword(sp, m_registers.GetPC());
    
    sp -= 2;
    m_bus->WriteWord(sp, m_registers.GetSR());
    
    m_registers.WriteA(7, sp);
    
    Word sr = m_registers.GetSR();
    sr |= 0x2000; 
    m_registers.SetSR(sr);
    
    m_registers.SetPC(vectorAddress);
}

// ------------------------------------------------------------------------------
// Dynamic Execution Router
// ------------------------------------------------------------------------------
int M68k::Step() {
    if (m_halted) {
        return 4; 
    }

    Address instructionPC = m_registers.GetPC();
    Word opcode = FetchCode();
    DecodedInstruction inst = M68kDecoder::Decode(opcode);

    switch (inst.type) {
        case OpType::NOP:
            return 4;

        // --- 1. Move Operations ---
        case OpType::MOVE:
        case OpType::MOVE_TO_SR:
        case OpType::MOVE_TO_CCR:
        case OpType::MOVE_FROM_SR:
        case OpType::MOVE_USP:
        case OpType::MOVEQ:
        case OpType::MOVEM:
        case OpType::LEA:  
        case OpType::PEA:  
            return Executors::MoveExecutor::Execute(inst, *this, m_bus, opcode);

        // --- 2. Arithmetic Operations ---
        case OpType::ADD:
        case OpType::ADDQ:
        case OpType::ADDX:
        case OpType::SUB:
        case OpType::SUBQ:
        case OpType::SUBX:
        case OpType::CMP:
        case OpType::CMPI:
        case OpType::TST:
            return Executors::ArithmeticExecutor::Execute(inst, *this, m_bus, opcode);

        // --- 3. Logical Operations ---
        case OpType::AND:
        case OpType::OR:
        case OpType::EOR:
        case OpType::ANDI_TO_SR:
        case OpType::ORI_TO_SR:
        case OpType::EORI_TO_SR:
        case OpType::NOT:
        case OpType::CLR:
            return Executors::LogicalExecutor::Execute(inst, *this, m_bus, opcode);

        // --- 4. Flow Control Operations ---
        case OpType::JMP:
        case OpType::JSR:
        case OpType::RTS:
        case OpType::RTE:
        case OpType::BRA:
        case OpType::BCC: case OpType::BCS: case OpType::BEQ: case OpType::BGE:
        case OpType::BGT: case OpType::BHI: case OpType::BLE: case OpType::BLS:
        case OpType::BLT: case OpType::BMI: case OpType::BNE: case OpType::BPL:
        case OpType::BVC: case OpType::BVS:
        case OpType::BSR:
        case OpType::DBF:
            return Executors::FlowExecutor::Execute(inst, *this, m_bus, opcode, instructionPC);

        // --- 5. Bit Manipulation Operations ---
        case OpType::BTST:
        case OpType::BCHG:
        case OpType::BCLR:
        case OpType::BSET:
            return Executors::BitExecutor::Execute(inst, *this, m_bus, opcode);

        // --- 6. Shift and Rotate Operations ---
        case OpType::LSR: case OpType::LSL:
        case OpType::ASR: case OpType::ASL:
        case OpType::ROR: case OpType::ROL:
        case OpType::ROXR: case OpType::ROXL:
        case OpType::SWAP:
        case OpType::EXT:
            return Executors::ShiftExecutor::Execute(inst, *this, m_bus, opcode);

        // --- 7. Safe Unknown Opcode Fallback ---
        default: {
            Byte line = (opcode >> 12) & 0x0F;
            if (line == 0x0A) {
                Exception(10); // Line 1010 Emulator Exception (Vector 10)
            } else if (line == 0x0F) {
                Exception(11); // Line 1111 Emulator Exception (Vector 11)
            } else {
                // To prevent halting the entire game loop upon encountering a poorly decoded
                // or entirely unknown instruction, log it but execute as a standard NOP (4 cycles).
                std::cerr << "[M68k Warning] Unhandled Opcode bypassed (NOP applied) at PC: 0x" 
                          << std::hex << std::uppercase << instructionPC << " | Opcode: 0x" << opcode << std::dec << std::endl;
            }
            return 4;
        }
    }
}

} // namespace GenesisEmu::Core::Domain::M68k