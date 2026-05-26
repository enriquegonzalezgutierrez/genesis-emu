// ==============================================================================
// GenesisEmu - Motorola 68000 CPU Implementation (Core Domain)
// ==============================================================================
// This file implements the main M68k CPU execution loops.
// Added LSR (Logical Shift Right) and LSL (Logical Shift Left) logic.
// ==============================================================================

#include "M68k.h"
#include "M68kDecoder.h"
#include "M68kArithmetic.h"
#include "M68kFlowControl.h"
#include "M68kCoreInstructions.h"
#include "M68kAddressing.h" 
#include <iostream>
#include <vector>

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


struct M68kTraceEntry {
    Address pc;
    Word opcode;
};

static std::vector<M68kTraceEntry> s_m68k_trace;
static size_t s_m68k_trace_index = 0;
constexpr size_t TRACE_LIMIT = 32;

static void RecordTrace(Address pc, Word opcode) {
    if (s_m68k_trace.size() < TRACE_LIMIT) {
        s_m68k_trace.push_back({pc, opcode});
    } else {
        s_m68k_trace[s_m68k_trace_index] = {pc, opcode};
        s_m68k_trace_index = (s_m68k_trace_index + 1) % TRACE_LIMIT;
    }
}

static void PrintTrace() {
    std::cerr << "----------------------------------------------------" << std::endl;
    std::cerr << "Execution Trace (Last " << s_m68k_trace.size() << " instructions):" << std::endl;
    size_t count = s_m68k_trace.size();
    size_t idx = (count < TRACE_LIMIT) ? 0 : s_m68k_trace_index;
    for (size_t i = 0; i < count; ++i) {
        std::cerr << "  " << (i + 1) << ". PC: 0x" << std::hex << std::uppercase 
                  << s_m68k_trace[idx].pc << "  Opcode: 0x" << s_m68k_trace[idx].opcode << std::dec << std::endl;
        idx = (idx + 1) % count;
    }
    std::cerr << "----------------------------------------------------" << std::endl;
}

// ------------------------------------------------------------------------------
// Helper to print complete Diagnostic Halt Report
// ------------------------------------------------------------------------------
static void TriggerDiagnosticHalt(bool& haltedRef, Address pc, Word opcode, Word sr, const Longword* d, const Longword* a, const std::string& message) {
    haltedRef = true;
    std::cerr << "\n====================================================" << std::endl;
    std::cerr << "[CPU DIAGNOSTIC HALT] " << message << std::endl;
    std::cerr << "====================================================" << std::endl;
    std::cerr << "PC:         0x" << std::hex << std::uppercase << pc << std::endl;
    std::cerr << "Opcode:     0x" << opcode << std::endl;
    std::cerr << "Status Reg: 0x" << sr << std::endl;
    std::cerr << "----------------------------------------------------" << std::endl;
    for (int i = 0; i < 8; ++i) {
        std::cerr << "D" << i << ": 0x" << d[i] << "   A" << i << ": 0x" << a[i] << std::endl;
    }
    std::cerr << "====================================================\n" << std::dec << std::endl;
    PrintTrace();
}

