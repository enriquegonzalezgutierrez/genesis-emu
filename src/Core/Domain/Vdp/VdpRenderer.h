// ==============================================================================
// GenesisEmu - VDP Graphics Renderer Header (Core Domain)
// ==============================================================================
// This file declares the VdpRenderer class, providing static, stateless utilities
// to compile background planes and sprites directly from VDP internal memories.
//
// SOLID Compliance:
// 1. Single Responsibility Principle (SRP):
//    It is strictly responsible for graphics translation and scanline composition.
//    It holds no state, isolating rendering routines from VRAM storage structures.
// ==============================================================================

#pragma once

#include "Vdp.h"
#include <cstdint>

namespace GenesisEmu::Core::Domain::Vdp {

/**
 * @class VdpRenderer
 * @brief Stateless renderer utility performing real-time background composition.
 */
class VdpRenderer {
public:
    VdpRenderer() = delete; // Enforce pure static utility design
    ~VdpRenderer() = delete;

    /**
     * @brief Translates 9-bit Sega BGR formats to 32-bit RGBA color configurations.
     *        Sega Format: 0000 BBB0 GGG0 RRR0 (Scaled to 24-bit RGB + Alpha Opaque).
     * @param cramHigh High byte from Color RAM.
     * @param cramLow Low byte from Color RAM.
     * @return Output 32-bit RGBA color.
     */
    static std::uint32_t ConvertColor(Common::Byte cramHigh, Common::Byte cramLow);

    /**
     * @brief Rasterizes a single scanline for Scroll Plane A or Plane B.
     * @param vdp Reference to the parent VDP instance.
     * @param planeIndex 0 for Plane A, 1 for Plane B.
     * @param scanline Active raster row index (0-223).
     * @param screenWidth Width of target screen buffer (320).
     * @param lineBuffer Pointer to the target scanline memory in the framebuffer.
     */
    static void RenderPlaneScanline(const Vdp& vdp, int planeIndex, int scanline, 
                                    int screenWidth, std::uint32_t* lineBuffer);

    /**
     * @brief Rasterizes active sprites on the specified scanline (Priority compilation stub).
     * @param vdp Reference to the parent VDP instance.
     * @param scanline Active raster row index.
     * @param screenWidth Width of target screen buffer.
     * @param lineBuffer Pointer to the target scanline memory in the framebuffer.
     */
    static void RenderSpritesScanline(const Vdp& vdp, int scanline, 
                                      int screenWidth, std::uint32_t* lineBuffer);
};

} // namespace GenesisEmu::Core::Domain::Vdp