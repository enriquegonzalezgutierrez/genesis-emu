// ==============================================================================
// GenesisEmu - M68k Registers Entity (Core Domain)
// ==============================================================================
// This class encapsulates the raw physical register files of the Motorola
// 68000 CPU. It abstracts register reads, writes, and status flag masks.
//
// SOLID Compliance:
// 1. Single Responsibility Principle (SRP):
//    It is solely responsible for register storage and status flag formatting.
//    It contains no logic for memory access, decoding, or instruction execution.
// ==============================================================================

#pragma once

#include <array>
#include <cstdint>

namespace GenesisEmu::Core::Domain::M68k {

// --- Architectural Type Aliases ---
using Byte     = std::uint8_t;
using Word     = std::uint16_t;
using Longword = std::uint32_t;
using Address  = std::uint32_t;

/**
 * @class M68kRegisters
 * @brief Domain Entity representing the internal registers and status flags of the M68k.
 */
class M68kRegisters {
public:
    M68kRegisters()
        : m_pc(0)
        , m_sr(0x2700)
        , m_usp(0)
    {
        m_d.fill(0);
        m_a.fill(0);
    }

    ~M68kRegisters() = default;

    // --- Data and Address Register Interfaces ---
    
    inline Longword ReadD(int index) const { return m_d[index & 0x7]; }
    inline void WriteD(int index, Longword value) { m_d[index & 0x7] = value; }

    inline Longword ReadA(int index) const { return m_a[index & 0x7]; }
    inline void WriteA(int index, Longword value) { m_a[index & 0x7] = value; }

    // --- Control Register Interfaces ---
    
    inline Address GetPC() const { return m_pc; }
    inline void SetPC(Address address) { m_pc = address; }

    inline Word GetSR() const { return m_sr; }
    inline void SetSR(Word value) { m_sr = value; }

    inline Longword GetUSP() const { return m_usp; }
    inline void SetUSP(Longword value) { m_usp = value; }

    // --- Status Register (SR) Flag Accessors ---
    
    // Condition Code Register (CCR) status bit masks:
    // C = Carry (bit 0)
    // V = Overflow (bit 1)
    // Z = Zero (bit 2)
    // N = Negative (bit 3)
    // X = Extend (bit 4)

    inline bool GetCarry() const    { return (m_sr & 0x0001) != 0; }
    inline bool GetOverflow() const { return (m_sr & 0x0002) != 0; }
    inline bool GetZero() const     { return (m_sr & 0x0004) != 0; }
    inline bool GetNegative() const { return (m_sr & 0x0008) != 0; }
    inline bool GetExtend() const   { return (m_sr & 0x0010) != 0; }

    // --- Flag Modification Helpers ---
    
    inline void SetCarry(bool value) {
        if (value) m_sr |= 0x0001;
        else m_sr &= ~0x0001;
    }

    inline void SetOverflow(bool value) {
        if (value) m_sr |= 0x0002;
        else m_sr &= ~0x0002;
    }

    inline void SetZero(bool value) {
        if (value) m_sr |= 0x0004;
        else m_sr &= ~0x0004;
    }

    inline void SetNegative(bool value) {
        if (value) m_sr |= 0x0008;
        else m_sr &= ~0x0008;
    }

    inline void SetExtend(bool value) {
        if (value) m_sr |= 0x0010;
        else m_sr &= ~0x0010;
    }

    inline void ClearCCR() {
        m_sr &= ~0x001F;
    }

private:
    std::array<Longword, 8> m_d;  // Data Registers D0-D7
    std::array<Longword, 8> m_a;  // Address Registers A0-A7 (A7 represents Active Stack Pointer)
    Address                 m_pc; // Program Counter
    Word                    m_sr; // Status Register (System byte + User byte CCR)
    Longword                m_usp;// User Stack Pointer (active in User mode)
};

} // namespace GenesisEmu::Core::Domain::M68k