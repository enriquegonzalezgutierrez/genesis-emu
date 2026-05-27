// ==============================================================================
// GenesisEmu - M68k Processor Orchestrator Implementation (Core Domain)
// ==============================================================================
// This file implements the Fetch-Decode-Execute pipeline, exception stack
// configurations for interrupts, and delegates executions to modular Executors.
// ==============================================================================

#include "M68k.h"
#include "M68kDecoder.h"
#include <iostream>
#include <iomanip>

// Include modular execution subdomains (The OCP Executors)
#include "Executors/MoveExecutor.h"
#include "Executors/ArithmeticExecutor.h"
#include "Executors/LogicalExecutor.h"
#include "Executors/FlowExecutor.h"
#include "Executors/BitExecutor.h"
#include "Executors/ShiftExecutor.h"

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

    // Reset instruction history tracking
    m_instructionHistory.fill({0, 0});
    m_historyIndex = 0;

    // --- Boot Diagnostic Logging ---
    std::cout << "[CPU RESET] Vector table parsed successfully: "
              << "Initial SP: 0x" << std::hex << std::uppercase << m_registers.ReadA(7)
              << " | Initial PC: 0x" << m_registers.GetPC() << std::dec << std::endl;
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

        // --- Hardware Interrupt Diagnostic Logging ---
        std::cout << "[CPU INTERRUPT] Servicing Level " << level << " Hardware Event"
                  << " (Old Mask: " << (int)currentMask << " -> Elevating to: " << level << ")" << std::endl;
        
        // 1. Call Exception sequence FIRST. This pushes the *current* (old) SR and PC 
        // onto the supervisor stack before modifying the interrupt mask.
        Exception(24 + level); // Auto-vectors trigger exceptions 25 to 31
        
        // 2. NOW elevate the CPU interrupt mask to the active level to prevent
        // nested interrupts of the same or lower priority.
        Word sr = m_registers.GetSR();
        sr = (sr & ~0x0700) | (static_cast<Word>(level) << 8); 
        m_registers.SetSR(sr);
    }
}

// ------------------------------------------------------------------------------
// Hardware Exception Vector Handler
// ------------------------------------------------------------------------------
void M68k::Exception(int vector) {
    // --- Dump Instruction Queue Leading Up to Crash ---
    DumpExecutionHistory();

    Address vectorAddress = m_bus->ReadLongword(vector * 4);
    Longword sp = m_registers.ReadA(7);

    // --- Hardware Trap Diagnostic Logging ---
    std::string exceptionTypeName = "UNKNOWN EXCEPTION";
    if (vector == 2) exceptionTypeName = "Bus Error / Access Violation";
    else if (vector == 3) exceptionTypeName = "Address Alignment Error";
    else if (vector == 4) exceptionTypeName = "Illegal Instruction / Opcode Trap";
    else if (vector == 5) exceptionTypeName = "Division by Zero Exception";
    else if (vector == 10) exceptionTypeName = "Line 1010 Emulator (Line A)";
    else if (vector == 11) exceptionTypeName = "Line 1111 Emulator (Line F)";
    else if (vector >= 25 && vector <= 31) exceptionTypeName = "Auto-Vectored Interrupt";

    std::cout << "[CPU EXCEPTION] Hardware Trap Triggered: " << exceptionTypeName
              << " (Vector: " << vector << " at 0x" << std::hex << std::uppercase << (vector * 4) << ")"
              << " | Target Handler: 0x" << vectorAddress
              << " | Trigger PC: 0x" << m_registers.GetPC()
              << " | Stack Pointer (SP): 0x" << sp << std::dec << std::endl;
    
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
// Instruction Logging Machinery
// ------------------------------------------------------------------------------
void M68k::RecordInstruction(Address pc, Word opcode) {
    m_instructionHistory[m_historyIndex] = {pc, opcode};
    m_historyIndex = (m_historyIndex + 1) % m_instructionHistory.size();
}

void M68k::DumpExecutionHistory() {
    std::cout << "\n=== [M68K INSTRUCTION EXECUTION HISTORY - LAST 32 INSTRUCTIONS] ===" << std::endl;
    for (std::size_t i = 0; i < m_instructionHistory.size(); ++i) {
        std::size_t idx = (m_historyIndex + i) % m_instructionHistory.size();
        if (m_instructionHistory[idx].pc != 0) {
            std::cout << "  PC: 0x" << std::hex << std::uppercase << m_instructionHistory[idx].pc
                      << " | Opcode: 0x" << std::setw(4) << std::setfill('0') << m_instructionHistory[idx].opcode 
                      << std::dec << std::endl;
        }
    }
    std::cout << "===================================================================\n" << std::endl;
}

// ------------------------------------------------------------------------------
// Dynamic Execution Router
// ------------------------------------------------------------------------------
int M68k::Step() {
    if (m_halted) {
        return 4; 
    }

    Address instructionPC = m_registers.GetPC();
    
    // --- TEMPORARY DIAGNOSTIC: VBLANK HANDLER DISASSEMBLY ---
    if (instructionPC >= 0x1B246E && instructionPC < 0x1B24EC) {
        Word opcode = m_bus->ReadWord(instructionPC);
        std::cout << "[HANDLER DEBUG] PC: 0x" << std::hex << std::uppercase << instructionPC 
                  << " | Opcode: 0x" << opcode << std::dec << std::endl;
    }
    // --------------------------------------------------------
    
    Word opcode = FetchCode();
    
    // Push the context into the rolling log queue
    RecordInstruction(instructionPC, opcode);

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
        case OpType::EXG:
        case OpType::LINK:
        case OpType::UNLK:
            return Executors::MoveExecutor::Execute(inst, *this, m_bus, opcode);

        // --- 2. Arithmetic Operations ---
        case OpType::ADD:
        case OpType::ADDQ:
        case OpType::ADDX:
        case OpType::SUB:
        case OpType::SUBQ:
        case OpType::SUBX:
        case OpType::NEG:   // <--- Route NEG to ArithmeticExecutor
        case OpType::NEGX:  // <--- Route NEGX to ArithmeticExecutor
        case OpType::CMP:
        case OpType::CMPI:
        case OpType::TST:
        case OpType::MULU:
        case OpType::MULS:
        case OpType::DIVU:
        case OpType::DIVS:
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
        case OpType::DBCC:
        case OpType::SCC:
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