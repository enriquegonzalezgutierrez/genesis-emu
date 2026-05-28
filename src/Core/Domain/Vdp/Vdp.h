// ==============================================================================
// GenesisEmu - VDP Aggregate Root Header (Core Domain)
// ==============================================================================
// This file declares the primary Vdp entity. It encapsulates VRAM, CRAM, and VSRAM
// memories, coordinating line-by-line rendering and cycle-accurate DMA block copies.
//
// SOLID Compliance:
// 1. Single Responsibility Principle (SRP):
//    It is solely responsible for emulating VDP state, memory mappings, and
//    coordinating cycle-stealing DMAs. Rendering is delegated to VdpRenderer.
// 2. Dependency Inversion Principle (DIP):
//    It accesses the motherboard bus via the abstract Common::IBus interface.
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
    
    // --- Host Timing & Scanline Synchronization ---
    
    /**
     * @brief Renders a single horizontal scanline to the host framebuffer.
     * @param scanline The current Y-coordinate of the CRT beam (0 to 223).
     * @param frameBuffer Pointer to the start of the 320x224 32-bit screen buffer.
     */
    void RenderScanline(int scanline, std::uint32_t* frameBuffer);

    /**
     * @brief Decrements the Horizontal Interrupt (H-Int) counter.
     * @return True if the counter underflowed and the H-Int (Level 4) is enabled.
     */
    bool DecrementHintCounter();

    /**
     * @brief Updates the VBlank status flag based on active motherboard timing.
     * @param active True if the console is currently inside the VBlank period.
     */
    void SetVblankActive(bool active);

    /**
     * @brief Updates the current frame cycle counter to emulate the HV Beam Counter.
     */
    void SetFrameCycles(int cycles) { m_frameCycles = cycles; }

    // --- Cycle-Accurate DMA (Direct Memory Access) Engine ---
    
    /**
     * @brief Checks if a hardware DMA transfer is currently holding the bus.
     */
    bool IsDmaActive() const { return m_dmaActive; }

    /**
     * @brief Executes pending DMA transfers, consuming the allotted CPU cycle budget.
     * @param cycleBudget The amount of M68k cycles available to spend on copying.
     * @return The exact number of CPU cycles consumed by the DMA transfer.
     */
    int ProcessDma(int cycleBudget);

    // Direct memory viewers to allow the decoupled renderer to pull layers
    Common::Byte ReadVramDirect(Common::Address addr) const { return m_vram[addr & 0xFFFF]; }
    Common::Byte ReadCramDirect(Common::Address addr) const { return m_cram[addr & 0x7F]; } 
    Common::Byte ReadVsramDirect(Common::Address addr) const { return m_vsram[addr % 80]; }

private:
    // Virtual encapsulated VDP memory spaces
    std::array<Common::Byte, 0x10000> m_vram;   // 64 KB Video RAM (Tiles, Nametables)
    std::array<Common::Byte, 128>     m_cram;   // 128 Bytes Color RAM (Palettes)
    std::array<Common::Byte, 80>      m_vsram;  // 80 Bytes Vertical Scroll RAM

    // Encapsulated Control register unit
    VdpControlUnit m_controlUnit;

    // Pointer to system bus to perform DMA copies from system ROM/RAM
    Common::IBus*  m_bus;

    // Interrupt and Timing States
    bool m_vblankActive;
    bool m_vblankPending; // VIP (Vertical Interrupt Pending) flag
    int  m_frameCycles;
    int  m_hintCounter;   // Horizontal Interrupt countdown

    // DMA Execution States
    bool            m_dmaActive;
    bool            m_dmaFillPending;
    Common::Word    m_dmaLength;
    Common::Address m_dmaSourceAddress;
    Common::Word    m_dmaFillData;

    // --- Private Helpers ---
    void WriteDataPort(Common::Word data);
    Common::Word ReadDataPort();
    void ArmDmaTransfer();
};

} // namespace GenesisEmu::Core::Domain::Vdp