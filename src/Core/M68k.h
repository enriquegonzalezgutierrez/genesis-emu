// ==============================================================================
// GenesisEmu - Motorola 68000 CPU Domain Model Header
// ==============================================================================
// This class represents the M68k CPU. It contains registers, execution flags,
// and the core execution loop. It relies entirely on the IBus interface.
// ==============================================================================

#pragma once

#include "IBus.h"

namespace GenesisEmu::Core {

class M68k {
public:
    // Constructor requires dependency injection of the System Bus
    explicit M68k(IBus* bus);
    ~M68k() = default;

    // --------------------------------------------------------------------------
    // CPU Lifecycle Controls
    // --------------------------------------------------------------------------
    // Performs hardware reset. Reads initial SSP and PC from vector table
    void Reset();

    // Executes a single instruction, updates internal state, and returns 
    // the exact number of clock cycles consumed by the operation.
    int Step();

    // --------------------------------------------------------------------------
    // State Inspection (Getters for TDD / Debugging)
    // --------------------------------------------------------------------------
    Longword GetDRegister(int index) const { return m_d[index & 0x7]; }
    Longword GetARegister(int index) const { return m_a[index & 0x7]; }
    Address  GetPC() const { return m_pc; }
    Word     GetSR() const { return m_sr; }

    // Helper to inspect individual Condition Code Register (CCR) flags
    bool GetFlagCarry() const    { return (m_sr & 0x0001) != 0; }
    bool GetFlagOverflow() const { return (m_sr & 0x0002) != 0; }
    bool GetFlagZero() const     { return (m_sr & 0x0004) != 0; }
    bool GetFlagNegative() const { return (m_sr & 0x0008) != 0; }
    bool GetFlagExtend() const   { return (m_sr & 0x0010) != 0; }

    // --------------------------------------------------------------------------
    // State Modification (Setters for TDD unit test initialization)
    // --------------------------------------------------------------------------
    void SetDRegister(int index, Longword value) { m_d[index & 0x7] = value; }
    void SetARegister(int index, Longword value) { m_a[index & 0x7] = value; }
    void SetPC(Address address) { m_pc = address; }
    void SetSR(Word value) { m_sr = value; }

private:
    // Core Dependency
    IBus* m_bus;

    // --------------------------------------------------------------------------
    // M68k Internal Registers (Physical State)
    // --------------------------------------------------------------------------
    Longword m_d[8];  // Data Registers D0 - D7 (32-bit)
    Longword m_a[8];  // Address Registers A0 - A7 (32-bit). A7 is active SP.
    Address  m_pc;    // Program Counter (stores the address of next instruction)
    Word     m_sr;    // Status Register (System byte + User byte/CCR flags)

    // --------------------------------------------------------------------------
    // Helper Methods for Instruction Pipeline
    // --------------------------------------------------------------------------
    // Reads a 16-bit word from the current PC and increments PC by 2
    Word FetchCode();
};

} // namespace GenesisEmu::Core