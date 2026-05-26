// ==============================================================================
// GenesisEmu - Motorola 68000 CPU Implementation (Core Domain)
// ==============================================================================
// This file implements the main M68k CPU execution loops.
// Upgraded the Step loop to execute ADDX (Add with Extend) and SUBX (Subtract 
// with Extend) instructions with full multi-precision flag accuracy.
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
            Address targetAddress = M68kAddressing::ResolveAddress(inst.destMode, inst.destRegister, OperandSize::NONE, *this, m_bus);
            m_pc = targetAddress;
            
            int cycles = 8;
            if (inst.destMode == AddressingMode::AbsoluteLong) cycles = 16;
            else if (inst.destMode == AddressingMode::AbsoluteShort || inst.destMode == AddressingMode::AddressRegisterIndirect) cycles = 12;
            else cycles = 14;
            return cycles;
        }

        // --- Branch Conditionally (Bcc) Family ---
        case OpType::BRA:
        case OpType::BCC:
        case OpType::BCS:
        case OpType::BEQ:
        case OpType::BGE:
        case OpType::BGT:
        case OpType::BHI:
        case OpType::BLE:
        case OpType::BLS:
        case OpType::BLT:
        case OpType::BMI:
        case OpType::BNE:
        case OpType::BPL:
        case OpType::BVC:
        case OpType::BVS: {
            bool takeBranch = false;
            
            bool n = GetFlagNegative();
            bool v = GetFlagOverflow();
            bool z = GetFlagZero();
            bool c = GetFlagCarry();

            if (inst.type == OpType::BRA) takeBranch = true; 
            else if (inst.type == OpType::BCC) takeBranch = !c; 
            else if (inst.type == OpType::BCS) takeBranch = c;  
            else if (inst.type == OpType::BEQ) takeBranch = z; 
            else if (inst.type == OpType::BGE) takeBranch = (n == v);  
            else if (inst.type == OpType::BGT) takeBranch = (n == v) && !z;
            else if (inst.type == OpType::BHI) takeBranch = !c && !z;
            else if (inst.type == OpType::BLE) takeBranch = z || (n != v);
            else if (inst.type == OpType::BLS) takeBranch = c || z;
            else if (inst.type == OpType::BLT) takeBranch = (n != v);
            else if (inst.type == OpType::BMI) takeBranch = n;
            else if (inst.type == OpType::BNE) takeBranch = !z;
            else if (inst.type == OpType::BPL) takeBranch = !n;
            else if (inst.type == OpType::BVC) takeBranch = !v;
            else if (inst.type == OpType::BVS) takeBranch = v;

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

        // --- Set Conditionally (Scc) Family ---
        case OpType::SCC: {
            bool conditionMet = false;
            bool n = GetFlagNegative();
            bool v = GetFlagOverflow();
            bool z = GetFlagZero();
            bool c = GetFlagCarry();
            Byte cond = static_cast<Byte>(inst.immediateData & 0x0F);

            switch (cond) {
                case 0x0: conditionMet = true; break;              
                case 0x1: conditionMet = false; break;             
                case 0x2: conditionMet = !c && !z; break;          
                case 0x3: conditionMet = c || z; break;            
                case 0x4: conditionMet = !c; break;                
                case 0x5: conditionMet = c; break;                 
                case 0x6: conditionMet = !z; break;                
                case 0x7: conditionMet = z; break;                 
                case 0x8: conditionMet = !v; break;                
                case 0x9: conditionMet = v; break;                 
                case 0xA: conditionMet = !n; break;                
                case 0xB: conditionMet = n; break;                 
                case 0xC: conditionMet = (n == v); break;          
                case 0xD: conditionMet = (n != v); break;          
                case 0xE: conditionMet = (n == v) && !z; break;    
                case 0xF: conditionMet = z || (n != v); break;     
                default: break;
            }

            Byte valueToWrite = conditionMet ? 0xFF : 0x00;
            M68kAddressing::WriteOperand(inst.destMode, inst.destRegister, OperandSize::BYTE, valueToWrite, *this, m_bus);

            if (inst.destMode == AddressingMode::DataRegisterDirect) {
                return conditionMet ? 6 : 4;
            } else {
                return 8;
            }
        }

        case OpType::JSR: {
            Address targetAddress = M68kAddressing::ResolveAddress(inst.destMode, inst.destRegister, OperandSize::NONE, *this, m_bus);
            M68kFlowControl::ExecuteJSR(m_bus, m_pc, m_a[7], targetAddress);
            
            int cycles = 16;
            if (inst.destMode == AddressingMode::AddressRegisterIndex || inst.destMode == AddressingMode::ProgramCounterIndex) {
                cycles = 20;
            }
            return cycles;
        }

        case OpType::BSR: {
            if (inst.size == OperandSize::WORD) {
                std::int16_t displacement = static_cast<std::int16_t>(FetchCode());
                return M68kFlowControl::ExecuteBSR(m_bus, m_pc, m_a[7], displacement, instructionPC);
            } else if (inst.size == OperandSize::BYTE) {
                std::int8_t displacement8 = static_cast<std::int8_t>(opcode & 0x00FF);
                return M68kFlowControl::ExecuteBSR(m_bus, m_pc, m_a[7], displacement8, instructionPC);
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
            m_sr &= ~0x000F;
            Longword mask = (inst.size == OperandSize::WORD) ? 0xFFFF : 0xFFFFFFFF;
            Longword msb  = (inst.size == OperandSize::WORD) ? 0x8000 : 0x80000000;
            if ((result & mask) == 0)      m_sr |= 0x0004; 
            if ((result & msb) != 0)       m_sr |= 0x0008; 
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
        case OpType::LSL:
        case OpType::ASR:
        case OpType::ASL:
        case OpType::ROR:
        case OpType::ROL: {
            Byte shiftCount = 0;
            if (inst.srcMode == AddressingMode::Immediate) {
                shiftCount = static_cast<Byte>(inst.immediateData);
            } else {
                shiftCount = static_cast<Byte>(GetDRegister(inst.srcRegister) & 63);
            }

            Longword targetValue = M68kAddressing::ReadOperand(inst.destMode, inst.destRegister, inst.size, *this, m_bus);
            Longword result = 0;

            if (inst.type == OpType::LSR)       result = M68kCoreInstructions::ExecuteLSR(targetValue, shiftCount, inst.size, m_sr);
            else if (inst.type == OpType::LSL)  result = M68kCoreInstructions::ExecuteLSL(targetValue, shiftCount, inst.size, m_sr);
            else if (inst.type == OpType::ASR)  result = M68kCoreInstructions::ExecuteASR(targetValue, shiftCount, inst.size, m_sr);
            else if (inst.type == OpType::ASL)  result = M68kCoreInstructions::ExecuteASL(targetValue, shiftCount, inst.size, m_sr);
            else if (inst.type == OpType::ROR)  result = M68kCoreInstructions::ExecuteROR(targetValue, shiftCount, inst.size, m_sr);
            else if (inst.type == OpType::ROL)  result = M68kCoreInstructions::ExecuteROL(targetValue, shiftCount, inst.size, m_sr);

            M68kAddressing::WriteOperand(inst.destMode, inst.destRegister, inst.size, result, *this, m_bus);

            int baseCycles = (inst.size == OperandSize::LONG) ? 8 : 6;
            if (inst.srcMode == AddressingMode::DataRegisterDirect) {
                baseCycles += 2;
            }
            return baseCycles + (2 * shiftCount);
        }

        // --- ADDED: ADDX Multi-Precision Sum ---
        case OpType::ADDX: {
            Longword srcVal  = M68kAddressing::ReadOperand(inst.srcMode, inst.srcRegister, inst.size, *this, m_bus);
            Longword destVal = M68kAddressing::ReadOperand(inst.destMode, inst.destRegister, inst.size, *this, m_bus);
            
            Longword ext = GetFlagExtend() ? 1 : 0;
            Longword mask = (inst.size == OperandSize::BYTE) ? 0xFF : (inst.size == OperandSize::WORD) ? 0xFFFF : 0xFFFFFFFF;
            
            Longword d = destVal & mask;
            Longword s = srcVal & mask;
            Longword result = (d + s + ext) & mask;

            auto IsSignBitSet = [](Longword val, OperandSize sz) {
                if (sz == OperandSize::BYTE) return (val & 0x80) != 0;
                if (sz == OperandSize::WORD) return (val & 0x8000) != 0;
                return (val & 0x80000000) != 0;
            };

            bool dSign = IsSignBitSet(d, inst.size);
            bool sSign = IsSignBitSet(s, inst.size);
            bool rSign = IsSignBitSet(result, inst.size);

            m_sr &= ~0x001B; // Clear N, V, C, X (Z is left untouched)
            
            if (rSign) m_sr |= 0x0008; // Set N
            
            // Z is cleared if result is non-zero, otherwise remains unaffected
            if (result != 0) {
                m_sr &= ~0x0004; 
            }
            
            // V flag
            if (dSign == sSign && dSign != rSign) {
                m_sr |= 0x0002;
            }
            
            // C & X carry flags
            bool carry = false;
            if (inst.size == OperandSize::BYTE) {
                carry = (destVal & 0xFF) + (srcVal & 0xFF) + ext > 0xFF;
            } else if (inst.size == OperandSize::WORD) {
                carry = (destVal & 0xFFFF) + (srcVal & 0xFFFF) + ext > 0xFFFF;
            } else {
                carry = (static_cast<std::uint64_t>(destVal) + srcVal + ext) > 0xFFFFFFFFu;
            }
            
            if (carry) {
                m_sr |= 0x0001; // C
                m_sr |= 0x0010; // X
            }

            M68kAddressing::WriteOperand(inst.destMode, inst.destRegister, inst.size, result, *this, m_bus);
            
            if (inst.destMode == AddressingMode::DataRegisterDirect) {
                return (inst.size == OperandSize::LONG) ? 8 : 4;
            } else {
                return (inst.size == OperandSize::LONG) ? 30 : 18;
            }
        }

        // --- ADDED: SUBX Multi-Precision Subtraction ---
        case OpType::SUBX: {
            Longword srcVal  = M68kAddressing::ReadOperand(inst.srcMode, inst.srcRegister, inst.size, *this, m_bus);
            Longword destVal = M68kAddressing::ReadOperand(inst.destMode, inst.destRegister, inst.size, *this, m_bus);
            
            Longword ext = GetFlagExtend() ? 1 : 0;
            Longword mask = (inst.size == OperandSize::BYTE) ? 0xFF : (inst.size == OperandSize::WORD) ? 0xFFFF : 0xFFFFFFFF;
            
            Longword d = destVal & mask;
            Longword s = srcVal & mask;
            Longword result = (d - s - ext) & mask;

            auto IsSignBitSet = [](Longword val, OperandSize sz) {
                if (sz == OperandSize::BYTE) return (val & 0x80) != 0;
                if (sz == OperandSize::WORD) return (val & 0x8000) != 0;
                return (val & 0x80000000) != 0;
            };

            bool dSign = IsSignBitSet(d, inst.size);
            bool sSign = IsSignBitSet(s, inst.size);
            bool rSign = IsSignBitSet(result, inst.size);

            m_sr &= ~0x001B; // Clear N, V, C, X
            
            if (rSign) m_sr |= 0x0008; // Set N
            
            if (result != 0) {
                m_sr &= ~0x0004; // Clear Z
            }
            
            // V flag
            if (dSign != sSign && dSign != rSign) {
                m_sr |= 0x0002;
            }
            
            // C & X borrow flags
            bool borrow = false;
            if (inst.size == OperandSize::BYTE) {
                borrow = (destVal & 0xFF) < (srcVal & 0xFF) + ext;
            } else if (inst.size == OperandSize::WORD) {
                borrow = (destVal & 0xFFFF) < (srcVal & 0xFFFF) + ext;
            } else {
                borrow = static_cast<std::uint64_t>(destVal) < static_cast<std::uint64_t>(srcVal) + ext;
            }
            
            if (borrow) {
                m_sr |= 0x0001; // C
                m_sr |= 0x0010; // X
            }

            M68kAddressing::WriteOperand(inst.destMode, inst.destRegister, inst.size, result, *this, m_bus);
            
            if (inst.destMode == AddressingMode::DataRegisterDirect) {
                return (inst.size == OperandSize::LONG) ? 8 : 4;
            } else {
                return (inst.size == OperandSize::LONG) ? 30 : 18;
            }
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

        case OpType::MOVEQ: {
            Longword val = M68kCoreInstructions::ExecuteMOVEQ(static_cast<Byte>(inst.immediateData & 0xFF));
            SetDRegister(inst.destRegister, val);
            
            m_sr &= ~0x000F;
            if (val == 0) m_sr |= 0x0004; 
            if ((val & 0x80000000) != 0) m_sr |= 0x0008; 
            return 4;
        }

        case OpType::LEA: {
            Address targetAddress = M68kAddressing::ResolveAddress(inst.srcMode, inst.srcRegister, inst.size, *this, m_bus);
            Longword regVal = GetARegister(inst.destRegister);
            M68kCoreInstructions::ExecuteLEA(regVal, targetAddress);
            SetARegister(inst.destRegister, regVal);
            return 8; 
        }

        case OpType::MOVEM: {
            Word regMask = FetchCode();
            bool isLoad = (inst.immediateData & 0x0400) != 0;
            int regCount = 0;
            for (int i = 0; i < 16; ++i) {
                if ((regMask & (1 << i)) != 0) regCount++;
            }
            
            int step = (inst.size == OperandSize::LONG) ? 4 : 2;

            if (isLoad) {
                if (inst.srcMode == AddressingMode::AddressRegisterPostincrement) {
                    Byte regIndex = inst.srcRegister;
                    Address addr = GetARegister(regIndex);
                    for (int i = 0; i < 16; ++i) {
                        if ((regMask & (1 << i)) != 0) {
                            Longword val = 0;
                            if (inst.size == OperandSize::LONG) {
                                val = m_bus->ReadLongword(addr);
                            } else {
                                std::int16_t signedWord = static_cast<std::int16_t>(m_bus->ReadWord(addr));
                                val = static_cast<Longword>(static_cast<std::int32_t>(signedWord));
                            }
                            addr += step;
                            if (i < 8) {
                                SetDRegister(i, val);
                            } else {
                                SetARegister(i - 8, val);
                            }
                        }
                    }
                    bool eaRegLoaded = (regMask & (1 << (8 + regIndex))) != 0;
                    if (!eaRegLoaded) {
                        SetARegister(regIndex, addr);
                    }
                } else {
                    Address addr = M68kAddressing::ResolveAddress(inst.srcMode, inst.srcRegister, inst.size, *this, m_bus);
                    for (int i = 0; i < 16; ++i) {
                        if ((regMask & (1 << i)) != 0) {
                            Longword val = 0;
                            if (inst.size == OperandSize::LONG) {
                                val = m_bus->ReadLongword(addr);
                            } else {
                                std::int16_t signedWord = static_cast<std::int16_t>(m_bus->ReadWord(addr));
                                val = static_cast<Longword>(static_cast<std::int32_t>(signedWord));
                            }
                            if (i < 8) {
                                SetDRegister(i, val);
                            } else {
                                SetARegister(i - 8, val);
                            }
                            addr += step;
                        }
                    }
                }
            } else { // Store
                if (inst.destMode == AddressingMode::AddressRegisterPredecrement) {
                    Byte regIndex = inst.destRegister;
                    Address addr = GetARegister(regIndex);
                    for (int i = 0; i < 16; ++i) {
                        if ((regMask & (1 << i)) != 0) {
                            addr -= step;
                            Longword val = (i < 8) ? GetARegister(7 - i) : GetDRegister(15 - i);
                            if (inst.size == OperandSize::LONG) {
                                m_bus->WriteLongword(addr, val);
                            } else {
                                m_bus->WriteWord(addr, static_cast<Word>(val & 0xFFFF));
                            }
                        }
                    }
                    SetARegister(regIndex, addr);
                } else {
                    Address addr = M68kAddressing::ResolveAddress(inst.destMode, inst.destRegister, inst.size, *this, m_bus);
                    for (int i = 0; i < 16; ++i) {
                        if ((regMask & (1 << i)) != 0) {
                            Longword val = (i < 8) ? GetDRegister(i) : GetARegister(i - 8);
                            if (inst.size == OperandSize::LONG) {
                                m_bus->WriteLongword(addr, val);
                            } else {
                                m_bus->WriteWord(addr, static_cast<Word>(val & 0xFFFF));
                            }
                            addr += step;
                        }
                    }
                }
            }

            int baseCycles = isLoad ? 12 : 8;
            int multiplier = (inst.size == OperandSize::LONG) ? 8 : 4;
            return baseCycles + (multiplier * regCount);
        }

        case OpType::ADDQ: {
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
            Longword srcVal = inst.immediateData;
            if (inst.destMode == AddressingMode::AddressRegisterDirect) {
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

        case OpType::CMP: {
            Longword srcVal  = M68kAddressing::ReadOperand(inst.srcMode, inst.srcRegister, inst.size, *this, m_bus);
            Longword destVal = M68kAddressing::ReadOperand(inst.destMode, inst.destRegister, inst.size, *this, m_bus);
            M68kCoreInstructions::ExecuteCMP(destVal, srcVal, inst.size, m_sr);
            return (inst.size == OperandSize::LONG) ? 8 : 4;
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