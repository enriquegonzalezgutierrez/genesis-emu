// ==============================================================================
// GenesisEmu - M68k Processor Orchestrator Header (Core Domain)
// ==============================================================================
// This file declares the primary M68k CPU class. It manages program sync,
// auto-vectored interrupt requests, and the fetch-decode-execute pipeline.
//
// SOLID Compliance:
// 1. Single Responsibility Principle (SRP):
//    It acts as the Facade/Orchestrator for the CPU, managing the pipeline
//    without implementing the actual ALU or decoding logic.
// ==============================================================================

#pragma once

#include "../Common/IBus.h"
#include "M68kRegisters.h"
#include <array>

namespace GenesisEmu::Core::Domain::M68k {

/**
 * @class M68k
 * @brief Processor orchestrator managing execution pipeline loops and hardware synchronization.
 */
class M68k {
public:
    explicit M68k(Common::IBus* bus);
    ~M68k() = default;

    // --- Processor Lifecycle and Execution Controls ---
    void Reset();
    int Step();

    // --- Hardware Interrupt Exception Management ---
    /**
     * @brief Asserts an auto-vectored hardware interrupt request line.
     *        The interrupt is not executed immediately but evaluated at the start
     *        of the next instruction fetch cycle.
     * @param level Interrupt priority level (1 to 7). Level 6 is VBlank, Level 4 is HBlank.
     */
    void TriggerInterrupt(int level);

    /**
     * @brief Simulates a 68000 hardware exception vector call (e.g. Illegal, Line A, Line F, Div-By-Zero).
     *        Pushes SR and PC to the supervisor stack and jumps to VectorAddress.
     * @param vector Target vector index (4 = Illegal, 5 = Zero Divide, 10 = Line A, 11 = Line F).
     */
    void Exception(int vector);

    // --- State Accessors (Wraps Encapsulated M68kRegisters Entity) ---
    inline Common::Longword GetDRegister(int index) const { return m_registers.ReadD(index); }
    inline Common::Longword GetARegister(int index) const { return m_registers.ReadA(index); }
    inline Common::Address  GetPC() const { return m_registers.GetPC(); }
    inline Common::Word     GetSR() const { return m_registers.GetSR(); }
    inline Common::Longword GetUSP() const { return m_registers.GetUSP(); }
    inline bool             IsHalted() const { return m_halted; }

    inline bool GetFlagCarry() const    { return m_registers.GetCarry(); }
    inline bool GetFlagOverflow() const { return m_registers.GetOverflow(); }
    inline bool GetFlagZero() const     { return m_registers.GetZero(); }
    inline bool GetFlagNegative() const { return m_registers.GetNegative(); }
    inline bool GetFlagExtend() const   { return m_registers.GetExtend(); }

    // --- State Modifiers ---
    inline void SetDRegister(int index, Common::Longword value) { m_registers.WriteD(index, value); }
    inline void SetARegister(int index, Common::Longword value) { m_registers.WriteA(index, value); }
    inline void SetPC(Common::Address address) { m_registers.SetPC(address); }
    inline void SetSR(Common::Word value) { m_registers.SetSR(value); }
    inline void SetUSP(Common::Longword value) { m_registers.SetUSP(value); }

    /**
     * @brief Dumps the rolling instruction execution history buffer into the console.
     */
    void DumpExecutionHistory();

private:
    Common::IBus* m_bus;         // Pointer to the motherboard bus routing interface
    M68kRegisters m_registers;   // Isolated Entity holding CPU register states
    bool          m_halted;      // Active low processor Halt line state
    
    // Highest pending hardware interrupt level (0 means no interrupt pending)
    int           m_pendingInterruptLevel; 

    /**
     * @struct InstructionHistoryEntry
     * @brief Represents a logged executed instruction context.
     */
    struct InstructionHistoryEntry {
        Common::Address pc;
        Common::Word    opcode;
    };

    // Rolling circular queue holding the last 32 executed instructions
    std::array<InstructionHistoryEntry, 32> m_instructionHistory{};
    std::size_t                             m_historyIndex = 0;

    /**
     * @brief Evaluates pending interrupts against the current SR mask before fetching the next opcode.
     * @return Cycles consumed by servicing the exception (0 if no interrupt was serviced).
     */
    int CheckInterrupts();

    /**
     * @brief Pushes a fetched instruction context into the rolling history queue.
     */
    void RecordInstruction(Common::Address pc, Common::Word opcode);

    /**
     * @brief Fetches a 16-bit instruction word from the current PC, incrementing PC by 2.
     */
    Common::Word FetchCode();
};

} // namespace GenesisEmu::Core::Domain::M68k