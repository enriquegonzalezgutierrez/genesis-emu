// ==============================================================================
// GenesisEmu - ROM Loader Adapter Header (Outer Hexagon)
// ==============================================================================
// This file implements the host filesystem loading adapter. It isolates direct
// std::ifstream actions from the system core.
//
// SOLID Compliance:
// 1. Single Responsibility Principle (SRP):
//    It is strictly responsible for accessing host file resources. It performs
//    no CPU initialization, parsing, or bus mapping operations.
// ==============================================================================

#pragma once

#include "../Core/Domain/Common/IMemoryMappedDevice.h" // For Byte type definition
#include <vector>
#include <string>
#include <fstream>
#include <iostream>

namespace GenesisEmu::Adapters {

/**
 * @class RomLoaderAdapter
 * @brief Host file system adapter designed to load flat binaries into memory blocks.
 */
class RomLoaderAdapter {
public:
    RomLoaderAdapter() = delete; // Enforce static adapter design
    ~RomLoaderAdapter() = delete;

    /**
     * @brief Opens and reads a raw file from disk in binary mode.
     * @param filePath Host file system path to the ROM file.
     * @return Raw byte array buffer containing file contents. Empty on failure.
     */
    static std::vector<Core::Domain::Common::Byte> LoadFile(const std::string& filePath) {
        // Open file in binary mode, initializing at the end (std::ios::ate)
        // to immediately fetch the exact file size.
        std::ifstream file(filePath, std::ios::binary | std::ios::ate);
        
        if (!file.is_open()) {
            std::cerr << "[RomLoader Error] Failed to open ROM: " << filePath << std::endl;
            return {};
        }

        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg); // Rewind pointer to read from offset 0

        std::vector<Core::Domain::Common::Byte> buffer(static_cast<std::size_t>(size));

        if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
            std::cerr << "[RomLoader Error] Failed to read ROM buffer: " << filePath << std::endl;
            return {};
        }

        // Auto-detect and decode interleaved .smd files
        // SMD files have a 512-byte header and are interleaved in 16KB blocks
        if ((size % 16384) == 512) {
            std::cout << "[RomLoader] Detected interleaved SMD format. Decoding..." << std::endl;
            std::vector<Core::Domain::Common::Byte> decoded(size - 512);
            
            std::size_t blocks = (size - 512) / 16384;
            for (std::size_t b = 0; b < blocks; ++b) {
                std::size_t srcBlock = 512 + (b * 16384);
                std::size_t dstBlock = b * 16384;
                
                // SMD interleave pattern:
                // First 8KB contains all EVEN bytes of the 16KB block
                // Second 8KB contains all ODD bytes of the 16KB block
                for (std::size_t i = 0; i < 8192; ++i) {
                    decoded[dstBlock + (i * 2)]     = buffer[srcBlock + i];        // Even bytes (high byte of word)
                    decoded[dstBlock + (i * 2) + 1] = buffer[srcBlock + 8192 + i]; // Odd bytes (low byte of word)
                }
            }
            buffer = std::move(decoded);
            size = buffer.size();
        }

        std::cout << "[RomLoader] Loaded ROM: " << filePath 
                  << " (" << (size / 1024) << " KB)" << std::endl;
                  
        return buffer;
    }
};

} // namespace GenesisEmu::Adapters