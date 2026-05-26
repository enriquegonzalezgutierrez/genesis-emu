// ==============================================================================
// GenesisEmu - I/O Front Controller Ports Implementation (Core Domain)
// ==============================================================================
// This file implements the state calculation of the DE-9 controller ports,
// converting button presses into multiplexed data based on the Select pin.
// ==============================================================================

#include "IoPorts.h"

namespace GenesisEmu::Core {

IoPorts::IoPorts()
    : m_portAData(0x7F), m_portBData(0x7F), 
      m_portACtrl(0x00), m_portBCtrl(0x00), 
      m_buttonState(0xFFFF) {
    // 0xFFFF indicates all buttons are unpressed (Sega registers are Active-LOW)
}

// ------------------------------------------------------------------------------
// IMemoryMappedDevice Interface Overrides
// ------------------------------------------------------------------------------
Byte IoPorts::ReadByte(Address offset) {
    switch (offset) {
        case 0x03: return GetMultiplexedDataA();
        case 0x05: return m_portBData; // Port B stub (player 2)
        case 0x09: return m_portACtrl;
        case 0x0B: return m_portBCtrl;
        default:   return 0xFF;
    }
}

Word IoPorts::ReadWord(Address offset) {
    // Standard M68k read: word reads simply duplicate or combine ports
    return (static_cast<Word>(ReadByte(offset)) << 8) | ReadByte(offset + 1);
}

void IoPorts::WriteByte(Address offset, Byte data) {
    switch (offset) {
        case 0x03:
            // Write to Port A Data. This is used by the game to set the SELECT pin.
            // Only bits marked as output in Control Register (m_portACtrl) are written.
            m_portAData = (m_portAData & ~m_portACtrl) | (data & m_portACtrl);
            break;
        case 0x05:
            m_portBData = (m_portBData & ~m_portBCtrl) | (data & m_portBCtrl);
            break;
        case 0x09:
            m_portACtrl = data;
            break;
        case 0x0B:
            m_portBCtrl = data;
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
    // Select Line status is determined by Bit 6 of Port A Data (latched during write)
    bool selectLineHigh = (m_portAData & 0x40) != 0;

    // Retrieve active-low states of buttons
    bool up    = (m_buttonState & (1 << 0)) != 0;
    bool down  = (m_buttonState & (1 << 1)) != 0;
    bool left  = (m_buttonState & (1 << 2)) != 0;
    bool right = (m_buttonState & (1 << 3)) != 0;
    bool b     = (m_buttonState & (1 << 4)) != 0;
    bool c     = (m_buttonState & (1 << 5)) != 0;
    bool a     = (m_buttonState & (1 << 6)) != 0;
    bool start = (m_buttonState & (1 << 7)) != 0;

    Byte result = 0;

    // Reconstruct Sega multiplexed active-low Byte register
    if (selectLineHigh) {
        // SELECT HIGH: Output Up, Down, Left, Right, B, C
        // Layout: [Bit 7: Constant 0, Bit 6: Select (1), Bit 5: C, Bit 4: B, Bit 3: R, Bit 2: L, Bit 1: D, Bit 0: U]
        result = 0x40; // Set Select Bit
        if (c)     result |= 0x20;
        if (b)     result |= 0x10;
        if (right) result |= 0x08;
        if (left)  result |= 0x04;
        if (down)  result |= 0x02;
        if (up)    result |= 0x01;
    } 
    else {
        // SELECT LOW: Output Up, Down, A, Start
        // Layout: [Bit 7: Constant 0, Bit 6: Select (0), Bit 5: Start, Bit 4: A, Bit 3: 0, Bit 2: 0, Bit 1: D, Bit 0: U]
        result = 0x00; 
        if (start) result |= 0x20;
        if (a)     result |= 0x10;
        if (down)  result |= 0x02;
        if (up)    result |= 0x01;
        // Bits 3 and 2 are held at 0 for standard controller signatures
    }

    // Mask with current data direction to preserve values written to output bits
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
        m_buttonState &= ~(1 << bitIndex); // Held down => Pull line LOW (0)
    } else {
        m_buttonState |= (1 << bitIndex);  // Released => Float back HIGH (1)
    }
}

} // namespace GenesisEmu::Core