// ==============================================================================
// GenesisEmu - Sega Genesis ROM Checksum & Header Integrity Verifier (Utility)
// ==============================================================================
// This standalone diagnostic tool opens and verifies standard Sega ROMs.
// It parses the system type, domestic title, serial fields, and computes the 
// standard 16-bit additive checksum from offset $200 to the end of the file.
//
// SOLID Compliance:
// 1. Single Responsibility Principle (SRP):
//    It is strictly responsible for ROM file integrity diagnostics. It is fully
//    decoupled from the CPU orchestrator and active system execution components.
// ==============================================================================

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstdint>
#include <algorithm>

/**
 * @brief Helper function to trim trailing space paddings from fixed header strings.
 */
static std::string TrimTrailingSpaces(const std::string& str) {
    auto end = std::find_if_not(str.rbegin(), str.rend(), [](unsigned char ch) {
        return std::isspace(ch);
    });
    return std::string(str.begin(), end.base());
}

int main() {
    std::string romPath = "roms/final_fight_md.bin";
    std::cout << "====================================================" << std::endl;
    std::cout << " GenesisEmu - ROM Checksum & Header Integrity Checker" << std::endl;
    std::cout << "====================================================" << std::endl;
    std::cout << "Reading File: " << romPath << "..." << std::endl;

    std::ifstream file(romPath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "[Error] Failed to open ROM: " << romPath << std::endl;
        std::cerr << "Ensure target ROM is located inside standard directories." << std::endl;
        return 1;
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<std::uint8_t> rom(size);
    if (!file.read(reinterpret_cast<char*>(rom.data()), size)) {
        std::cerr << "[Error] Failed to read ROM data." << std::endl;
        return 1;
    }

    std::cout << "ROM Size: " << size << " bytes (" << (size / 1024) << " KB)" << std::endl;

    if (size < 512) {
        std::cerr << "[Error] ROM size is too small to contain a Sega Header." << std::endl;
        return 1;
    }

    // 1. Read Vector Table Initializations
    std::uint32_t ssp = (rom[0] << 24) | (rom[1] << 16) | (rom[2] << 8) | rom[3];
    std::uint32_t pc  = (rom[4] << 24) | (rom[5] << 16) | (rom[6] << 8) | rom[7];
    
    std::cout << "\n--- Vector Table Check ---" << std::endl;
    std::cout << "Initial Stack Pointer (SSP): 0x" << std::hex << std::uppercase << ssp << std::endl;
    std::cout << "Initial Program Counter (PC): 0x" << pc << std::dec << std::endl;

    // 2. Read Sega Header Metadata ($000100 - $0001FF)
    std::string systemType(16, ' ');
    for (std::size_t i = 0; i < 16; ++i) systemType[i] = rom[0x100 + i];
    
    std::string gameTitle(48, ' ');
    for (std::size_t i = 0; i < 48; ++i) gameTitle[i] = rom[0x120 + i];

    std::string serialCode(14, ' ');
    for (std::size_t i = 0; i < 14; ++i) serialCode[i] = rom[0x180 + i];

    std::uint16_t expectedChecksum = (rom[0x18E] << 8) | rom[0x18F];

    std::cout << "\n--- Sega Header Metadata ---" << std::endl;
    std::cout << "System Type:     " << TrimTrailingSpaces(systemType) << std::endl;
    std::cout << "Game Title:      " << TrimTrailingSpaces(gameTitle) << std::endl;
    std::cout << "Serial Code:     " << TrimTrailingSpaces(serialCode) << std::endl;
    std::cout << "Header Checksum: 0x" << std::hex << std::uppercase << expectedChecksum << std::dec << std::endl;

    // 3. Calculate 16-Bit Word-Wise Additive Checksum
    std::uint16_t calculatedChecksum = 0;
    for (std::size_t i = 0x200; i < static_cast<std::size_t>(size); i += 2) {
        if (i + 1 < static_cast<std::size_t>(size)) {
            std::uint16_t word = (rom[i] << 8) | rom[i + 1];
            calculatedChecksum += word;
        }
    }

    std::cout << "\n--- Verification Results ---" << std::endl;
    std::cout << "Computed Checksum: 0x" << std::hex << std::uppercase << calculatedChecksum << std::dec << std::endl;

    if (calculatedChecksum == expectedChecksum) {
        std::cout << "\n[SUCCESS] ROM verified. Checksum matches." << std::endl;
    } else {
        std::cout << "\n[INFO] Checksum Mismatch detected (0x" << std::hex << std::uppercase << calculatedChecksum 
                  << " vs 0x" << expectedChecksum << std::dec << ")." << std::endl;
        std::cout << "Note: Mismatches are common in custom translations, hacks, or homebrews." << std::endl;
        std::cout << "The emulator's Cartridge Loader automatically patches this mismatch in memory" << std::endl;
        std::cout << "to safely bypass any initial copy protection freezes." << std::endl;
    }
    std::cout << "====================================================" << std::endl;

    return 0;
}