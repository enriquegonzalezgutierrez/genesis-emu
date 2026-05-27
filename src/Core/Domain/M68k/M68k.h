// ==============================================================================
// GenesisEmu - M68k Processor Orchestrator Header (Core Domain)
// ==============================================================================
// This file declares the primary M68k CPU class. It manages program sync,
// auto-vectored interrupt requests, and the fetch-decode-execute steps.
// ==============================================================================

#pragma once

#include "../Common/IBus.h"
#include "M68kRegisters.h"

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
     * @brief Triggers an auto-vectored hardware interrupt exception.
     * @param level Interrupt priority level (1 to 7). Level 6 is VBlank, Level 4 is HBlank.
     */
    void TriggerInterrupt(int level);

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

private:
    Common::IBus* m_bus;         // Pointer to the motherboard bus routing interface
    M68kRegisters m_registers;   // Isolated Entity holding CPU register states
    bool          m_halted;      // Active low processor Halt line state

    /**
     * @brief Fetches a 16-bit instruction word from the current PC, incrementing PC by 2.
     */
    Common::Word FetchCode();

    /**
     * @brief Simulates a 68000 hardware exception vector call (e.g. Illegal, Line A, Line F).
     *        Pushes SR and PC to the supervisor stack and jumps to VectorAddress.
     * @param vector Target vector index (4 = Illegal, 10 = Line A, 11 = Line F).
     */
    void Exception(int vector);
};

} // namespace GenesisEmu::Core::Domain::M68k