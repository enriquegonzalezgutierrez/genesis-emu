// ==============================================================================
// GenesisEmu - M68k Processor Orchestrator Implementation (Core Domain)
// ==============================================================================
// This file implements the Fetch-Decode-Execute pipeline, deferred exception 
// stack configurations for hardware interrupts, and execution delegation.
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
    , m_pendingInterruptLevel(0)
{}

// ------------------------------------------------------------------------------
// Processor Lifecycle
// ------------------------------------------------------------------------------
void M68k::Reset() {
    // Standard M68k Reset vector reads
    m_registers.WriteA(7, m_bus->ReadLongword(0x000000)); // Initial SSP
    m_registers.SetPC(m_bus->ReadLongword(0x000004));     // Initial PC
    m_registers.SetSR(0x2700); // Supervisor mode, Interrupt mask level 7
    m_registers.SetUSP(0);
    m_halted = false;
    m_pendingInterruptLevel = 0;

    m_instructionHistory.fill({0, 0});
    m_historyIndex = 0;

    std::cout << "[CPU RESET] Vector table parsed successfully. Initial SP: 0x" 
              << std::hex << std::uppercase << m_registers.ReadA(7)
              << " | Initial PC: 0x" << m_registers.GetPC() << std::dec << std::endl;
}

Word M68k::FetchCode() {
    Word opcode = m_bus->ReadWord(m_registers.GetPC());
    m_registers.SetPC(m_registers.GetPC() + 2);
    return opcode;
}

// ------------------------------------------------------------------------------
// Deferred Hardware Interrupt Evaluation
// ------------------------------------------------------------------------------
void M68k::TriggerInterrupt(int level) {
    if (level < 1 || level > 7) return;
    
    // Record the highest requested interrupt line. It will be evaluated
    // before the execution of the NEXT instruction in the pipeline.
    if (level > m_pendingInterruptLevel) {
        m_pendingInterruptLevel = level;
    }
}

int M68k::CheckInterrupts() {
    if (m_pendingInterruptLevel == 0) {
        return 0; // Fast path: No interrupts pending
    }

    Byte currentMask = (m_registers.GetSR() >> 8) & 0x07;

    // Interrupts are only serviced if their priority level is strictly higher 
    // than the current interrupt mask, UNLESS it's a Level 7 Non-Maskable Interrupt (NMI).
    if (m_pendingInterruptLevel > currentMask || m_pendingInterruptLevel == 7) {
        
        int servicingLevel = m_pendingInterruptLevel;
        
        // --- IACK (Interrupt Acknowledge) Phase ---
        // Sega Mega Drive Hardware Quirk: When the 68000 acknowledges an interrupt,
        // it doesn't specify which one. If H-Int and V-Int are asserted simultaneously,
        // the VDP might clear the V-Int flag while the CPU actually services the H-Int.
        // For our decoupled CPU, we simulate the internal acknowledge by dropping the line.
        m_pendingInterruptLevel = 0; 
        
        // Interrupts wake the processor up from a STOP instruction
        m_halted = false;

        // Auto-vector offsets start at vector 24 (0x60 base address).
        // Vector 24 = Spurious, Vector 25 = Level 1 ... Vector 31 = Level 7
        Exception(24 + servicingLevel); 

        // Elevate the CPU interrupt mask to the active level to prevent
        // nested interrupts of the same or lower priority from firing.
        Word sr = m_registers.GetSR();
        sr = (sr & ~0x0700) | (static_cast<Word>(servicingLevel) << 8); 
        m_registers.SetSR(sr);

        // Hardware exception servicing standardly takes 44 cycles for auto-vectors
        return 44; 
    }

    return 0;
}

// ------------------------------------------------------------------------------
// Hardware Exception Vector Handler
// ------------------------------------------------------------------------------
void M68k::Exception(int vector) {
    Address vectorAddress = m_bus->ReadLongword(vector * 4);
    Longword sp = m_registers.ReadA(7);
    
    // 1. Push PC to Supervisor Stack (32-bit)
    sp -= 4;
    m_bus->WriteLongword(sp, m_registers.GetPC());
    
    // 2. Push SR to Supervisor Stack (16-bit)
    sp -= 2;
    m_bus->WriteWord(sp, m_registers.GetSR());
    
    m_registers.WriteA(7, sp);
    
    // 3. Force Supervisor state (Bit 13) and clear Trace state (Bit 15)
    Word sr = m_registers.GetSR();
    sr |= 0x2000;  
    sr &= ~0x8000; 
    m_registers.SetSR(sr);
    
    // 4. Jump to the exception vector address
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
    // Phase 1: Evaluate pending hardware interrupts BEFORE fetching next instruction
    int interruptCycles = CheckInterrupts();
    if (interruptCycles > 0) {
        return interruptCycles;
    }

    if (m_halted) {
        // CPU is in STOP state waiting for an interrupt, burn 4 cycles
        return 4; 
    }

    // Phase 2: Fetch and Decode
    Address instructionPC = m_registers.GetPC();
    Word opcode = FetchCode();
    
    RecordInstruction(instructionPC, opcode);

    DecodedInstruction inst = M68kDecoder::Decode(opcode);

    // Phase 3: Execute Domain Delegation
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
        case OpType::NEG:   
        case OpType::NEGX:  
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
                Exception(4); // Illegal Instruction Trap (Vector 4)
            }
            return 34; // standard trap execution cost
        }
    }
}

} // namespace GenesisEmu::Core::Domain::M68k