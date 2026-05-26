// ==============================================================================
// GenesisEmu - VDP Control Unit (Core Domain)
// ==============================================================================
// This file handles the VDP Control Port state machine ($C00004), managing 
// internal registers ($00 to $17) and processing the 32-bit Command Flip-Flop.
//
// DESIGN STRATEGY:
// 1. SRP: Isolates Command/Register parsing from VRAM read/write pipelines.
// 2. Anti-God File: Keeps Vdp.cpp strictly focused on high-level memory 
//    routing and frame timing.
// ==============================================================================

#pragma once

#include "IMemoryMappedDevice.h"
#include <array>

namespace GenesisEmu::Core {

/**
 * @struct VdpCommand
 * @brief Decoded 32-bit VDP command structure.
 */
struct VdpCommand {
    Address targetAddress = 0;
    Byte    code          = 0;
    bool    isValid       = false;
};

/**
 * @class VdpControlUnit
 * @brief Manages internal register files and processes 16-bit to 32-bit 
 *        control port write operations.
 */
class VdpControlUnit {
public:
    VdpControlUnit();
    ~VdpControlUnit() = default;

    /**
     * @brief Writes a 16-bit word to the Control Port.
     * @param data The 16-bit control command.
     * @return A Decoded VdpCommand if a 32-bit write cycle completes, 
     *         otherwise an invalid command.
     */
    VdpCommand WriteControl(Word data);

    /**
     * @brief Resets the flip-flop state (triggered by reading control port).
     */
    void ResetFlipFlop();

    // --- Register Access ---
    Byte GetRegister(int index) const;
    void SetRegister(int index, Byte value);

    // --- State Inspection ---
    Address GetTargetAddress() const { return m_targetAddress; }
    Byte GetControlCode() const { return m_controlCode; }
    void UpdateTargetAddress(Address address) { m_targetAddress = address; }

private:
    std::array<Byte, 24> m_registers; // Internal Registers $00 to $17

    // Flip-Flop state tracking
    bool m_writePending;
    Word m_registerLatch;

    // Decoded target parameters
    Address m_targetAddress;
    Byte    m_controlCode;
};

} // namespace GenesisEmu::Core