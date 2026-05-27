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

    // Video status vertical blank toggle simulator
    bool m_vblankToggle;

    // --- Private Data Access and DMA Operations ---
    void WriteDataPort(Common::Word data);
    Common::Word ReadDataPort();

    /**
     * @brief Performs high-speed hardware-level block copies into VRAM or CRAM.
     */
    void ExecuteDMA();
};

} // namespace GenesisEmu::Core::Domain::Vdp