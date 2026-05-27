// ==============================================================================
// GenesisEmu - VDP Aggregate Root Header (Core Domain)
// ==============================================================================
// This file declares the primary Vdp entity. It encapsulates VRAM, CRAM, and VSRAM
// memories, coordinating DMA block copy requests via the main system bus.
//
// SOLID Compliance:
// 1. Single Responsibility Principle (SRP):
//    It is solely responsible for emulating VDP data access, managing memories,
//    and executing hardware-level DMAs. It delegates render logic to the Renderer.
// 2. Dependency Inversion Principle (DIP):
//    It references the motherboard bus via the abstract Common::IBus interface.
// ==============================================================================

#pragma once

#include "../Common/IMemoryMappedDevice.h"
#include "../Common/IBus.h"
#include "VdpControlUnit.h"
#include <array>

namespace GenesisEmu::Core::Domain::Vdp {

/**
 * @class Vdp
 * @brief Aggregate Root representing the Video Display Processor.
 */
class Vdp : public Common::IMemoryMappedDevice {
public:
    explicit Vdp(Common::IBus* bus = nullptr);
    ~Vdp() override = default;

    // --- IMemoryMappedDevice Interface Overrides ---
    Common::Byte ReadByte(Common::Address offset) override;
    Common::Word ReadWord(Common::Address offset) override;
    void WriteByte(Common::Address offset, Common::Byte data) override;
    void WriteWord(Common::Address offset, Common::Word data) override;

    // --- Domain and Telemetry Inspection Getters ---
    Common::Byte GetRegister(int index) const { return m_controlUnit.GetRegister(index); }
    Common::Address GetTargetAddress() const { return m_controlUnit.GetTargetAddress(); }
    
    // --- Host Timing Synchronizations ---
    /**
     * @brief Updates the VBlank status flag based on active motherboard timing.
     * @param active True if the console is currently inside the VBlank period.
     */
    void SetVblankActive(bool active) { 
        // Trigger the hardware Interrupt Pending flag on the rising edge of VBlank
        if (active && !m_vblankActive) {
            m_vblankPending = true;
        }
        m_vblankActive = active; 
    }

    /**
     * @brief Updates the current frame cycle counter to emulate the HV Beam Counter.
     * @param cycles Number of CPU cycles executed during the current frame.
     */
    void SetFrameCycles(int cycles) { m_frameCycles = cycles; }

    // Direct memory viewers to allow the decoupled renderer to pull layers
    Common::Byte ReadVramDirect(Common::Address addr) const { return m_vram[addr & 0xFFFF]; }
    Common::Byte ReadCramDirect(Common::Address addr) const { return m_cram[addr & 0x7F]; } 

private:
    // Virtual encapsulated VDP memory spaces
    std::array<Common::Byte, 0x10000> m_vram;   // 64 KB Video RAM (Tiles, Nametables)
    std::array<Common::Byte, 128>     m_cram;   // 128 Bytes Color RAM (Palettes)
    std::array<Common::Byte, 80>      m_vsram;  // 80 Bytes Vertical Scroll RAM

    // Encapsulated Control register unit
    VdpControlUnit m_controlUnit;

    // Pointer to system bus to perform DMA copies from system ROM/RAM
    Common::IBus*  m_bus;

    // Current Vertical Blanking state (updated in real-time by the motherboard)
    bool m_vblankActive;
    
    // Pending VBlank Interrupt (Bit 7 of Status Register). Must be cleared upon read.
    bool m_vblankPending;

    // Accumulator of CPU cycles executed within the active frame
    int m_frameCycles;

    // --- Private Data Access and DMA Operations ---
    void WriteDataPort(Common::Word data);
    Common::Word ReadDataPort();

    /**
     * @brief Performs high-speed hardware-level block copies into VRAM or CRAM.
     */
    void ExecuteDMA();
};

} // namespace GenesisEmu::Core::Domain::Vdp