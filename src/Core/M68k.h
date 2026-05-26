// ==============================================================================
// GenesisEmu - Motorola 68000 CPU Domain Model Header (Updated)
// ==============================================================================
// Added TriggerInterrupt(int level) to support auto-vectored horizontal and 
// vertical blanking interrupts (Level 4 and Level 6 VBlank).
// ==============================================================================

#pragma once

#include "IBus.h"

namespace GenesisEmu::Core {

class M68k {
public:
    explicit M68k(IBus* bus);
    ~M68k() = default;

    // --- CPU Lifecycle Controls ---
    void Reset();
    int Step();

    // --- Interrupt Handling Core ---
    /**
     * @brief Injects an auto-vectored hardware interrupt into the CPU.
     * @param level The interrupt level (1 to 7). Level 6 is VBlank, Level 4 is HBlank.
     */
    void TriggerInterrupt(int level);

    // --- State Inspection ---
    Longword GetDRegister(int index) const { return m_d[index & 0x7]; }
    Longword GetARegister(int index) const { return m_a[index & 0x7]; }
    Address  GetPC() const { return m_pc; }
    Word     GetSR() const { return m_sr; }
    Longword GetUSP() const { return m_usp; } 
    bool     IsHalted() const { return m_halted; } 

    bool GetFlagCarry() const    { return (m_sr & 0x0001) != 0; }
    bool GetFlagOverflow() const { return (m_sr & 0x0002) != 0; }
    bool GetFlagZero() const     { return (m_sr & 0x0004) != 0; }
    bool GetFlagNegative() const { return (m_sr & 0x0008) != 0; }
    bool GetFlagExtend() const   { return (m_sr & 0x0010) != 0; }

    // --- State Modification ---
    void SetDRegister(int index, Longword value) { m_d[index & 0x7] = value; }
    void SetARegister(int index, Longword value) { m_a[index & 0x7] = value; }
    void SetPC(Address address) { m_pc = address; }
    void SetSR(Word value) { m_sr = value; }
    void SetUSP(Longword value) { m_usp = value; } 

private:
    IBus* m_bus;

    // CPU Registers
    Longword m_d[8];  
    Longword m_a[8];  
    Address  m_pc;    
    Word     m_sr;    
    Longword m_usp; 
    
    // Execution State flags
    bool     m_halted; 

    Word FetchCode();
};

} // namespace GenesisEmu::Core