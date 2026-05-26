// ==============================================================================
// GenesisEmu - Bus Unit Tests (TDD)
// ==============================================================================
// This file contains unit tests to verify the routing logic of our future
// MainBus implementation before the concrete code is written.
// ==============================================================================

#include <gtest/gtest.h>
#include "IMemoryMappedDevice.h"
#include "MainBus.h" // Note: This header will be created in the next step

using namespace GenesisEmu::Core;

// ------------------------------------------------------------------------------
// Mock Memory Device for Testing
// ------------------------------------------------------------------------------
// A simple spy/mock device that records write operations and returns a pre-set
// value on read operations. Used to isolate Bus testing from real components.
class MockMemoryDevice : public IMemoryMappedDevice {
public:
    Address lastWriteOffset = 0xFFFFFFFF;
    Byte lastWriteByteData = 0x00;
    Word lastWriteWordData = 0x0000;
    Byte presetReadByte = 0x00;
    Word presetReadWord = 0x0000;

    Byte ReadByte(Address offset) override {
        return presetReadByte;
    }

    Word ReadWord(Address offset) override {
        return presetReadWord;
    }

    void WriteByte(Address offset, Byte data) override {
        lastWriteOffset = offset;
        lastWriteByteData = data;
    }

    void WriteWord(Address offset, Word data) override {
        lastWriteOffset = offset;
        lastWriteWordData = data;
    }
};

// ------------------------------------------------------------------------------
// Test Suite: MainBusRoutingTests
// ------------------------------------------------------------------------------

TEST(MainBusRoutingTests, AttachAndRouteWriteByte) {
    // 1. Arrange
    MainBus bus;
    MockMemoryDevice mockRam;
    
    // Attach RAM to the Work RAM region ($E00000 to $FFFFFF)
    bus.AttachDevice(&mockRam, 0xE00000, 0xFFFFFF);

    // 2. Act
    // Write value 0x55 to address $E00005. 
    // The relative offset inside the RAM device should be calculated as 0x000005.
    bus.WriteByte(0xE00005, 0x55);

    // 3. Assert
    EXPECT_EQ(mockRam.lastWriteOffset, 0x000005);
    EXPECT_EQ(mockRam.lastWriteByteData, 0x55);
}

TEST(MainBusRoutingTests, AttachAndRouteReadByte) {
    // 1. Arrange
    MainBus bus;
    MockMemoryDevice mockRom;
    mockRom.presetReadByte = 0xAB;
    
    // Attach ROM to Cartridge region ($000000 to $3FFFFF)
    bus.AttachDevice(&mockRom, 0x000000, 0x3FFFFF);

    // 2. Act
    Byte data = bus.ReadByte(0x000100);

    // 3. Assert
    EXPECT_EQ(data, 0xAB);
}

TEST(MainBusRoutingTests, UnmappedAddressReturnsDefault) {
    // 1. Arrange
    MainBus bus;
    // No devices are attached to the bus.

    // 2. Act & Assert
    // Accessing an unmapped address must not crash the emulator.
    // In real hardware, this acts as an "open bus", typically returning 0xFF (or 0x00).
    EXPECT_NO_THROW({
        Byte data = bus.ReadByte(0x123456);
        EXPECT_EQ(data, 0xFF); 
    });
}