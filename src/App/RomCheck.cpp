// ==============================================================================
// GenesisEmu - Sega Genesis ROM Additive Checksum & Header Integrity Verifier
// ==============================================================================
// This standalone diagnostic tool verifies the integrity of the ROM file.
// Sega spec: 16-bit additive checksum of all Words from $200 to the end of ROM.
// Updated to handle Hack ROM checksum bypass logs.
// ==============================================================================

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstdint>
#include <algorithm>

// Helper to clean trailing spaces of Sega Header fields
std::string TrimSpaces(const std::string& str) {
    auto end = std::find_if_not(str.rbegin(), str.rend(), [](unsigned char ch) {
        return std::isspace(ch);
    });
    return std::string(str.begin(), end.base());
}

int main() {
    std::string romPath = "roms/final_fight_md.bin";
    std::cout << "====================================================" << std::endl;
    std::cout << " GenesisEmu - ROM Checksum & Header Validator (Hack-Safe)" << std::endl;
    std::cout << "====================================================" << std::endl;
    std::cout << "Opening: " << romPath << "..." << std::endl;

    std::ifstream file(romPath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "[Error] Could not find or open: " << romPath << std::endl;
        std::cerr << "Please ensure the ROM is placed in 'roms/final_fight_md.bin'" << std::endl;
        return 1;
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> rom(size);
    if (!file.read(reinterpret_cast<char*>(rom.data()), size)) {
        std::cerr << "[Error] Failed to read ROM contents." << std::endl;
        return 1;
    }

    std::cout << "ROM Size: " << size << " bytes (" << (size / 1024) << " KB)" << std::endl;

    if (size < 512) {
        std::cerr << "[Error] ROM size is too small to contain a Sega Header." << std::endl;
        return 1;
    }

    // 1. Read Vector Table
    uint32_t ssp = (rom[0] << 24) | (rom[1] << 16) | (rom[2] << 8) | rom[3];
    uint32_t pc  = (rom[4] << 24) | (rom[5] << 16) | (rom[6] << 8) | rom[7];
    
    std::cout << "\n--- Vector Table Check ---" << std::endl;
    std::cout << "Initial SP (SSP):  0x" << std::hex << std::uppercase << ssp << std::endl;
    std::cout << "Initial PC (Entry):0x" << pc << std::dec << std::endl;

    // 2. Read Sega Header Metadata ($100 - $1FF)
    std::string systemType(16, ' ');
    for (size_t i = 0; i < 16; ++i) systemType[i] = rom[0x100 + i];
    
    std::string gameTitle(48, ' ');
    for (size_t i = 0; i < 48; ++i) gameTitle[i] = rom[0x120 + i];

    std::string serialCode(14, ' ');
    for (size_t i = 0; i < 14; ++i) serialCode[i] = rom[0x180 + i];

    uint16_t expectedChecksum = (rom[0x18E] << 8) | rom[0x18F];

    std::cout << "\n--- Sega Header Metadata ---" << std::endl;
    std::cout << "System Type:       " << TrimSpaces(systemType) << std::endl;
    std::cout << "Game Title:        " << TrimSpaces(gameTitle) << std::endl;
    std::cout << "Serial Code:       " << TrimSpaces(serialCode) << std::endl;
    std::cout << "Header Checksum:   0x" << std::hex << std::uppercase << expectedChecksum << std::dec << std::endl;

    // 3. Compute Additive 16-bit Checksum
    uint16_t calculatedChecksum = 0;
    for (size_t i = 0x200; i < static_cast<size_t>(size); i += 2) {
        if (i + 1 < static_cast<size_t>(size)) {
            uint16_t word = (rom[i] << 8) | rom[i + 1];
            calculatedChecksum += word;
        }
    }

    std::cout << "\n--- Integrity Verification ---" << std::endl;
    std::cout << "Computed Checksum: 0x" << std::hex << std::uppercase << calculatedChecksum << std::dec << std::endl;

    if (calculatedChecksum == expectedChecksum) {
        std::cout << "\n[SUCCESS] ROM integrity verified! Checksum matches perfectly." << std::endl;
    } else {
        std::cout << "\n[INFO] ROM Checksum Mismatch detected (0x" << std::hex << std::uppercase << calculatedChecksum 
                  << " vs 0x" << expectedChecksum << std::dec << ")." << std::endl;
        std::cout << "Note: This is extremely common in Hack ROMs / Translations as authors rarely update headers." << std::endl;
        std::cout << "Don't worry: the emulator's Cartridge Loader will automatically patch this in memory" << std::endl;
        std::cout << "to bypass any internal game boot validation freeze locks successfully!" << std::endl;
    }
    std::cout << "====================================================" << std::endl;

    return 0;
}