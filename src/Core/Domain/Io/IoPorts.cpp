// ==============================================================================
// GenesisEmu - I/O Front Controller Ports Implementation (Core Domain)
// ==============================================================================
// This file implements the state calculation of the DE-9 controller ports,
// accurately modeling the 6-button pad's 4-phase strobe sequence and timeout.
// ==============================================================================

#include "IoPorts.h"

namespace GenesisEmu::Core::Domain::Io {

using namespace GenesisEmu::Core::Domain::Common;

// A standard 1.5ms timeout at 7.67 MHz translates to roughly 11,500 clock cycles
constexpr int STROBE_TIMEOUT_CYCLES = 11500;

IoPorts::IoPorts()
    : m_portAData(0x7F)
    , m_portBData(0x7F) 
    , m_portACtrl(0x00)
    , m_portBCtrl(0x00) 
    , m_buttonState(0xFFFF) // 0xFFFF indicates all buttons are unpressed (Active-LOW)
    , m_strobes(0)
    , m_timeoutCycles(0)
{}

void IoPorts::UpdateTimers(int cpuCycles) {
    if (m_timeoutCycles > 0) {
        m_timeoutCycles -= cpuCycles;
        if (m_timeoutCycles <= 0) {
            // Hardware timeout reached: Game stopped strobing the TH pin.
            // Reset the internal 6-button phase counter back to 0.
            m_timeoutCycles = 0;
            m_strobes = 0;
        }
    }
}

// ------------------------------------------------------------------------------
// IMemoryMappedDevice Interface Overrides (Read Operations)
// ------------------------------------------------------------------------------
Byte IoPorts::ReadByte(Address offset) {
    switch (offset) {
        case 0x01: 
            // Sega Version/Region register ($A10001)
            // Bit 7: US/Japan (1 = Export US, 0 = Domestic Japan)
            // Bit 6: PAL/NTSC (1 = PAL 50Hz, 0 = NTSC 60Hz)
            // Bit 5: Sega CD attached (1 = No CD, 0 = CD attached)
            // Return 0xA0 representing a standard Export US NTSC Genesis console with no Sega CD.
            return 0xA0;

        case 0x03: return GetMultiplexedDataA();
        case 0x05: return m_portBData; // Port B Data stub (Player 2)
        case 0x09: return m_portACtrl; // Port A Control (Data Direction Register)
        case 0x0B: return m_portBCtrl; // Port B Control
        default:   return 0xFF;
    }
}

Word IoPorts::ReadWord(Address offset) {
    // Word reads combine sequential ports into a single 16-bit container
    return (static_cast<Word>(ReadByte(offset)) << 8) | ReadByte(offset + 1);
}

// ------------------------------------------------------------------------------
// IMemoryMappedDevice Interface Overrides (Write Operations)
// ------------------------------------------------------------------------------
void IoPorts::WriteByte(Address offset, Byte data) {
    switch (offset) {
        case 0x03: {
            // Check for a Rising Edge on the TH (Select) pin (Bit 6)
            bool oldTH = (m_portAData & 0x40) != 0;
            bool newTH = (data & 0x40) != 0;

            if (!oldTH && newTH) {
                // Rising Edge detected: increment strobe counter (mod 4)
                m_strobes = (m_strobes + 1) % 4;
                m_timeoutCycles = STROBE_TIMEOUT_CYCLES; // Reset the 1.5ms timeout
            }

            // Write to Port A Data: sets output lines
            // Only bits configured as outputs in the DDR (m_portACtrl) are affected.
            m_portAData = (m_portAData & ~m_portACtrl) | (data & m_portACtrl);
            break;
        }
        case 0x05:
            m_portBData = (m_portBData & ~m_portBCtrl) | (data & m_portBCtrl);
            break;
        case 0x09:
            m_portACtrl = data; // Set Port A Direction Mask
            break;
        case 0x0B:
            m_portBCtrl = data; // Set Port B Direction Mask
            break;
        default:
            break;
    }
}

void IoPorts::WriteWord(Address offset, Word data) {
    WriteByte(offset, static_cast<Byte>(data >> 8));
    WriteByte(offset + 1, static_cast<Byte>(data & 0xFF));
}

// ------------------------------------------------------------------------------
// 6-Button Multiplexed Data Generator
// ------------------------------------------------------------------------------
bool IoPorts::IsReleased(GamepadButton button) const {
    int bitIndex = static_cast<int>(button);
    return (m_buttonState & (1 << bitIndex)) != 0; // Active-LOW: 1 means released
}

Byte IoPorts::GetMultiplexedDataA() const {
    // TH line (Select) status is determined by Bit 6 of Port A Data
    bool selectLineHigh = (m_portAData & 0x40) != 0;
    Byte result = 0;

    if (selectLineHigh) {
        // TH = 1 (HIGH)
        if (m_strobes == 3) {
            // Phase 3 (Extra 6-button data): [0, 1(TH), C, B, MODE, X, Y, Z]
            result |= 0x40; // TH is HIGH
            if (IsReleased(GamepadButton::C))    result |= 0x20;
            if (IsReleased(GamepadButton::B))    result |= 0x10;
            if (IsReleased(GamepadButton::MODE)) result |= 0x08;
            if (IsReleased(GamepadButton::X))    result |= 0x04;
            if (IsReleased(GamepadButton::Y))    result |= 0x02;
            if (IsReleased(GamepadButton::Z))    result |= 0x01;
        } else {
            // Phases 0, 1, 2: [0, 1(TH), C, B, Right, Left, Down, Up]
            result |= 0x40; // TH is HIGH
            if (IsReleased(GamepadButton::C))     result |= 0x20;
            if (IsReleased(GamepadButton::B))     result |= 0x10;
            if (IsReleased(GamepadButton::RIGHT)) result |= 0x08;
            if (IsReleased(GamepadButton::LEFT))  result |= 0x04;
            if (IsReleased(GamepadButton::DOWN))  result |= 0x02;
            if (IsReleased(GamepadButton::UP))    result |= 0x01;
        }
    } 
    else {
        // TH = 0 (LOW)
        if (m_strobes == 2) {
            // Phase 2 (Controller signature): [0, 0(TH), Start, A, 0, 0, 0, 0]
            if (IsReleased(GamepadButton::START)) result |= 0x20;
            if (IsReleased(GamepadButton::A))     result |= 0x10;
        } 
        else if (m_strobes == 3) {
            // Phase 3 (Controller signature 2): [0, 0(TH), Start, A, 1, 1, 1, 1]
            if (IsReleased(GamepadButton::START)) result |= 0x20;
            if (IsReleased(GamepadButton::A))     result |= 0x10;
            result |= 0x0F; // Lower bits are hardcoded to 1
        } 
        else {
            // Phases 0, 1: [0, 0(TH), Start, A, 0, 0, Down, Up]
            if (IsReleased(GamepadButton::START)) result |= 0x20;
            if (IsReleased(GamepadButton::A))     result |= 0x10;
            if (IsReleased(GamepadButton::DOWN))  result |= 0x02;
            if (IsReleased(GamepadButton::UP))    result |= 0x01;
        }
    }

    // Mask with DDR to protect output pins from being overridden by our input logic
    return (result & ~m_portACtrl) | (m_portAData & m_portACtrl);
}

// ------------------------------------------------------------------------------
// Outer Hexagon Bridge
// ------------------------------------------------------------------------------
void IoPorts::SetButtonState(GamepadButton button, bool pressed) {
    int bitIndex = static_cast<int>(button);
    if (pressed) {
        m_buttonState &= ~(1 << bitIndex); // Held down => Ground line to 0 (Active-LOW)
    } else {
        m_buttonState |= (1 << bitIndex);  // Released => Float back to 1
    }
}

} // namespace GenesisEmu::Core::Domain::Io