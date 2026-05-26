// ==============================================================================
// GenesisEmu - I/O Front Controller Ports (Core Domain)
// ==============================================================================
// This file emulates the DE-9 controller ports mapping to memory addresses
// $A10000 to $A1001F, resolving multiplexed gamepad reads.
//
// DESIGN STRATEGY:
// 1. SRP: Decouples host keyboard polling from emulator bus register states.
// 2. DDD: Encapsulates Sega's multiplexed controller lines (Select Pin toggle).
// ==============================================================================

#pragma once

#include "IMemoryMappedDevice.h"

namespace GenesisEmu::Core {

/**
 * @enum GamepadButton
 * @brief Sega Genesis standard controller buttons.
 */
enum class GamepadButton {
    UP,
    DOWN,
    LEFT,
    RIGHT,
    A,
    B,
    C,
    START
};

/**
 * @class IoPorts
 * @brief Manages data and control registers for DE-9 front ports.
 *        Implements IMemoryMappedDevice to hook directly to MainBus at $A10000.
 */
class IoPorts : public IMemoryMappedDevice {
public:
    IoPorts();
    ~IoPorts() override = default;

    // --- IMemoryMappedDevice Interface Overrides ---
    // Mapped offsets: 0x03 (Port A Data), 0x05 (Port B Data), 0x09 (Port A Ctrl), 0x0B (Port B Ctrl)
    Byte ReadByte(Address offset) override;
    Word ReadWord(Address offset) override;
    void WriteByte(Address offset, Byte data) override;
    void WriteWord(Address offset, Word data) override;

    // --------------------------------------------------------------------------
    // Host Input Bridge (Outer Hexagon API)
    // --------------------------------------------------------------------------
    /**
     * @brief Updates the physical state of a controller button.
     * @param button Target Sega button.
     * @param pressed True if held down, false if released.
     */
    void SetButtonState(GamepadButton button, bool pressed);

private:
    // Raw register states
    Byte m_portAData;
    Byte m_portBData;
    Byte m_portACtrl;
    Byte m_portBCtrl;

    // Bit-field representing physical button presses (0 = Pressed/LOW, 1 = Released/HIGH)
    // Sega original hardware lines pull low on contact closures (buttons held).
    std::uint16_t m_buttonState; 

    // Helper to compile read register bytes based on current SELECT line states
    Byte GetMultiplexedDataA() const;
};

} // namespace GenesisEmu::Core