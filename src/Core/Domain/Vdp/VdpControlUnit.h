// ==============================================================================
// GenesisEmu - VDP Control Unit Header (Core Domain)
// ==============================================================================
// This file declares the VdpControlUnit class. It processes 16-bit writes to
// the VdpCtrl port ($C00004), decoding them into register modifications or 
// 32-bit memory-mapped commands.
//
// SOLID Compliance:
// 1. Single Responsibility Principle (SRP):
//    It is solely responsible for VDP control command decoding and internal register
//    state updates. It contains no pixel-rendering or video memory storage.
// ==============================================================================

#pragma once

#include "../Common/IMemoryMappedDevice.h"
#include <array>

namespace GenesisEmu::Core::Domain::Vdp {

/**
 * @struct VdpCommand
 * @brief Value Object representing a decoded 32-bit VDP memory access request.
 */
struct VdpCommand {
    Common::Address targetAddress = 0;
    Common::Byte    code          = 0;
    bool            isValid       = false;
};

/**
 * @class VdpControlUnit
 * @brief Manages VDP configuration registers and the 32-bit command latch state machine.
 */
class VdpControlUnit {
public:
    VdpControlUnit();
    ~VdpControlUnit() = default;

    /**
     * @brief Processes a 16-bit write to the VDP Control Port.
     * @param data The 16-bit command word.
     * @return A Decoded VdpCommand if the 32-bit write cycle completes, 
     *         otherwise an invalid command packet.
     */
    VdpCommand WriteControl(Common::Word data);

    /**
     * @brief Resets the 32-bit command write latch state (triggered on Control Port read).
     */
    void ResetFlipFlop();

    // --- Register Access ---
    Common::Byte GetRegister(int index) const;
    void SetRegister(int index, Common::Byte value);

    // --- State Inspection ---
    Common::Address GetTargetAddress() const { return m_targetAddress; }
    Common::Byte GetControlCode() const { return m_controlCode; }
    void UpdateTargetAddress(Common::Address address) { m_targetAddress = address; }

private:
    std::array<Common::Byte, 24> m_registers; // Internal registers $00 to $17

    // 32-Bit Command Flip-Flop latches
    bool         m_writePending;
    Common::Word m_registerLatch;

    // Decoded memory targets
    Common::Address m_targetAddress;
    Common::Byte    m_controlCode;
};

} // namespace GenesisEmu::Core::Domain::Vdp