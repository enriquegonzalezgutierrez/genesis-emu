// ==============================================================================
// GenesisEmu - I/O Front Controller Ports Header (Core Domain)
// ==============================================================================
// This file declares the IoPorts class. It emulates front controller register
// logic and manages 6-button multiplexed reads based on the TH (Select) pin strobing.
//
// SOLID Compliance:
// 1. Single Responsibility Principle (SRP):
//    It is solely responsible for emulating physical port registers and
//    mapping host gamepad interactions to the simulated active-low state lines.
// ==============================================================================

#pragma once

#include "../Common/IMemoryMappedDevice.h"

namespace GenesisEmu::Core::Domain::Io {

/**
 * @enum GamepadButton
 * @brief Identifiers for standard Sega Genesis 6-button controller keys.
 */
enum class GamepadButton {
    UP,
    DOWN,
    LEFT,
    RIGHT,
    A,
    B,
    C,
    START,
    X,
    Y,
    Z,
    MODE
};

/**
 * @class IoPorts
 * @brief Memory-mapped device emulating the physical controller DE-9 ports.
 */
class IoPorts : public Common::IMemoryMappedDevice {
public:
    IoPorts();
    ~IoPorts() override = default;

    // --- IMemoryMappedDevice Interface Overrides ---
    Common::Byte ReadByte(Common::Address offset) override;
    Common::Word ReadWord(Common::Address offset) override;
    void WriteByte(Common::Address offset, Common::Byte data) override;
    void WriteWord(Common::Address offset, Common::Word data) override;

    // --- Host Input Bridge (Outer Hexagon API) ---

    /**
     * @brief Injects the state of a physical controller key from the host.
     * @param button Target Sega button.
     * @param pressed True if held, false if released.
     */
    void SetButtonState(GamepadButton button, bool pressed);

    /**
     * @brief Steps the internal 1.5ms multiplexer reset timeout.
     * @param cpuCycles Consumed CPU cycles to subtract from the timeout.
     */
    void UpdateTimers(int cpuCycles);

private:
    // Raw register states
    Common::Byte m_portAData;
    Common::Byte m_portBData;
    Common::Byte m_portACtrl; // Data Direction Register (DDR) for Port A
    Common::Byte m_portBCtrl; // Data Direction Register (DDR) for Port B

    // Active-LOW bitfield containing current button states (1 = Released, 0 = Pressed)
    // Supports up to 16 buttons. Default state is 0xFFFF (all released).
    std::uint16_t m_buttonState; 

    // 6-Button Controller Strobe States
    int m_strobes;         // Counts TH line transitions (0 to 3)
    int m_timeoutCycles;   // CPU cycle countdown (~1.5ms) to reset strobes

    /**
     * @brief Computes Port A's multiplexed data byte depending on the SELECT line 
     *        and the current 6-button strobe phase.
     */
    Common::Byte GetMultiplexedDataA() const;

    /**
     * @brief Helper to query the active-low state of a specific button.
     */
    bool IsReleased(GamepadButton button) const;
};

} // namespace GenesisEmu::Core::Domain::Io