int M68k::Step() {
    if (m_halted) {
        return 4;
    }

    Address instructionPC = m_pc;

    // Fetch and Decode
    Word opcode = FetchCode();
    RecordTrace(instructionPC, opcode);
    DecodedInstruction inst = M68kDecoder::Decode(opcode);

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
            TriggerDiagnosticHalt(m_halted, instructionPC, opcode, m_sr, m_d, m_a, "Unhandled addressing mode for JMP");
            return 4;
        }

        case OpType::BRA:
        case OpType::BNE:
        case OpType::BEQ:
        case OpType::BPL:
        case OpType::BMI:
        case OpType::BHI: {
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
            } else if (inst.type == OpType::BHI) {
                takeBranch = !GetFlagCarry() && !GetFlagZero();
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
            else if (inst.size == OperandSize::BYTE) {
                std::int8_t displacement8 = static_cast<std::int8_t>(opcode & 0x00FF);

                if (takeBranch) {
                    m_pc = (instructionPC + 2) + displacement8;
                    return 10; 
                } else {
                    return 8;  
                }
            }

            TriggerDiagnosticHalt(m_halted, instructionPC, opcode, m_sr, m_d, m_a, "Unhandled size for Branch");
            return 4;
        }

        case OpType::JSR: {
            if (inst.destMode == AddressingMode::AbsoluteLong) {
                Word highWord = FetchCode();
                Word lowWord  = FetchCode();
                Address targetAddress = (static_cast<Longword>(highWord) << 16) | lowWord;
                
                return M68kFlowControl::ExecuteJSR(m_bus, m_pc, m_a[7], targetAddress);
            }
            else if (inst.destMode == AddressingMode::ProgramCounterDisplacement) {
                std::int16_t displacement = static_cast<std::int16_t>(FetchCode());
                Address targetAddress = (instructionPC + 2) + displacement;

                return M68kFlowControl::ExecuteJSR(m_bus, m_pc, m_a[7], targetAddress);
            }

            TriggerDiagnosticHalt(m_halted, instructionPC, opcode, m_sr, m_d, m_a, "Unhandled addressing mode for JSR");
            return 4;
        }

        case OpType::BSR: {
            if (inst.size == OperandSize::WORD) {
                std::int16_t displacement = static_cast<std::int16_t>(FetchCode());
                return M68kFlowControl::ExecuteBSR(m_bus, m_pc, m_a[7], displacement, instructionPC);
            }
            TriggerDiagnosticHalt(m_halted, instructionPC, opcode, m_sr, m_d, m_a, "Unhandled size for BSR");
            return 4;
        }

        case OpType::RTS: {
            return M68kFlowControl::ExecuteRTS(m_bus, m_pc, m_a[7]);
        }

        case OpType::CLR: {
            M68kAddressing::WriteOperand(inst.destMode, inst.destRegister, inst.size, 0, *this, m_bus);

            m_sr &= ~0x000B; 
            m_sr |= 0x0004;  

            return (inst.destMode == AddressingMode::DataRegisterDirect) ? 4 : 12;
        }

        case OpType::SWAP: {
            Longword val    = GetDRegister(inst.destRegister);
            Longword result = M68kCoreInstructions::ExecuteSWAP(val, m_sr);
            SetDRegister(inst.destRegister, result);
            return 4;
        }

        case OpType::EXT: {
            Longword val    = GetDRegister(inst.destRegister);
            Longword result = M68kCoreInstructions::ExecuteEXT(val, inst.size);
            // EXT updates N and Z flags, clears V and C
            m_sr &= ~0x000F;
            Longword mask = (inst.size == OperandSize::WORD) ? 0xFFFF : 0xFFFFFFFF;
            Longword msb  = (inst.size == OperandSize::WORD) ? 0x8000 : 0x80000000;
            if ((result & mask) == 0)      m_sr |= 0x0004; // Z
            if ((result & msb) != 0)       m_sr |= 0x0008; // N
            SetDRegister(inst.destRegister, result);
            return 4;
        }

        case OpType::PEA: {
            Address targetAddress = M68kAddressing::ResolveAddress(inst.destMode, inst.destRegister, inst.size, *this, m_bus);
            M68kCoreInstructions::ExecutePEA(m_bus, m_a[7], targetAddress);

            return 12;
        }

        case OpType::DBF: {
            std::int16_t displacement = static_cast<std::int16_t>(FetchCode());
            
            Word counter = static_cast<Word>(GetDRegister(inst.srcRegister) & 0xFFFF);
            counter--;
            
            Longword currentD = GetDRegister(inst.srcRegister);
            SetDRegister(inst.srcRegister, (currentD & 0xFFFF0000) | counter);

            if (counter != 0xFFFF) {
                m_pc = (instructionPC + 2) + displacement;
                return 10; 
            } else {
                return 14; 
            }
        }

        case OpType::TST: {
            Longword value = M68kAddressing::ReadOperand(inst.srcMode, inst.srcRegister, inst.size, *this, m_bus);
            M68kCoreInstructions::ExecuteTST(value, inst.size, m_sr);
            return 8;
        }

        case OpType::BTST: {
            Byte bitNum = 0;
            int cycles = 8;
            
            if (inst.srcMode == AddressingMode::Immediate) {
                bitNum = static_cast<Byte>(FetchCode() & 0xFF);
            } else {
                TriggerDiagnosticHalt(m_halted, instructionPC, opcode, m_sr, m_d, m_a, "Unhandled src mode for BTST");
                return 4;
            }

            Longword targetValue = M68kAddressing::ReadOperand(inst.destMode, inst.destRegister, inst.size, *this, m_bus);
            M68kCoreInstructions::ExecuteBTST(targetValue, bitNum, inst.size, m_sr);

            if (inst.destMode == AddressingMode::DataRegisterDirect) {
                cycles = 10;
            } else {
                cycles = 12; 
            }

            return cycles;
        }

        case OpType::LSR:
        case OpType::LSL: {
            // --- Shift Register Immediate ---
            Byte shiftCount = static_cast<Byte>(inst.immediateData);
            Longword targetValue = M68kAddressing::ReadOperand(inst.destMode, inst.destRegister, inst.size, *this, m_bus);

            Longword result = 0;
            if (inst.type == OpType::LSR) {
                result = M68kCoreInstructions::ExecuteLSR(targetValue, shiftCount, inst.size, m_sr);
            } else {
                result = M68kCoreInstructions::ExecuteLSL(targetValue, shiftCount, inst.size, m_sr);
            }

            M68kAddressing::WriteOperand(inst.destMode, inst.destRegister, inst.size, result, *this, m_bus);

            // Shift cycles = 6 base + (2 * shiftCount)
            return 6 + (2 * shiftCount);
        }

        case OpType::CMPI: {
            Longword immediateValue = 0;
            int cycles = 8; 

            if (inst.size == OperandSize::LONG) {
                Word hi = FetchCode();
                Word lo = FetchCode();
                immediateValue = (static_cast<Longword>(hi) << 16) | lo;
                cycles = 14;
            } else if (inst.size == OperandSize::WORD) {
                immediateValue = FetchCode();
                cycles = 8;
            } else {
                immediateValue = FetchCode() & 0xFF;
                cycles = 8;
            }

            Longword destValue = M68kAddressing::ReadOperand(inst.destMode, inst.destRegister, inst.size, *this, m_bus);
            M68kCoreInstructions::ExecuteCMP(destValue, immediateValue, inst.size, m_sr);

            return cycles;
        }

        case OpType::MOVE_TO_SR: {
            Word val = 0;
            if (inst.srcMode == AddressingMode::Immediate) {
                val = FetchCode();
            } else {
                TriggerDiagnosticHalt(m_halted, instructionPC, opcode, m_sr, m_d, m_a, "Unhandled src mode for MOVE_TO_SR");
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

        case OpType::ADDQ: {
            // Quick ADD: immediate is embedded in the opcode, no extension word.
            Longword srcVal  = inst.immediateData;
            Longword destVal = M68kAddressing::ReadOperand(inst.destMode, inst.destRegister, inst.size, *this, m_bus);
            Longword result  = M68kArithmetic::ExecuteADD(destVal, srcVal, inst.size, m_sr);
            M68kAddressing::WriteOperand(inst.destMode, inst.destRegister, inst.size, result, *this, m_bus);
            return (inst.size == OperandSize::LONG) ? 8 : 4;
        }

        case OpType::ADD: {
            Longword srcVal  = M68kAddressing::ReadOperand(inst.srcMode, inst.srcRegister, inst.size, *this, m_bus);
            Longword destVal = M68kAddressing::ReadOperand(inst.destMode, inst.destRegister, inst.size, *this, m_bus);
            Longword result  = M68kArithmetic::ExecuteADD(destVal, srcVal, inst.size, m_sr);
            
            M68kAddressing::WriteOperand(inst.destMode, inst.destRegister, inst.size, result, *this, m_bus);
            return (inst.srcMode == AddressingMode::Immediate) ? ((inst.size == OperandSize::LONG) ? 16 : 8) : 4; 
        }

        case OpType::SUBQ: {
            // Quick SUB: immediate is embedded in the opcode, no extension word.
            Longword srcVal = inst.immediateData;
            if (inst.destMode == AddressingMode::AddressRegisterDirect) {
                // SUBQ on An does not affect flags
                Longword destVal = GetARegister(inst.destRegister);
                SetARegister(inst.destRegister, destVal - srcVal);
            } else {
                Longword destVal = M68kAddressing::ReadOperand(inst.destMode, inst.destRegister, inst.size, *this, m_bus);
                Longword result  = M68kArithmetic::ExecuteSUB(destVal, srcVal, inst.size, m_sr);
                M68kAddressing::WriteOperand(inst.destMode, inst.destRegister, inst.size, result, *this, m_bus);
            }
            return (inst.size == OperandSize::LONG) ? 8 : 4;
        }

        case OpType::SUB: {
            Longword srcVal  = M68kAddressing::ReadOperand(inst.srcMode, inst.srcRegister, inst.size, *this, m_bus);
            
            if (inst.destMode == AddressingMode::AddressRegisterDirect) {
                Longword destVal = GetARegister(inst.destRegister);
                SetARegister(inst.destRegister, destVal - srcVal);
                return (inst.size == OperandSize::LONG) ? 12 : 8;
            } else {
                Longword destVal = M68kAddressing::ReadOperand(inst.destMode, inst.destRegister, inst.size, *this, m_bus);
                Longword result  = M68kArithmetic::ExecuteSUB(destVal, srcVal, inst.size, m_sr);
                
                M68kAddressing::WriteOperand(inst.destMode, inst.destRegister, inst.size, result, *this, m_bus);
                return (inst.srcMode == AddressingMode::Immediate) ? ((inst.size == OperandSize::LONG) ? 16 : 8) : 4; 
            }
        }

        case OpType::AND: {
            Longword srcVal  = M68kAddressing::ReadOperand(inst.srcMode, inst.srcRegister, inst.size, *this, m_bus);
            Longword destVal = M68kAddressing::ReadOperand(inst.destMode, inst.destRegister, inst.size, *this, m_bus);
            Longword result  = M68kArithmetic::ExecuteAND(destVal, srcVal, inst.size, m_sr);

            M68kAddressing::WriteOperand(inst.destMode, inst.destRegister, inst.size, result, *this, m_bus);
            return (inst.srcMode == AddressingMode::Immediate) ? ((inst.size == OperandSize::LONG) ? 16 : 8) : 4; 
        }

        case OpType::OR: {
            Longword srcVal  = M68kAddressing::ReadOperand(inst.srcMode, inst.srcRegister, inst.size, *this, m_bus);
            Longword destVal = M68kAddressing::ReadOperand(inst.destMode, inst.destRegister, inst.size, *this, m_bus);
            Longword result  = M68kArithmetic::ExecuteOR(destVal, srcVal, inst.size, m_sr);

            M68kAddressing::WriteOperand(inst.destMode, inst.destRegister, inst.size, result, *this, m_bus);
            return (inst.srcMode == AddressingMode::Immediate) ? ((inst.size == OperandSize::LONG) ? 16 : 8) : 4; 
        }

        case OpType::EOR: {
            Longword srcVal  = M68kAddressing::ReadOperand(inst.srcMode, inst.srcRegister, inst.size, *this, m_bus);
            Longword destVal = M68kAddressing::ReadOperand(inst.destMode, inst.destRegister, inst.size, *this, m_bus);
            Longword result  = M68kArithmetic::ExecuteEOR(destVal, srcVal, inst.size, m_sr);

            M68kAddressing::WriteOperand(inst.destMode, inst.destRegister, inst.size, result, *this, m_bus);
            return (inst.srcMode == AddressingMode::Immediate) ? ((inst.size == OperandSize::LONG) ? 16 : 8) : 4; 
        }

        case OpType::MOVE: {
            Longword value = M68kAddressing::ReadOperand(inst.srcMode, inst.srcRegister, inst.size, *this, m_bus);
            M68kAddressing::WriteOperand(inst.destMode, inst.destRegister, inst.size, value, *this, m_bus);

            if (inst.destMode != AddressingMode::AddressRegisterDirect) {
                m_sr &= ~0x0003; 
                
                Longword mask = (inst.size == OperandSize::BYTE) ? 0xFF : (inst.size == OperandSize::WORD) ? 0xFFFF : 0xFFFFFFFF;
                Longword msb  = (inst.size == OperandSize::BYTE) ? 0x80 : (inst.size == OperandSize::WORD) ? 0x8000 : 0x80000000;
                Longword val  = value & mask;

                if (val == 0) m_sr |= 0x0004;
                else          m_sr &= ~0x0004;

                if ((val & msb) != 0) m_sr |= 0x0008;
                else                  m_sr &= ~0x0008;
            }

            int cycles = 4; 
            
            if (inst.srcMode == AddressingMode::Immediate || 
                inst.srcMode == AddressingMode::AddressRegisterPostincrement ||
                inst.srcMode == AddressingMode::AddressRegisterPredecrement ||
                inst.srcMode == AddressingMode::AddressRegisterDisplacement) {
                cycles += 4;
            }
            
            if (inst.destMode == AddressingMode::AddressRegisterIndirect ||
                inst.destMode == AddressingMode::AbsoluteShort ||
                inst.destMode == AddressingMode::AddressRegisterPredecrement ||
                inst.destMode == AddressingMode::AddressRegisterDisplacement) {
                cycles += 4;
            }
            else if (inst.destMode == AddressingMode::AbsoluteLong) {
                cycles += 8;
            }

            return cycles; 
        }

        default: {
            TriggerDiagnosticHalt(m_halted, instructionPC, opcode, m_sr, m_d, m_a, "Unhandled Opcode encountered!");
            return 4;
        }
    }
}

} // namespace GenesisEmu::Core