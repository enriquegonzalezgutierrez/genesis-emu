// ==============================================================================
// GenesisEmu - I/O Front Controller Ports Implementation (Core Domain)
// ==============================================================================
// This file implements the state calculation of the DE-9 controller ports.
// ==============================================================================

#include "IoPorts.h"

namespace GenesisEmu::Core::Domain::Io {

using namespace GenesisEmu::Core::Domain::Common;

IoPorts::IoPorts()
    : m_portAData(0x7F)
    , m_portBData(0x7F) 
    , m_portACtrl(0x00)
    , m_portBCtrl(0x00) 
    , m_buttonState(0xFFFF) // 0xFFFF indicates all buttons are unpressed (Active-LOW)
{}

// ------------------------------------------------------------------------------
// IMemoryMappedDevice Interface Overrides (Read Operations)
// ------------------------------------------------------------------------------
Byte IoPorts::ReadByte(Address offset) {
    switch (offset) {
        case 0x01: 
            // Sega Version/Region register ($A10001)
            // Bit 7: US/Japan (1 = Export US, 0 = Domestic Japan)
            // Bit 6: PAL/NTSC (1 = PAL 50Hz, 0 = NTSC 60Hz)
            // Return 0x80 representing a standard Export US NTSC Genesis console.
            return 0x80;

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
        case 0x03:
            // Write to Port A Data: sets output lines (such as SELECT / Bit 6)
            // Only bits configured as outputs in the DDR (m_portACtrl) are written.
            m_portAData = (m_portAData & ~m_portACtrl) | (data & m_portACtrl);
            break;
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
// Sega Multiplexed Data Generator
// ------------------------------------------------------------------------------
Byte IoPorts::GetMultiplexedDataA() const {
    // SELECT line status is determined by Bit 6 of Port A Data (if configured as output)
    bool selectLineHigh = (m_portAData & 0x40) != 0;

    // Retrieve active-low states of individual buttons (true = released, false = pressed)
    bool up    = (m_buttonState & (1 << 0)) != 0;
    bool down  = (m_buttonState & (1 << 1)) != 0;
    bool left  = (m_buttonState & (1 << 2)) != 0;
    bool right = (m_buttonState & (1 << 3)) != 0;
    bool b     = (m_buttonState & (1 << 4)) != 0;
    bool c     = (m_buttonState & (1 << 5)) != 0;
    bool a     = (m_buttonState & (1 << 6)) != 0;
    bool start = (m_buttonState & (1 << 7)) != 0;

    Byte result = 0;

    if (selectLineHigh) {
        // SELECT HIGH: Read Up, Down, Left, Right, B, C
        // Bit Layout: [0, SelectState(1), C, B, Right, Left, Down, Up]
        result = 0x40; // Set Select State bit to 1
        if (c)     result |= 0x20;
        if (b)     result |= 0x10;
        if (right) result |= 0x08;
        if (left)  result |= 0x04;
        if (down)  result |= 0x02;
        if (up)    result |= 0x01;
    } 
    else {
        // SELECT LOW: Read Up, Down, A, Start
        // Bit Layout: [0, SelectState(0), Start, A, 0, 0, Down, Up]
        result = 0x00; 
        if (start) result |= 0x20;
        if (a)     result |= 0x10;
        if (down)  result |= 0x02;
        if (up)    result |= 0x01;
        // Bits 3 and 2 are always pulled low (0) in 3-button signature mode
    }

    // Mask with DDR to protect output pins from being overridden by inputs
    return (result & ~m_portACtrl) | (m_portAData & m_portACtrl);
}

// ------------------------------------------------------------------------------
// Outer Hexagon Bridge
// ------------------------------------------------------------------------------
void IoPorts::SetButtonState(GamepadButton button, bool pressed) {
    int bitIndex = 0;
    switch (button) {
        case GamepadButton::UP:    bitIndex = 0; break;
        case GamepadButton::DOWN:  bitIndex = 1; break;
        case GamepadButton::LEFT:  bitIndex = 2; break;
        case GamepadButton::RIGHT: bitIndex = 3; break;
        case GamepadButton::B:     bitIndex = 4; break;
        case GamepadButton::C:     bitIndex = 5; break;
        case GamepadButton::A:     bitIndex = 6; break;
        case GamepadButton::START: bitIndex = 7; break;
    }

    if (pressed) {
        m_buttonState &= ~(1 << bitIndex); // Held down => Ground line to 0 (Active-LOW)
    } else {
        m_buttonState |= (1 << bitIndex);  // Released => Float back to 1
    }
}

} // namespace GenesisEmu::Core::Domain::Io