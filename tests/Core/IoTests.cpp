// ==============================================================================
// GenesisEmu - I/O Ports Unit Tests (Corrected with DDD Namespaces)
// ==============================================================================
// This file contains unit tests to verify the multiplexed read/write states
// of the front controller DE-9 ports.
// ==============================================================================

#include <gtest/gtest.h>
#include "IoPorts.h"

using namespace GenesisEmu::Core::Domain::Common;
using namespace GenesisEmu::Core::Domain::Io;

// ------------------------------------------------------------------------------
// Test Cases: IoPorts Unit
// ------------------------------------------------------------------------------

TEST(IoTests, DefaultRegisterStatesArePulledHigh) {
    IoPorts io;

    // By default, Sega ports float to active-LOW 1 (Released/HIGH)
    // Writing nothing should return 0x7F (bits 6-0 are set to 1, bit 7 is 0)
    // Control registers default to 0x00 (inputs)
    EXPECT_EQ(io.ReadByte(0x03), 0x7F);
    EXPECT_EQ(io.ReadByte(0x09), 0x00);
}

TEST(IoTests, MultiplexedReadsWithSelectHigh) {
    IoPorts io;

    // Configure Port A Control Register to allow SELECT line (bit 6) output writing
    // Register 0x09: Port A Control. Set bit 6 to 1 (output mode)
    io.WriteByte(0x09, 0x40);

    // Set SELECT pin to HIGH (bit 6 of Data register)
    io.WriteByte(0x03, 0x40);

    // Press UP, DOWN, and C on the host
    io.SetButtonState(GamepadButton::UP, true);
    io.SetButtonState(GamepadButton::DOWN, true);
    io.SetButtonState(GamepadButton::C, true);

    // Read Port A Data ($A10003)
    Byte data = io.ReadByte(0x03);

    // When SELECT is HIGH:
    // Bits Up (0), Down (1), and C (5) are pressed (LOW/0)
    // Other bits remain unpressed (HIGH/1)
    // Bit 6 is Select state (1)
    // Expect: 0x40 (Select) | 0x10 (B high) | 0x08 (R high) | 0x04 (L high) => 0x5C
    EXPECT_EQ(data, 0x5C);
}

TEST(IoTests, MultiplexedReadsWithSelectLow) {
    IoPorts io;

    // Configure Port A Control Register to allow SELECT (bit 6) output writing
    io.WriteByte(0x09, 0x40);

    // Set SELECT pin to LOW (bit 6 of Data register is 0)
    io.WriteByte(0x03, 0x00);

    // Press UP, DOWN, A, and START
    io.SetButtonState(GamepadButton::UP, true);
    io.SetButtonState(GamepadButton::DOWN, true);
    io.SetButtonState(GamepadButton::A, true);
    io.SetButtonState(GamepadButton::START, true);

    Byte data = io.ReadByte(0x03);

    // When SELECT is LOW:
    // Bits Up (0), Down (1), A (4), and Start (5) are pressed (LOW/0)
    // Other bits remain unpressed (HIGH/1)
    // Bit 6 is Select state (0)
    // Expect: All pressed buttons pull lines to 0. Unpressed lines stay 1.
    // Bits 3 and 2 are always 0.
    // Result should be 0x00 (all monitored pins are pressed/grounded)
    EXPECT_EQ(data, 0x00);
}