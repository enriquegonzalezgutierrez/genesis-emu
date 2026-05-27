// ==============================================================================
// GenesisEmu - Cartridge Unit Tests (Corrected with DDD Namespaces)
// ==============================================================================
// This file contains unit tests to verify the Cartridge's ability to load raw
// ROM buffers, parse the Sega Rom Header metadata ($100-$1FF), and route reads.
// ==============================================================================

#include <gtest/gtest.h>
#include "Cartridge.h"

using namespace GenesisEmu::Core::Domain::Common;
using namespace GenesisEmu::Core::Domain::Cartridge;

// Helper to fill a simulated ROM buffer with a valid Sega Header structure
std::vector<Byte> CreateMockRom(const std::string& title, const std::string& serial) {
    // We allocate 1024 bytes to allow writing test opcodes safely beyond the 512-byte header
    std::vector<Byte> mockRom(1024, 0x00);

    // Write "SEGA MEGA DRIVE" to the System Type field ($100 - $10F)
    std::string system = "SEGA MEGA DRIVE ";
    for (std::size_t i = 0; i < 16; ++i) {
        mockRom[0x100 + i] = static_cast<Byte>(system[i]);
    }

    // Write Game Title to Domestic Title field ($120 - $14F)
    // Pad with spaces up to 48 bytes
    std::string paddedTitle = title;
    while (paddedTitle.length() < 48) paddedTitle += " ";
    for (std::size_t i = 0; i < 48; ++i) {
        mockRom[0x120 + i] = static_cast<Byte>(paddedTitle[i]);
    }

    // Write Serial Code to Serial field ($180 - $18D)
    // Pad with spaces up to 14 bytes
    std::string paddedSerial = serial;
    while (paddedSerial.length() < 14) paddedSerial += " ";
    for (std::size_t i = 0; i < 14; ++i) {
        mockRom[0x180 + i] = static_cast<Byte>(paddedSerial[i]);
    }

    return mockRom;
}

// ------------------------------------------------------------------------------
// Test Suite: CartridgeBehaviorTests
// ------------------------------------------------------------------------------

TEST(CartridgeBehaviorTests, CartridgeLoadsROMAndParsesHeader) {
    // 1. Arrange
    Cartridge cart;
    std::vector<Byte> mockRom = CreateMockRom("STREETS OF RAGE 2", "GM 00001043-00");

    // 2. Act
    bool success = cart.LoadROM(mockRom);

    // 3. Assert
    EXPECT_TRUE(success);
    EXPECT_EQ(cart.GetROMSize(), 1024);
    // Verify that the title and serial were correctly parsed and trimmed of excess spaces
    EXPECT_EQ(cart.GetGameTitle(), "STREETS OF RAGE 2");
    EXPECT_EQ(cart.GetSerialCode(), "GM 00001043-00");
}

TEST(CartridgeBehaviorTests, CartridgeRoutingAndMemoryBounds) {
    // 1. Arrange
    Cartridge cart;
    std::vector<Byte> mockRom = CreateMockRom("SONIC THE HEDGEHOG", "GM 00001009-00");
    
    // Write dummy opcode values at offset $200
    mockRom[0x200] = 0x4E;
    mockRom[0x201] = 0x71; // 0x4E71 (NOP)

    cart.LoadROM(mockRom);

    // 2. Act
    // Read Word from mapped Cartridge offset $200
    Word opcode = cart.ReadWord(0x000200);

    // 3. Assert
    // Big-Endian verification
    EXPECT_EQ(opcode, 0x4E71);
}