// ==============================================================================
// GenesisEmu - ROM Loader Adapter Header-Only (Outer Hexagon)
// ==============================================================================
// This utility class implements the file system loader. It opens a flat binary
// ROM file (.bin/.md) from the host disk and reads its bytes into a buffer.
// ==============================================================================

#pragma once

#include "IMemoryMappedDevice.h" // For Byte definition
#include <vector>
#include <string>
#include <fstream>
#include <iostream>

namespace GenesisEmu::Adapters {

class RomLoaderAdapter {
public:
    // --------------------------------------------------------------------------
    // File Loading Engine
    // --------------------------------------------------------------------------
    // Opens the file, reads it in binary mode, and returns a raw Byte vector.
    // Returns an empty vector if the file cannot be opened.
    // --------------------------------------------------------------------------
    static std::vector<Core::Byte> LoadFile(const std::string& filePath) {
        // Open file in binary mode, starting at the very end of the file (std::ios::ate)
        // to determine the file size instantly.
        std::ifstream file(filePath, std::ios::binary | std::ios::ate);
        
        if (!file.is_open()) {
            std::cerr << "[RomLoader Error] Failed to open ROM file: " << filePath << std::endl;
            return {};
        }

        // Get file size from the end pointer position, then rewind to the beginning
        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);

        // Allocate a vector of the exact size
        std::vector<Core::Byte> buffer(static_cast<size_t>(size));

        // Read the binary contents directly into the vector memory block
        if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
            std::cerr << "[RomLoader Error] Failed to read ROM data: " << filePath << std::endl;
            return {};
        }

        std::cout << "[RomLoader] Successfully loaded ROM: " << filePath 
                  << " (" << (size / 1024) << " KB)" << std::endl;
                  
        return buffer;
    }
};

} // namespace GenesisEmu::Adapters