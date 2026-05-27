// ==============================================================================
// GenesisEmu - Application Bootstrapper and Sync Loop (Application Layer)
// ==============================================================================
// This file initializes the virtual console motherboard, connects physical
// peripherals to the MainBus, and runs the cycle-sync execution frame loop.
// ==============================================================================

#include <iostream>
#include <array>
#include <chrono>
#include <thread>
#include "../Core/Domain/Bus/MainBus.h"
#include "../Core/Domain/M68k/M68k.h"
#include "../Core/Domain/Vdp/Vdp.h"
#include "../Core/Domain/Cartridge/Cartridge.h"
#include "../Core/Domain/Cartridge/SegaMapperDevice.h" 
#include "../Core/Domain/WorkRAM/WorkRAM.h"
#include "../Core/Domain/Io/IoPorts.h"
#include "../Core/Domain/Vdp/VdpRenderer.h"
#include "../Core/Domain/M68k/M68kDecoder.h"       
#include "../Core/Domain/M68k/M68kInstruction.h"   
#include "../Adapters/SdlVideoAdapter.h"
#include "../Adapters/RomLoaderAdapter.h"

using namespace GenesisEmu::Core::Domain::Common; 
using namespace GenesisEmu::Core::Domain::Bus;
using namespace GenesisEmu::Core::Domain::M68k;
using namespace GenesisEmu::Core::Domain::Vdp;
using namespace GenesisEmu::Core::Domain::Cartridge;
using namespace GenesisEmu::Core::Domain::WorkRAM;
using namespace GenesisEmu::Core::Domain::Io;
using namespace GenesisEmu::Adapters;

constexpr int SCREEN_WIDTH  = 320;
constexpr int SCREEN_HEIGHT = 224;
constexpr int WINDOW_SCALE  = 4;

// 7.67 MHz CPU NTSC clock sync parameters: ~127,840 CPU cycles per 60Hz frame.
// VBlank typically triggers at scanline 224 (approx 93% of the frame execution).
constexpr int CYCLES_PER_FRAME = 127840; 
constexpr int VBLANK_TRIGGER_CYCLE = 118000;

