// ==============================================================================
// GenesisEmu - Real-Time System Frame Loop Entry Point (App Layer - Glow Loading)
// ==============================================================================
// This file initializes the motherboard bus, registers memories and ports, 
// and executes instructions inside a real-time cycle-sync frame loop.
// Upgraded with a beautiful, glowing retro Loading Bar during decompression phases.
// ==============================================================================

#include <iostream>
#include <array>
#include <chrono>
#include <thread>
#include "MainBus.h"
#include "M68k.h"
#include "Vdp.h"
#include "Cartridge.h"
#include "WorkRAM.h"
#include "IoPorts.h"
#include "SdlVideoAdapter.h"
#include "RomLoaderAdapter.h"
#include "VdpRenderer.h"
#include "M68kDecoder.h"       
#include "M68kInstruction.h"   

using namespace GenesisEmu::Core;
using namespace GenesisEmu::Adapters;

// Sega Genesis standard NTSC High-Resolution mode parameters
constexpr int SCREEN_WIDTH  = 320;
constexpr int SCREEN_HEIGHT = 224;
constexpr int WINDOW_SCALE = 4;

// Standard NTSC clock sync parameters
// 7.67 MHz CPU Clock => ~127,840 CPU clock cycles per 60Hz video frame
constexpr int CYCLES_PER_FRAME = 127840; 

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) {
    std::cout << "====================================================" << std::endl;
    std::cout << " GenesisEmu - Real-Time Core Console Engine          " << std::endl;
    std::cout << "====================================================" << std::endl;

    // 1. Load the ROM file
    std::string romPath = "roms/final_fight_md.bin";
    auto romData = RomLoaderAdapter::LoadFile(romPath);
    if (romData.empty()) {
        std::cerr << "[Fatal Error] Could not load ROM: " << romPath << std::endl;
        return 1;
    }

    // 2. Instantiate and load the game cartridge
    Cartridge cartridge;
    if (!cartridge.LoadROM(romData)) {
        std::cerr << "[Fatal Error] Invalid Sega ROM format." << std::endl;
        return 1;
    }

    std::cout << "[Cartridge] Loaded: " << cartridge.GetGameTitle() << std::endl;
    std::cout << "[Cartridge] Serial: " << cartridge.GetSerialCode() << std::endl;

    // 3. Initialize physical presentation window
    SdlVideoAdapter videoAdapter("GenesisEmu [Real-Time Game Mode]", SCREEN_WIDTH, SCREEN_HEIGHT, WINDOW_SCALE);
    if (!videoAdapter.Initialize()) {
        std::cerr << "[Fatal Error] Failed to initialize SDL Video Adapter." << std::endl;
        return 1;
    }

    // 4. Instantiate motherboard devices
    MainBus  bus;
    M68k     cpu(&bus);
    
    // Bind VDP with system bus pointer to enable DMA transfers
    Vdp      vdp(&bus);
    
    WorkRAM  wram;
    IoPorts  ioPorts;
    
    // Instantiate Sega Mapper register interface pointing to our cartridge
    SegaMapperDevice mapperDevice(&cartridge);

    // 5. Connect devices to MainBus (Memory Map Routing)
    bus.AttachDevice(&cartridge, 0x000000, 0x3FFFFF);
    bus.AttachDevice(&ioPorts, 0xA10000, 0xA1001F);
    
    // Attach the bank-switching register hardware to $A13000 - $A130FF
    bus.AttachDevice(&mapperDevice, 0xA13000, 0xA130FF);
    
    bus.AttachDevice(&vdp, 0xC00000, 0xC0001F);
    bus.AttachDevice(&wram, 0xE00000, 0xFFFFFF);

    // 6. Reset CPU
    cpu.Reset();
    std::cout << "[CPU] Reset complete. Stack Pointer (A7): 0x" 
              << std::hex << std::uppercase << cpu.GetARegister(7)
              << ", Program Counter (PC): 0x" << cpu.GetPC() << std::dec << std::endl;

    // Framebuffer representing the internal 320x224 emulated screen
    std::array<std::uint32_t, SCREEN_WIDTH * SCREEN_HEIGHT> screenBuffer;
    screenBuffer.fill(0x000000FF);

    bool running = true;

    // Telemetry tracking variables
    int frameCount = 0;
    int instructionsThisSecond = 0;
    auto lastDiagnosticTime = std::chrono::steady_clock::now();

    std::cout << "====================================================" << std::endl;
    std::cout << " Engine active! Play using Arrow Keys + Z / X / C. " << std::endl;
    std::cout << "====================================================" << std::endl;

    // --------------------------------------------------------------------------
    // Real-Time System Frame Loop
    // --------------------------------------------------------------------------
    while (running) {
        // Poll window close events and update gamepad states
        running = videoAdapter.ProcessEvents(ioPorts);

        // Execute instruction clock cycle budget ONLY if CPU is not halted
        if (!cpu.IsHalted()) {
            int currentFrameCycles = 0;
            while (currentFrameCycles < CYCLES_PER_FRAME) {
                int consumedCycles = cpu.Step();
                currentFrameCycles += consumedCycles;
                instructionsThisSecond++;
                
                // If a step triggered a diagnostic halt, exit the instruction loop immediately
                if (cpu.IsHalted()) {
                    break;
                }
            }
        }

        // Trigger Level 6 VBlank Interrupt (Vertical Blanking) per frame
        cpu.TriggerInterrupt(6);

        // Render current background planes
        for (int scanline = 0; scanline < SCREEN_HEIGHT; ++scanline) {
            std::uint32_t planeBLine[SCREEN_WIDTH] = {0};
            std::uint32_t planeALine[SCREEN_WIDTH] = {0};

            VdpRenderer::RenderPlaneScanline(vdp, 1, scanline, SCREEN_WIDTH, planeBLine);
            VdpRenderer::RenderPlaneScanline(vdp, 0, scanline, SCREEN_WIDTH, planeALine);

            for (int x = 0; x < SCREEN_WIDTH; ++x) {
                int pixelIndex = scanline * SCREEN_WIDTH + x;
                
                if (planeALine[x] != 0) {
                    screenBuffer[pixelIndex] = planeALine[x];
                } else if (planeBLine[x] != 0) {
                    screenBuffer[pixelIndex] = planeBLine[x];
                } else {
                    screenBuffer[pixelIndex] = 0x000000FF; 
                }
            }
        }

        // --- ADDED: Retro Loading Bar Overlay ---
        // If VRAM is mostly empty (less than 1000 bytes filled), draw an interactive 
        // progress bar representing the ROM-to-RAM decompression/copy progress.
        int nonZeroVramCount = 0;
        for (int i = 0; i < 0x10000; ++i) {
            if (vdp.ReadVramDirect(i) != 0) nonZeroVramCount++;
        }

        if (nonZeroVramCount < 1000) {
            // A1 register tracks copy progress up to the 4MB limit (0x3FFFFF)
            double progress = static_cast<double>(cpu.GetARegister(1) & 0x3FFFFF) / 4194304.0;
            if (progress < 0.0) progress = 0.0;
            if (progress > 1.0) progress = 1.0;

            int barX = 60;
            int barY = 110;
            int barWidth = 200;
            int barHeight = 8;
            int fillWidth = static_cast<int>(progress * barWidth);

            // Overlay the loading bar pixels directly on top of the black frame
            for (int y = barY - 2; y < barY + barHeight + 2; ++y) {
                for (int x = barX - 2; x < barX + barWidth + 2; ++x) {
                    int pixelIndex = y * SCREEN_WIDTH + x;
                    
                    if (y >= barY && y < barY + barHeight && x >= barX && x < barX + barWidth) {
                        if (x < barX + fillWidth) {
                            screenBuffer[pixelIndex] = 0x00F0FFFF; // Glowing Cyan fill
                        } else {
                            screenBuffer[pixelIndex] = 0x222222FF; // Dark Gray background
                        }
                    } else {
                        screenBuffer[pixelIndex] = 0x444444FF; // Outer Border outline
                    }
                }
            }
        }

        // Output to GPU window
        videoAdapter.RenderFrame(screenBuffer.data());
        frameCount++;

        // --- Real-Time Telemetry Monitor (Fires every 1000ms / 1s) ---
        auto currentTime = std::chrono::steady_clock::now();
        auto elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - lastDiagnosticTime).count();
        if (elapsedTime >= 1000) {
            int nonZeroVram = 0;
            for (int i = 0; i < 0x10000; ++i) {
                if (vdp.ReadVramDirect(i) != 0) nonZeroVram++;
            }
            
            int nonZeroCram = 0;
            for (int i = 0; i < 128; ++i) {
                if (vdp.ReadCramDirect(i) != 0) nonZeroCram++;
            }

            Word currentOpcode = bus.ReadWord(cpu.GetPC());
            DecodedInstruction currentInst = M68kDecoder::Decode(currentOpcode);
            
            std::string opName = "UNKNOWN";
            switch (currentInst.type) {
                case OpType::NOP: opName = "NOP"; break;
                case OpType::MOVE: opName = "MOVE"; break;
                case OpType::MOVE_TO_SR: opName = "MOVE_TO_SR"; break;
                case OpType::MOVE_USP: opName = "MOVE_USP"; break;
                case OpType::ADD: opName = "ADD"; break;
                case OpType::ADDQ: opName = "ADDQ"; break;
                case OpType::ADDX: opName = "ADDX"; break;
                case OpType::SUB: opName = "SUB"; break;
                case OpType::SUBQ: opName = "SUBQ"; break;
                case OpType::SUBX: opName = "SUBX"; break;
                case OpType::JMP: opName = "JMP"; break;
                case OpType::BRA: opName = "BRA"; break;
                case OpType::BCC: opName = "BCC"; break;
                case OpType::BCS: opName = "BCS"; break;
                case OpType::BEQ: opName = "BEQ"; break;
                case OpType::BGE: opName = "BGE"; break;
                case OpType::BGT: opName = "BGT"; break;
                case OpType::BHI: opName = "BHI"; break;
                case OpType::BLE: opName = "BLE"; break;
                case OpType::BLS: opName = "BLS"; break;
                case OpType::BLT: opName = "BLT"; break;
                case OpType::BMI: opName = "BMI"; break;
                case OpType::BNE: opName = "BNE"; break;
                case OpType::BPL: opName = "BPL"; break;
                case OpType::BVC: opName = "BVC"; break;
                case OpType::BVS: opName = "BVS"; break;
                case OpType::SCC: opName = "SCC"; break;
                case OpType::AND: opName = "AND"; break;
                case OpType::OR: opName = "OR"; break;
                case OpType::EOR: opName = "EOR"; break;
                case OpType::BSR: opName = "BSR"; break;
                case OpType::JSR: opName = "JSR"; break;
                case OpType::RTS: opName = "RTS"; break;
                case OpType::TST: opName = "TST"; break;
                case OpType::CMP: opName = "CMP"; break;
                case OpType::CMPI: opName = "CMPI"; break;
                case OpType::DBF: opName = "DBF"; break;
                case OpType::CLR: opName = "CLR"; break;
                case OpType::SWAP: opName = "SWAP"; break;
                case OpType::EXT: opName = "EXT"; break;
                case OpType::PEA: opName = "PEA"; break;
                case OpType::LEA: opName = "LEA"; break;
                case OpType::MOVEQ: opName = "MOVEQ"; break;
                case OpType::MOVEM: opName = "MOVEM"; break;
                case OpType::BTST: opName = "BTST"; break;
                case OpType::LSR: opName = "LSR"; break;
                case OpType::LSL: opName = "LSL"; break;
                case OpType::ASR: opName = "ASR"; break;
                case OpType::ASL: opName = "ASL"; break;
                case OpType::ROR: opName = "ROR"; break;
                case OpType::ROL: opName = "ROL"; break;
                default: opName = "UNKNOWN"; break;
            }

            std::cout << "\n--- [REAL-TIME ENGINE DIAGNOSTICS] ---" << std::endl;
            std::cout << "Presentation Speed:  " << frameCount << " FPS" << std::endl;
            std::cout << "Core Execution Speed:" << instructionsThisSecond << " Instructions/sec" << std::endl;
            std::cout << "CPU State:           PC=0x" << std::hex << std::uppercase << cpu.GetPC() 
                      << "  Opcode=0x" << currentOpcode << " (" << opName << ")"
                      << "  SP=0x" << cpu.GetARegister(7) << "  SR=0x" << cpu.GetSR() << std::dec << std::endl;
            std::cout << "CPU Registers:       D2=0x" << std::hex << cpu.GetDRegister(2) 
                      << "  A1=0x" << cpu.GetARegister(1) << "  A2=0x" << cpu.GetARegister(2) << std::dec << std::endl;
            std::cout << "VDP Register 2 (PlA):0x" << std::hex << (int)vdp.GetRegister(2) 
                      << " (Addr: 0x" << ((vdp.GetRegister(2) & 0x38) << 10) << ")" << std::dec << std::endl;
            std::cout << "VDP Register 4 (PlB):0x" << std::hex << (int)vdp.GetRegister(4) 
                      << " (Addr: 0x" << ((vdp.GetRegister(4) & 0x07) << 13) << ")" << std::dec << std::endl;
            std::cout << "VDP Register 15 (Inc):" << (int)vdp.GetRegister(15) << std::endl;
            std::cout << "VDP Target Address:  0x" << std::hex << vdp.GetTargetAddress() << std::dec << std::endl;
            std::cout << "VRAM Filled Bytes:   " << nonZeroVram << " / 65536 bytes" << std::endl;
            std::cout << "CRAM Active Colors:  " << (nonZeroCram / 2) << " / 64 colors" << std::endl;
            std::cout << "--------------------------------------\n" << std::endl;

            frameCount = 0;
            instructionsThisSecond = 0;
            lastDiagnosticTime = currentTime;
        }
    }

    std::cout << "System shutting down. Goodbye." << std::endl;
    return 0;
}