int main(int argc, char* argv[]) {
    std::cout << "====================================================" << std::endl;
    std::cout << " GenesisEmu - Real-Time Core Console Engine          " << std::endl;
    std::cout << "====================================================" << std::endl;

    std::string romPath = "roms/sonic.bin"; 
    if (argc > 1) {
        romPath = argv[1];
    }

    auto romData = RomLoaderAdapter::LoadFile(romPath);
    if (romData.empty()) {
        std::cerr << "[Fatal Error] ROM loader returned an empty buffer: " << romPath << std::endl;
        return 1;
    }

    Cartridge cartridge;
    if (!cartridge.LoadROM(romData)) {
        std::cerr << "[Fatal Error] ROM header parsing failed. Invalid Sega format." << std::endl;
        return 1;
    }

    std::cout << "[Cartridge] Title:  " << cartridge.GetGameTitle() << std::endl;
    std::cout << "[Cartridge] Serial: " << cartridge.GetSerialCode() << std::endl;

    SdlVideoAdapter videoAdapter("GenesisEmu [Real-Time Game Mode]", SCREEN_WIDTH, SCREEN_HEIGHT, WINDOW_SCALE);
    if (!videoAdapter.Initialize()) {
        std::cerr << "[Fatal Error] Failed to initialize SDL Video Adapter." << std::endl;
        return 1;
    }

    MainBus  bus;
    M68k     cpu(&bus);
    Vdp      vdp(&bus); 
    WorkRAM  wram;
    IoPorts  ioPorts;
    
    SegaMapperDevice mapperDevice(&cartridge);

    bus.AttachDevice(&cartridge,    0x000000, 0x3FFFFF);
    bus.AttachDevice(&ioPorts,      0xA10000, 0xA1001F);
    bus.AttachDevice(&mapperDevice, 0xA13000, 0xA130FF); 
    bus.AttachDevice(&vdp,          0xC00000, 0xC0001F);
    bus.AttachDevice(&wram,         0xE00000, 0xFFFFFF);

    cpu.Reset();
    std::cout << "[CPU] Reset executed. SP: 0x" << std::hex << std::uppercase << cpu.GetARegister(7)
              << " | PC: 0x" << cpu.GetPC() << std::dec << std::endl;

    std::array<std::uint32_t, SCREEN_WIDTH * SCREEN_HEIGHT> screenBuffer;
    screenBuffer.fill(0x000000FF); 

    bool running = true;
    int frameCount = 0;
    int instructionsThisSecond = 0;
    auto lastDiagnosticTime = std::chrono::steady_clock::now();

    std::cout << "====================================================" << std::endl;
    std::cout << " Motherboard synchronizations active. Controls: Arrow Keys + Z/X/C" << std::endl;
    std::cout << "====================================================" << std::endl;

    while (running) {
        running = videoAdapter.ProcessEvents(ioPorts);

        int currentFrameCycles = 0;
        bool vblankTriggeredThisFrame = false;

        // Execute instructions up to the NTSC frame clock cycle budget
        while (currentFrameCycles < CYCLES_PER_FRAME) {
            
            if (cpu.IsHalted() && !vblankTriggeredThisFrame) {
                currentFrameCycles = VBLANK_TRIGGER_CYCLE;
            }

            // --- Synchronize VDP VBlank status and HV Counter with active frame cycles ---
            bool isVblankPhase = (currentFrameCycles >= VBLANK_TRIGGER_CYCLE);
            vdp.SetVblankActive(isVblankPhase);
            vdp.SetFrameCycles(currentFrameCycles);

            // Trigger VBlank interrupt only if enabled in VDP Register 1 (bit 5 / IE0)
            // as specified in page 13 of the Sega Genesis Software Manual.
            bool vblankEnabled = (vdp.GetRegister(1) & 0x20) != 0;
            if (vblankEnabled && isVblankPhase && !vblankTriggeredThisFrame) {
                cpu.TriggerInterrupt(6);
                vblankTriggeredThisFrame = true;
            } else if (!vblankEnabled && isVblankPhase) {
                // Ensure we mark the cycle check finished even if disabled to avoid infinite checks
                vblankTriggeredThisFrame = true;
            }

            int consumedCycles = cpu.Step();
            currentFrameCycles += consumedCycles;
            instructionsThisSecond++;
        }

        // Fetch active backdrop background color index from VDP Register 7
        Byte bgIndex = vdp.GetRegister(7) & 0x3F;
        Byte colorHigh = vdp.ReadCramDirect(bgIndex * 2);
        Byte colorLow  = vdp.ReadCramDirect(bgIndex * 2 + 1);
        std::uint32_t backdropColor = VdpRenderer::ConvertColor(colorHigh, colorLow);

        // Render visual layers for this frame sychronously
        for (int scanline = 0; scanline < SCREEN_HEIGHT; ++scanline) {
            std::uint32_t planeBLine[SCREEN_WIDTH] = {0};
            std::uint32_t planeALine[SCREEN_WIDTH] = {0};
            std::uint32_t spriteLine[SCREEN_WIDTH] = {0};

            VdpRenderer::RenderPlaneScanline(vdp, 1, scanline, SCREEN_WIDTH, planeBLine);
            VdpRenderer::RenderPlaneScanline(vdp, 0, scanline, SCREEN_WIDTH, planeALine);
            VdpRenderer::RenderSpritesScanline(vdp, scanline, SCREEN_WIDTH, spriteLine);

            for (int x = 0; x < SCREEN_WIDTH; ++x) {
                int pixelIndex = scanline * SCREEN_WIDTH + x;
                
                // Composite Layer Priority: Sprites > Plane A > Plane B > Backdrop Color
                if (spriteLine[x] != 0) {
                    screenBuffer[pixelIndex] = spriteLine[x];
                } else if (planeALine[x] != 0) {
                    screenBuffer[pixelIndex] = planeALine[x];
                } else if (planeBLine[x] != 0) {
                    screenBuffer[pixelIndex] = planeBLine[x];
                } else {
                    screenBuffer[pixelIndex] = backdropColor; 
                }
            }
        }

        videoAdapter.RenderFrame(screenBuffer.data());
        frameCount++;

        // --- Real-Time Telemetry Monitor (Fires once every 1000ms) ---
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
                case OpType::MOVE_FROM_SR: opName = "MOVE_FROM_SR"; break;
                case OpType::MOVE_TO_CCR: opName = "MOVE_TO_CCR"; break;
                case OpType::MOVE_USP: opName = "MOVE_USP"; break;
                case OpType::ADD: opName = "ADD"; break;
                case OpType::ADDQ: opName = "ADDQ"; break;
                case OpType::ADDX: opName = "ADDX"; break;
                case OpType::SUB: opName = "SUB"; break;
                case OpType::SUBQ: opName = "SUBQ"; break;
                case OpType::SUBX: opName = "SUBX"; break;
                case OpType::NEG: opName = "NEG"; break;
                case OpType::NEGX: opName = "NEGX"; break;
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
                case OpType::RTE: opName = "RTE"; break;
                case OpType::TST: opName = "TST"; break;
                case OpType::CMP: opName = "CMP"; break;
                case OpType::CMPI: opName = "CMPI"; break;
                case OpType::DBCC: opName = "DBcc"; break;
                case OpType::DBF: opName = "DBF"; break;
                case OpType::CLR: opName = "CLR"; break;
                case OpType::SWAP: opName = "SWAP"; break;
                case OpType::EXT: opName = "EXT"; break;
                case OpType::PEA: opName = "PEA"; break;
                case OpType::LEA: opName = "LEA"; break;
                case OpType::MOVEQ: opName = "MOVEQ"; break;
                case OpType::MOVEM: opName = "MOVEM"; break;
                case OpType::BTST: opName = "BTST"; break;
                case OpType::BCHG: opName = "BCHG"; break;
                case OpType::BCLR: opName = "BCLR"; break;
                case OpType::BSET: opName = "BSET"; break;
                case OpType::LSR: opName = "LSR"; break;
                case OpType::LSL: opName = "LSL"; break;
                case OpType::ASR: opName = "ASR"; break;
                case OpType::ASL: opName = "ASL"; break;
                case OpType::ROR: opName = "ROR"; break;
                case OpType::ROL: opName = "ROL"; break;
                case OpType::ROXR: opName = "ROXR"; break;
                case OpType::ROXL: opName = "ROXL"; break;
                default: opName = "UNKNOWN"; break;
            }

            std::cout << "\n--- [REAL-TIME ENGINE DIAGNOSTICS] ---" << std::endl;
            std::cout << "Presentation Speed:  " << frameCount << " FPS" << std::endl;
            std::cout << "Core Execution Speed:" << instructionsThisSecond << " Instructions/sec" << std::endl;
            std::cout << "CPU State:           PC=0x" << std::hex << std::uppercase << cpu.GetPC() 
                      << "  Opcode=0x" << currentOpcode << " (" << opName << ")"
                      << "  SP=0x" << cpu.GetARegister(7) << "  SR=0x" << cpu.GetSR() << std::dec << std::endl;
            std::cout << "CPU Registers:       D1=0x" << std::hex << cpu.GetDRegister(1)
                      << "  D2=0x" << cpu.GetDRegister(2)
                      << "  A0=0x" << cpu.GetARegister(0)
                      << "  A1=0x" << cpu.GetARegister(1) << std::dec << std::endl;
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